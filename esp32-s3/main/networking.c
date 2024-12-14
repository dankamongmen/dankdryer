#include "dryer-network.h"
#include "networking.h"
#include <mdns.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <lwip/netif.h>
#include <nimble/ble.h>
#include <mqtt_client.h>
#include <host/ble_hs.h>
#include <host/ble_gatt.h>
#include <host/ble_hs_id.h>
#include <esp_nimble_hci.h>
#include <esp_netif_sntp.h>
#include <esp_http_server.h>
#include <host/ble_hs_mbuf.h>
#include <nimble/nimble_port.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>
#include <nimble/nimble_port_freertos.h>

// 693c9ea2cb764ac38e6bb05d0bb57845 -- setup service
static const ble_uuid128_t setup_svc_uuid =
    BLE_UUID128_INIT(0x69, 0x3c, 0x9e, 0xa2, 0xcb, 0x76, 0x4a, 0xc3, 0x8e, 0x6b, 0xb0, 0x5d, 0x0b, 0xb5, 0x78, 0x45);

// c3868812-e56b-4059-8c0c-54695d0baf6d -- essid (read, write in setup state 0)
static const ble_uuid128_t essid_chr_uuid =
    BLE_UUID128_INIT(0xc3, 0x86, 0x88, 0x12, 0xe5, 0x6b, 0x40, 0x59, 0x8c, 0x0c, 0x54, 0x69, 0x5d, 0x0b, 0xaf, 0x6d);

static const ble_uuid128_t psk_chr_uuid =
    BLE_UUID128_INIT(0x06, 0x2e, 0xf3, 0x54, 0xfe, 0xa5, 0x42, 0x47, 0x98, 0xc9, 0x9f, 0x9d, 0xa1, 0xee, 0x4c, 0x2e);

static const ble_uuid128_t setup_state_chr_uuid =
    BLE_UUID128_INIT(0xa1, 0x54, 0xe0, 0x6e, 0xb5, 0x62, 0x40, 0x2f, 0x90, 0x57, 0xd2, 0x59, 0x01, 0x38, 0x97, 0xe2);

#define CCHAN "control/"
#define MOTOR_CHANNEL CCHAN DEVICE "/motor"
#define HEATER_CHANNEL CCHAN DEVICE "/heater"
#define LPWM_CHANNEL CCHAN DEVICE "/lpwm"
#define UPWM_CHANNEL CCHAN DEVICE "/upwm"
#define DRY_CHANNEL CCHAN DEVICE "/dry"
#define TARE_CHANNEL CCHAN DEVICE "/tare"
#define CALIBRATE_CHANNEL CCHAN DEVICE "/calibrate"
#define FACTORYRESET_CHANNEL CCHAN DEVICE "/factoryreset"
#define CLIENTID DEVICE VERSION

static httpd_handle_t HTTPServ;
static uint8_t WifiEssid[33];
static uint8_t WifiPSK[65];
static esp_mqtt_client_handle_t MQTTHandle;

static enum {
  SETUP_STATE_NEEDWIFI,
  SETUP_STATE_WIFIATTEMPT,
  SETUP_STATE_CONFIGURED
} SetupState = SETUP_STATE_NEEDWIFI;

// only relevant in SETUP_STATE_WIFIATTEMPT/SETUP_STATE_CONFIGURED
static enum {
  WIFI_INVALID,
  WIFI_CONNECTING,
  NET_CONNECTING,
  MQTT_CONNECTING,
  MQTT_ESTABLISHED,
  NETWORK_STATE_COUNT
} NetworkState = WIFI_INVALID;

static const char*
state_str(void){
  return SetupState == SETUP_STATE_NEEDWIFI ? "Waiting for configuration" :
          SetupState == SETUP_STATE_WIFIATTEMPT ? "Connecting to WiFi" :
          SetupState == SETUP_STATE_CONFIGURED ? "Configured" : "Unknown state";
}

static inline const char*
bool_as_onoff_http(bool b){
  return b ? "<font color=\"green\">on</font>" : "off";
}

static inline const char*
motor_state_http(void){
  return bool_as_onoff_http(get_motor_state());
}

static inline const char*
heater_state_http(void){
  return bool_as_onoff_http(get_heater_state());
}

static void
set_network_state(int state){
  // FIXME lock
  NetworkState = state;
  if(state != WIFI_INVALID){ // if invalid, leave any initial failure status up
    if(state < NETWORK_STATE_COUNT){
      //set_led(&NetworkIndications[state]);
    }
  }
}

static const esp_mqtt_client_config_t MQTTConfig = {
  .broker = {
    .address = {
      .uri = MQTTURI,
    },
  },
  .credentials = {
    .username = MQTTUSER,
    // FIXME set client id, but stir in some per-device data
    .authentication = {
      .password = MQTTPASS,
    },
  },
};

void mqtt_publish(const char *s){
  size_t slen = strlen(s);
  printf("MQTT: %s\n", s);
  if(esp_mqtt_client_publish(MQTTHandle, MQTTTOPIC, s, slen, 0, 0)){
    fprintf(stderr, "couldn't publish %zuB mqtt message\n", slen);
  }
}

// arguments to dry are a target temp and number of seconds in the form
// TEMP/SECONDS. a well-formed request replaces any existing one, including
// cancelling it if SECONDS is 0. we allow leading and trailing space.
static int
handle_dry_req(const char* payload, size_t plen){
  unsigned seconds = 0;
  unsigned temp = 0;
  size_t idx = 0;
  enum {
    PRESPACE,
    TEMP,
    SLASH,
    SECONDS,
    POSTSPACE
  } state = PRESPACE;
  // FIXME need address wrapping of temp and/or seconds
  while(idx < plen){
    printf("payload[%zu] = 0x%02x state: %d temp: %u\n", idx, payload[idx], state, temp);
    if(payload[idx] >= 0x80 || payload[idx] <= 0){ // invalid character
      goto err;
    }
    unsigned char c = payload[idx];
    switch(state){
      case PRESPACE:
        if(!isspace(c)){
          if(isdigit(c)){
            state = TEMP;
            temp = c - '0';
          }else{
            goto err;
          }
        }
        break;
      case TEMP:
        if(isdigit(c)){
          temp *= 10;
          temp += c - '0';
        }else if(c == '/'){
          state = SLASH;
        }else{
          goto err;
        }
        break;
      case SLASH:
        if(isdigit(c)){
          seconds = c - '0';
          state = SECONDS;
        }else{
          goto err;
        }
        break;
      case SECONDS:
        if(isdigit(c)){
          seconds *= 10;
          seconds += c - '0';
        }else if(isspace(c)){
          state = POSTSPACE;
        }else{
          goto err;
        }
        break;
      case POSTSPACE:
        if(!isspace(c)){
          goto err;
        }
        break;
    }
    ++idx;
  }
  return handle_dry(seconds, temp);

err:
  fprintf(stderr, "invalid dry payload [%.*s]\n", plen, payload);
  return -1;
}

static inline bool
topic_matches(const esp_mqtt_event_t* e, const char* chan){
  if(strncmp(e->topic, chan, e->topic_len) == 0 && e->topic_len == strlen(chan)){
    return true;
  }
  return false;
}

static void
handle_mqtt_msg(const esp_mqtt_event_t* e){
  printf("control message [%.*s] [%.*s]\n", e->topic_len, e->topic, e->data_len, e->data);
  if(topic_matches(e, DRY_CHANNEL)){
    handle_dry_req(e->data, e->data_len);
  }else if(topic_matches(e, MOTOR_CHANNEL)){
    bool motor;
    int ret = extract_bool(e->data, e->data_len, &motor);
    if(ret == 0){
      set_motor(motor);
    }
  }else if(topic_matches(e, HEATER_CHANNEL)){
    bool heater;
    int ret = extract_bool(e->data, e->data_len, &heater);
    if(ret == 0){
      set_heater(heater);
    }
  }else if(topic_matches(e, LPWM_CHANNEL)){
    int pwm = extract_pwm(e->data, e->data_len);
    if(pwm >= 0){
      set_lower_pwm(pwm);
    }
  }else if(topic_matches(e, UPWM_CHANNEL)){
    int pwm = extract_pwm(e->data, e->data_len);
    if(pwm >= 0){
      set_upper_pwm(pwm);
    }
  }else if(topic_matches(e, TARE_CHANNEL)){
    set_tare();
  }else if(topic_matches(e, CALIBRATE_CHANNEL)){
    // FIXME get value, match against LastWeight - TareWeight
  }else if(topic_matches(e, FACTORYRESET_CHANNEL)){
    factory_reset();
    // ought not reach here
  }else{
    fprintf(stderr, "unknown topic [%.*s], ignoring message\n", e->topic_len, e->topic);
  }
}

static void
subscribe(esp_mqtt_client_handle_t handle, const char* chan){
  int er;
  if((er = esp_mqtt_client_subscribe(handle, chan, 0)) < 0){
    fprintf(stderr, "failure %d subscribing to %s\n", er, chan);
  }
}

static void
mqtt_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data){
  if(id == MQTT_EVENT_CONNECTED){
    printf("connected to mqtt\n");
    set_network_state(MQTT_ESTABLISHED);
    subscribe(MQTTHandle, MOTOR_CHANNEL);
    subscribe(MQTTHandle, LPWM_CHANNEL);
    subscribe(MQTTHandle, UPWM_CHANNEL);
    subscribe(MQTTHandle, DRY_CHANNEL);
    subscribe(MQTTHandle, TARE_CHANNEL);
    subscribe(MQTTHandle, CALIBRATE_CHANNEL);
    subscribe(MQTTHandle, FACTORYRESET_CHANNEL);
  }else if(id == MQTT_EVENT_DATA){
    handle_mqtt_msg(data);
  }else{
    printf("unhandled mqtt event %ld\n", id);
  }
}

static esp_err_t
httpd_get_handler(httpd_req_t *req){
#define RESPBYTES 1024
  char* resp = malloc(RESPBYTES);
  if(!resp){
    fprintf(stderr, "couldn't allocate httpd response\n");
    return ESP_FAIL;
  }
  time_t now = time(NULL);
  // FIXME need locking or atomics now
  int slen = snprintf(resp, RESPBYTES, "<!DOCTYPE html><html><head><title>" DEVICE "</title></head>"
            "<body><h2>a drying comes across the sky</h2><br/>"
            "lpwm: %u upwm: %u<br/>"
            "lrpm: %u urpm: %u<br/>"
            "motor: %s heater: %s<br/>"
            "mass: %f tare: %f<br/>"
            "lm35: %f esp32s3: %f<br/>"
            "dryends: %llu<br/>"
            "target temp: %lu<br/>"
            "<hr/>%s<br/>"
            "</body></html>",
            get_lower_pwm(), get_upper_pwm(),
            get_lower_rpm(), get_upper_rpm(),
            motor_state_http(),
            heater_state_http(),
            get_weight(), get_tare(),
            get_upper_temp(), get_lower_temp(),
            get_dry_ends_at(),
            get_target_temp(),
            asctime(localtime(&now))
            );
  esp_err_t ret = ESP_FAIL;
  if(slen < 0 || slen >= RESPBYTES){
    fprintf(stderr, "httpd response too large (%d)\n", slen);
  }else{
    esp_err_t e = httpd_resp_send(req, resp, slen);
    if(e != ESP_OK){
      fprintf(stderr, "failure (%s) sending http response\n", esp_err_to_name(e));
    }else{
      ret = ESP_OK;
    }
  }
  free(resp);
  return ret;
}


static int
setup_httpd(void){
  httpd_config_t hconf = HTTPD_DEFAULT_CONFIG();
  esp_err_t err;
  if((err = httpd_start(&HTTPServ, &hconf)) != ESP_OK){
    fprintf(stderr, "failure (%s) initializing httpd\n", esp_err_to_name(err));
    return -1;
  }
  const httpd_uri_t httpd_get = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = httpd_get_handler,
    .user_ctx = NULL,
  };
  if((err = httpd_register_uri_handler(HTTPServ, &httpd_get)) != ESP_OK){
    fprintf(stderr, "failure (%s) preparing URI %s\n", esp_err_to_name(err), httpd_get.uri);
    return -1;
  }
  return 0;
}
// we want to use the NTP servers provided by DHCP, so don't provide any
// static ones
static int
setup_sntp(void){
  esp_sntp_config_t sconf = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(0, {});
  sconf.start = false;
  sconf.server_from_dhcp = true;
  sconf.renew_servers_after_new_IP = true;
  sconf.ip_event_to_renew = IP_EVENT_STA_GOT_IP;
  esp_err_t e = esp_netif_sntp_init(&sconf);
  if(e != ESP_OK){
    fprintf(stderr, "error (%s) initializing SNTP\n", esp_err_to_name(e));
    return -1;
  }
  return 0;
}

static int
setup_mdns(void){
  esp_err_t err;
  if((err = mdns_init()) != ESP_OK){
    fprintf(stderr, "failure %d (%s) initializing mDNS\n", err, esp_err_to_name(err));
    return -1;
  }
  if((err = mdns_hostname_set(CLIENTID)) != ESP_OK){
    fprintf(stderr, "failure %d (%s) initializing mDNS\n", err, esp_err_to_name(err));
    return -1;
  }
  mdns_instance_name_set("Dankdryer");
  mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
  mdns_service_instance_name_set("_http", "_tcp", "Dankdryer webserver");
  return 0;
}

static void
wifi_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data){
  esp_err_t err;
  if(strcmp(base, WIFI_EVENT)){
    fprintf(stderr, "non-wifi event %s in wifi handler\n", base);
    return;
  }
  if(id == WIFI_EVENT_STA_START || id == WIFI_EVENT_STA_DISCONNECTED){
    set_network_state(WIFI_CONNECTING);
    if((err = esp_wifi_connect()) != ESP_OK){
      fprintf(stderr, "error (%s) connecting to wifi\n", esp_err_to_name(err));
    }else{
      printf("attempting to connect to wifi\n");
    }
  }else if(id == WIFI_EVENT_STA_CONNECTED){
    set_network_state(NET_CONNECTING);
    printf("connected to wifi, looking for ip\n");
    uint16_t aid = 65535u;
    esp_wifi_sta_get_aid(&aid);
    printf("association id: %u\n", aid);
  }else if(id == WIFI_EVENT_HOME_CHANNEL_CHANGE){
    printf("wifi channel changed\n");
  }else{
    fprintf(stderr, "unknown wifi event %ld\n", id);
  }
}

static void
ip_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data){
  esp_err_t err;
  if(strcmp(base, IP_EVENT)){
    fprintf(stderr, "non-ip event %s in ip handler\n", base);
    return;
  }
  if(id == IP_EVENT_STA_GOT_IP || id == IP_EVENT_GOT_IP6){
    printf("got network address, connecting to mqtt/sntp\n");
    if(SetupState != SETUP_STATE_CONFIGURED){
      SetupState = SETUP_STATE_CONFIGURED;
      write_wifi_config(WifiEssid, WifiPSK, SetupState);
    }
    if((err = esp_netif_sntp_start()) != ESP_OK){
      fprintf(stderr, "error (%s) starting SNTP\n", esp_err_to_name(err));
    }
    set_network_state(MQTT_CONNECTING);
    if((err = esp_mqtt_client_start(MQTTHandle)) != ESP_OK){
      fprintf(stderr, "error (%s) connecting to mqtt\n", esp_err_to_name(err));
    }
  }else if(id == IP_EVENT_STA_LOST_IP){
    fprintf(stderr, "lost ip address\n");
  }else{
    fprintf(stderr, "unknown ip event %ld\n", id);
  }
}

static int
setup_wifi(void){
  esp_err_t err;
  const wifi_init_config_t wificfg = WIFI_INIT_CONFIG_DEFAULT();
  wifi_config_t stacfg = {
    .sta = {
        // FIXME what if the network is unprotected (0 byte PSK)?
        .sort_method = WIFI_CONNECT_AP_BY_SECURITY,
        .threshold = {
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
        .sae_pwe_h2e = WPA3_SAE_PWE_HUNT_AND_PECK,
        .sae_h2e_identifier = "",
    },
  };
  strcpy((char *)stacfg.sta.ssid, (const char*)WifiEssid);
  strcpy((char *)stacfg.sta.password, (const char*)WifiPSK);
  //printf("[%s][%s]\n", stacfg.sta.ssid, stacfg.sta.password);
  if((err = esp_netif_init()) != ESP_OK){
    fprintf(stderr, "failure (%s) initializing tcp/ip\n", esp_err_to_name(err));
    return -1;
  }
  if((err = esp_event_loop_create_default()) != ESP_OK){
    fprintf(stderr, "failure (%s) creating loop\n", esp_err_to_name(err));
    return -1;
  }
  if(!esp_netif_create_default_wifi_sta()){
    fprintf(stderr, "failure creating default STA\n");
    return -1;
  }
  if((err = esp_wifi_init(&wificfg)) != ESP_OK){
    fprintf(stderr, "failure (%s) initializing wifi\n", esp_err_to_name(err));
    return -1;
  }
  esp_event_handler_instance_t wid;
  if((err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, &wid)) != ESP_OK){
    fprintf(stderr, "failure (%s) registering wifi events\n", esp_err_to_name(err));
    return -1;
  }
  esp_event_handler_instance_t ipd;
  if((err = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, ip_event_handler, NULL, &ipd)) != ESP_OK){
    fprintf(stderr, "failure (%s) registering ip events\n", esp_err_to_name(err));
    return -1;
  }
  if((err = esp_mqtt_client_register_event(MQTTHandle, MQTT_EVENT_CONNECTED, mqtt_event_handler, NULL)) != ESP_OK){
    fprintf(stderr, "failure (%s) registering mqtt events\n", esp_err_to_name(err));
    return -1;
  }
  if((err = esp_mqtt_client_register_event(MQTTHandle, MQTT_EVENT_DATA, mqtt_event_handler, NULL)) != ESP_OK){
    fprintf(stderr, "failure (%s) registering mqtt events\n", esp_err_to_name(err));
    return -1;
  }
  if((err = esp_wifi_set_mode(WIFI_MODE_STA)) != ESP_OK){
    fprintf(stderr, "failure (%s) setting STA mode\n", esp_err_to_name(err));
    return -1;
  }
  if((err = esp_wifi_set_config(WIFI_IF_STA, &stacfg)) != ESP_OK){
    fprintf(stderr, "failure (%s) configuring wifi\n", esp_err_to_name(err));
    return -1;
  }
  if((err = esp_wifi_start()) != ESP_OK){
    fprintf(stderr, "failure (%s) starting wifi\n", esp_err_to_name(err));
    return -1;
  }
  const int8_t txpower = 84;
  if((err = esp_wifi_set_max_tx_power(txpower)) != ESP_OK){
    fprintf(stderr, "warning: error (%s) setting tx power to %hd\n", esp_err_to_name(err), txpower);
  }
  setup_sntp(); // allow a failure
  setup_mdns(); // allow a failure
  if(setup_httpd()){
    return -1;
  }
  return 0;
}

static void
connect_wifi(void){
  SetupState = SETUP_STATE_WIFIATTEMPT;
  write_wifi_config(WifiEssid, WifiPSK, SetupState);
  setup_wifi();
}

static int
gatt_essid(uint16_t conn_handle, uint16_t attr_handle,
           struct ble_gatt_access_ctxt *ctxt, void *arg){
  fprintf(stderr, "essid] access op %d conn %hu attr %hu\n", ctxt->op, conn_handle, attr_handle);
  int r = BLE_ATT_ERR_UNLIKELY;
  if(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR){
    r = os_mbuf_append(ctxt->om, WifiEssid, strlen((const char*)WifiEssid) + 1);
    if(r){
      r = BLE_ATT_ERR_INSUFFICIENT_RES;
    }
  }else if(ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR){
    if(SetupState == SETUP_STATE_NEEDWIFI){
      ble_hs_mbuf_to_flat(ctxt->om, WifiEssid, sizeof(WifiEssid), NULL);
      printf("essid] [%s]\n", WifiEssid);
      if(strlen((const char*)WifiEssid) && strlen((const char*)WifiPSK)){
        connect_wifi();
      }
      r = 0;
    }
  }
  return r;
}

static int
gatt_psk(uint16_t conn_handle, uint16_t attr_handle,
         struct ble_gatt_access_ctxt *ctxt, void *arg){
  fprintf(stderr, "psk] access op %d conn %hu attr %hu\n", ctxt->op, conn_handle, attr_handle);
  int r = BLE_ATT_ERR_UNLIKELY;
  if(SetupState == SETUP_STATE_NEEDWIFI){
    if(ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR){
      ble_hs_mbuf_to_flat(ctxt->om, WifiPSK, sizeof(WifiPSK), NULL);
      if(strlen((const char*)WifiEssid) && strlen((const char*)WifiPSK)){
        connect_wifi();
      }
      r = 0;
    }
  }
  return r;
}

static int
gatt_setup_state(uint16_t conn_handle, uint16_t attr_handle,
                 struct ble_gatt_access_ctxt *ctxt, void *arg){
  fprintf(stderr, "setupstate] access op %d conn %hu attr %hu\n", ctxt->op, conn_handle, attr_handle);
  int r = BLE_ATT_ERR_UNLIKELY;
  if(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR){
    const char* s = state_str();
    r = os_mbuf_append(ctxt->om, s, strlen(s));
  }
  return r;
}

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
  {
    .type = BLE_GATT_SVC_TYPE_PRIMARY,
    .uuid = &setup_svc_uuid.u,
    .includes = NULL,
    .characteristics = (const struct ble_gatt_chr_def[]){
      {
        .uuid = &essid_chr_uuid.u,
        .access_cb = gatt_essid,
        .arg = NULL,
        .descriptors = NULL,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
        .min_key_size = 0,
        .val_handle = NULL,
        .cpfd = NULL,
      }, {
        .uuid = &psk_chr_uuid.u,
        .access_cb = gatt_psk,
        .arg = NULL,
        .descriptors = NULL,
        .flags = BLE_GATT_CHR_F_WRITE,
        .min_key_size = 0,
        .val_handle = NULL,
        .cpfd = NULL,
      }, {
        .uuid = &setup_state_chr_uuid.u,
        .access_cb = gatt_setup_state,
        .arg = NULL,
        .descriptors = NULL,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .min_key_size = 0,
        .val_handle = NULL,
        .cpfd = NULL,
      }, { 0 }
    }
  }, { 0 },
};

static void
bletask(void* v){
  nimble_port_run();
  printf("bletask] NimBLE stack exited, deinitializing task\n");
  nimble_port_freertos_deinit();
}

static void
ble_sync_cb(void){
  printf("sync] enabling advertisements\n");
  ble_addr_t addr;
  esp_err_t e;
  if((e = ble_hs_id_gen_rnd(1, &addr)) != ESP_OK){
    fprintf(stderr, "sync] failure (%s) generating address\n", esp_err_to_name(e));
  }else{
    if((e = ble_hs_id_set_rnd(addr.val)) != ESP_OK){
      fprintf(stderr, "sync] failure (%s) setting address\n", esp_err_to_name(e));
    }
  }
  struct ble_hs_adv_fields fields;
  memset(&fields, 0, sizeof(fields));
  fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP; 
  fields.tx_pwr_lvl_is_present = 1;
  fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
  fields.name = (const uint8_t*)ble_svc_gap_device_name();
  printf("sync] got gap device name [%s]\n", fields.name);
  fields.name_len = strlen((const char*)fields.name);
  fields.name_is_complete = 1;
  memset(&fields.uuids16, 0, sizeof(ble_uuid16_t));
  fields.num_uuids16 = 1;
  fields.uuids16_is_complete = 1;
  if((e = ble_gap_adv_set_fields(&fields)) != ESP_OK){
    fprintf(stderr, "sync] failure (%s) enabling ibeacon\n", esp_err_to_name(e));
  }
  struct ble_gap_adv_params advcfg = { 0 };
  advcfg.conn_mode = BLE_GAP_CONN_MODE_UND;
  advcfg.disc_mode = BLE_GAP_DISC_MODE_GEN;
  if((e = ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, BLE_HS_FOREVER, &advcfg, NULL, NULL)) != ESP_OK){
    fprintf(stderr, "sync] failure (%s) enabling advertisements\n", esp_err_to_name(e));
  }
}

static void
ble_reset_cb(int reason){
  printf("reset] callback (reason: %d)\n", reason);
}

static void
ble_gatts_register_cb(struct ble_gatt_register_ctxt *bctx, void *v){
  printf("gatts] register callback %p %p\n", bctx, v);
}

static int
ble_store_status_cb(struct ble_store_status_event *bevent, void *v){
  printf("storestatus] callback %p %p\n", bevent, v);
  return 0;
}

static int
setup_ble(void){
  esp_err_t err;
  if((err = nimble_port_init()) != ESP_OK){
    fprintf(stderr, "error (%s) initializing NimBLE\n", esp_err_to_name(err));
    return -1;
  }
  ble_hs_cfg.reset_cb = ble_reset_cb;
  ble_hs_cfg.sync_cb = ble_sync_cb;
  ble_hs_cfg.gatts_register_cb = ble_gatts_register_cb;
  ble_hs_cfg.store_status_cb = ble_store_status_cb;
  ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_DISP_ONLY;
  ble_hs_cfg.sm_bonding = 1;
  ble_hs_cfg.sm_our_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC;
  ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC;
  ble_hs_cfg.sm_mitm = 1;
  ble_hs_cfg.sm_sc = 0;
  ble_hs_cfg.sm_our_key_dist = 0;
  ble_hs_cfg.sm_their_key_dist = 0;
  ble_svc_gap_init();
  ble_svc_gatt_init();
  if((err = ble_gatts_count_cfg(gatt_svr_svcs)) != ESP_OK){
    return -1;
  }
  if((err = ble_gatts_add_svcs(gatt_svr_svcs)) != ESP_OK){
    return -1;
  }
  if((err = ble_svc_gap_device_name_set(CLIENTID)) != ESP_OK){
    fprintf(stderr, "error (%s) setting BLE name\n", esp_err_to_name(err));
    return -1;
  }
  ble_gatts_show_local();
  nimble_port_freertos_init(bletask);
  printf("successfully initialized BLE\n");
  return 0;
}

int setup_network(void){
  if((MQTTHandle = esp_mqtt_client_init(&MQTTConfig)) == NULL){
    fprintf(stderr, "couldn't create mqtt client\n");
    return -1;
  }
  int sstate;
  if(!read_wifi_config(WifiEssid, sizeof(WifiEssid), WifiPSK, sizeof(WifiPSK), &sstate)){
    if((SetupState = sstate) != SETUP_STATE_NEEDWIFI){
      setup_wifi();
    }
  }
  if(setup_ble()){
    return -1;
  }
  return 0;
}

