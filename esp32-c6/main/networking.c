#include "networking.h"
#include "dankdryer.h"
#include "version.h"
#include "efuse.h"
#include "ota.h"
#include <mdns.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <lwip/netif.h>
#include <nimble/ble.h>
#include <mqtt_client.h>
#include <host/ble_hs.h>
#include <host/ble_gatt.h>
#include <host/ble_hs_id.h>
#include <esp_netif_sntp.h>
#include <esp_http_server.h>
#include <host/ble_hs_mbuf.h>
#include <nimble/nimble_port.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>
#include <nimble/nimble_port_freertos.h>

#define TAG "net"

// 693c9ea2cb764ac38e6bb05d0bb57845 -- setup service
static const ble_uuid128_t setup_svc_uuid =
    BLE_UUID128_INIT(0x69, 0x3c, 0x9e, 0xa2, 0xcb, 0x76, 0x4a, 0xc3, 0x8e, 0x6b, 0xb0, 0x5d, 0x0b, 0xb5, 0x78, 0x45);

// characteristics for setup service

// write-only device ID LL-DDD-DDD-DDD
static const ble_uuid128_t deviceid_uuid =
    BLE_UUID128_INIT(0x09, 0x85, 0x82, 0x15, 0xd2, 0x5c, 0x41, 0x51, 0xb3, 0x9b, 0x76, 0xb3, 0x41, 0x45, 0x2a, 0x42);

// c3868812-e56b-4059-8c0c-54695d0baf6d -- essid (read, write in setup state 0)
static const ble_uuid128_t essid_chr_uuid =
    BLE_UUID128_INIT(0xc3, 0x86, 0x88, 0x12, 0xe5, 0x6b, 0x40, 0x59, 0x8c, 0x0c, 0x54, 0x69, 0x5d, 0x0b, 0xaf, 0x6d);

// WPA preshared key (write-only)
static const ble_uuid128_t psk_chr_uuid =
    BLE_UUID128_INIT(0x06, 0x2e, 0xf3, 0x54, 0xfe, 0xa5, 0x42, 0x47, 0x98, 0xc9, 0x9f, 0x9d, 0xa1, 0xee, 0x4c, 0x2e);

// string representing state (read-only)
static const ble_uuid128_t setup_state_chr_uuid =
    BLE_UUID128_INIT(0xa1, 0x54, 0xe0, 0x6e, 0xb5, 0x62, 0x40, 0x2f, 0x90, 0x57, 0xd2, 0x59, 0x01, 0x38, 0x97, 0xe2);

// mqtt configuration service
static const ble_uuid128_t mqtt_svc_uuid =
    BLE_UUID128_INIT(0x45, 0x13, 0x4f, 0xbd, 0x48, 0x0f, 0x45, 0xee, 0x9b, 0x39, 0x78, 0x7e, 0x00, 0x68, 0x32, 0x44);

// mqtt broker (read, write)
static const ble_uuid128_t mqttbroker_chr_uuid =
    BLE_UUID128_INIT(0x2e, 0xe6, 0x20, 0x3e, 0xb3, 0xc4, 0x47, 0x84, 0xab, 0x02, 0x83, 0xee, 0x7e, 0xe8, 0x76, 0xdd);

// mqtt user (read, write)
static const ble_uuid128_t mqttuser_chr_uuid =
    BLE_UUID128_INIT(0x2f, 0x5c, 0x13, 0xf7, 0x05, 0x98, 0x4f, 0xbf, 0x92, 0x16, 0xbc, 0x6d, 0xae, 0x53, 0x29, 0xb1);

// mqtt pass (write-only)
static const ble_uuid128_t mqttpass_chr_uuid =
    BLE_UUID128_INIT(0x5a, 0x15, 0x45, 0x14, 0xe1, 0x85, 0x45, 0x31, 0xaa, 0x36, 0xf3, 0xed, 0xad, 0x6a, 0xe8, 0x53);

// mqtt topic
static const ble_uuid128_t mqtttopic_chr_uuid =
    BLE_UUID128_INIT(0x0c, 0x3c, 0x1b, 0xc0, 0xe9, 0x98, 0x44, 0xda, 0x8f, 0x97, 0x50, 0xe0, 0xbd, 0xc4, 0x32, 0x82);

static SemaphoreHandle_t MQTTSemaphore;

// MQTTConfig is the actual config we're working with. BLEConfig is one being
// built up via a series of BLE GATT characteristic writes. When we commit the
// BLEConfig, we copy the entries into MQTTConfig.
static mqttconfig MQTTConfig, BLEConfig;

static esp_mqtt_client_handle_t MQTTHandle;

static httpd_handle_t HTTPServ;
static uint8_t WifiEssid[33];
static uint8_t WifiPSK[65];
// we use the product name and the six digit serial number
static char clientID[__builtin_strlen(DEVICE) + 6 + 1];

// unsafe from ISR context
static bool mqtt_lock(void){
  if(xSemaphoreTake(MQTTSemaphore, portMAX_DELAY) == pdTRUE){
    return false;
  }
  ESP_LOGE(TAG, "couldn't take mqtt lock");
  return true;
}

// unsafe from ISR context
static void mqtt_unlock(void){
  xSemaphoreGive(MQTTSemaphore);
}

// copies the incoming configuration. if that went without problem, frees the
// old configuration, and installs the copied elements.
static bool
mqttconfig_copy(mqttconfig *old, const mqttconfig* newconf){
  char *broker, *topic, *user, *pass;
  broker = topic = user = pass = NULL;
  if(newconf->broker){
    if((broker = strdup(newconf->broker)) == NULL){
      goto err;
    }
  }
  if(newconf->user){
    if((user = strdup(newconf->user)) == NULL){
      goto err;
    }
  }
  if(newconf->pass){
    if((pass = strdup(newconf->pass)) == NULL){
      goto err;
    }
  }
  if(newconf->topic){
    if((topic = strdup(newconf->topic)) == NULL){
      goto err;
    }
  }
  // any necessary copies succeeded; install it
  mqttconfig_free(old);
  old->broker = broker;
  old->user = user;
  old->pass = pass;
  old->topic = topic;
  return false;

err:
  free(broker);
  free(topic);
  free(user);
  free(pass);
  return true;
}

// frees the members of conf, but not conf itself
void mqttconfig_free(mqttconfig *conf){
  if(conf){
    free(conf->broker);
    free(conf->user);
    free(conf->pass);
    free(conf->topic);
  }
}

static inline bool
string_nonempty_p(const char* s){
  return s && strlen(s);
}

// mqtt lock must be held around call
// if successful, returns any old handle, which must be destroyed
//  (this can and should be done after unlocking mqtt_lock)
static esp_mqtt_client_handle_t
reconfig_mqtt(const mqttconfig* newconfig){
  const esp_mqtt_client_config_t conf = {
    .broker = {
      .address = {
        .uri = newconfig->broker,
      },
    },
    .credentials = {
      .username = newconfig->user,
      .authentication = {
        .password = newconfig->pass,
      },
    },
  };
  // if a username is provided, a password must be provided. if a username
  // is not provided, a password must not be provided.
  if(string_nonempty_p(conf.credentials.username) !=
      string_nonempty_p(conf.credentials.authentication.password)){
    ESP_LOGE(TAG, "not using config missing user or password");
    return NULL;
  }
  // if we don't specify a broker, don't allow a username or password
  if(string_nonempty_p(conf.credentials.username) && !string_nonempty_p(conf.broker.address.uri)){
    ESP_LOGE(TAG, "not using config with user but no broker");
    return NULL;
  }
  esp_mqtt_client_handle_t oldmqtt, newmqtt;
  if(conf.broker.address.uri){
    if((newmqtt = esp_mqtt_client_init(&conf)) == NULL){
      ESP_LOGE(TAG, "couldn't create mqtt client"); // FIXME logging while locked?
      return NULL;
    }
    if(!mqttconfig_copy(&MQTTConfig, newconfig)){
      write_mqtt_config(&MQTTConfig);
    }else{
      esp_mqtt_client_destroy(newmqtt);
    }
  }else{
    newmqtt = NULL;
  }
  oldmqtt = MQTTHandle;
  MQTTHandle = newmqtt;
  return oldmqtt; // can't distinguish error from no prior handle, hrmm
}

// call with mqtt_lock held. unlocks mqtt_lock on all paths. frees any old
// mqtt handle we obsoleted.
static inline int
reconfig_and_free_mqtt(void){
  esp_mqtt_client_handle_t oldmqtt = reconfig_mqtt(&BLEConfig);
  mqtt_unlock();
  if(oldmqtt){
    esp_mqtt_client_destroy(oldmqtt);
  }
  return 0;
}

// call during initialization to construct clientID
static void
set_client_name(void){
  strcpy(clientID, DEVICE);
  char* serial = clientID + __builtin_strlen(DEVICE);
  sprintf(serial, "%06" PRIu32, get_serial_num());
  ESP_LOGI(TAG, "using hostname %s", clientID);
}

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

void mqtt_publish(const char *s){
  size_t slen = strlen(s);
  //ESP_LOGI(TAG, "MQTT: %s", s);
  if(mqtt_lock()){
    return;
  }
  if(MQTTHandle && MQTTConfig.topic){
    if(esp_mqtt_client_publish(MQTTHandle, MQTTConfig.topic, s, slen, 0, 0)){
      ESP_LOGE(TAG, "couldn't publish %zuB mqtt message", slen);
    }
  }
  mqtt_unlock();
}

static void
subscribe(esp_mqtt_client_handle_t handle, const char* chan){
  int er;
  if((er = esp_mqtt_client_subscribe(handle, chan, 0)) < 0){
    ESP_LOGE(TAG, "failure %d subscribing to %s", er, chan);
  }
}

// home assistant autodiscovery message
static void
mqtt_publish_hadiscovery(void){
  #define DISCOVERYPREFIX "homeassistant" // FIXME can be changed in HA
  #define DKEY "device"
  #define CKEY "config"
  char topic[__builtin_strlen(DISCOVERYPREFIX) + 1
             + __builtin_strlen(DKEY) + 1
             + sizeof(clientID) + 1
             + __builtin_strlen(CKEY) + 1];
  snprintf(topic, sizeof(topic), DISCOVERYPREFIX "/" DKEY "/%s/" CKEY, clientID); 

  // FIXME set up discovery message
  static const char s[] = "";
  ESP_LOGI(TAG, "HADiscovery to %s: [%s]", topic, s);
  mqtt_publish(s);
  #undef CKEY
  #undef DKEY
  #undef DISCOVERYPREFIX
}

static void
mqtt_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data){
  if(id == MQTT_EVENT_CONNECTED){
    ESP_LOGI(TAG, "connected to mqtt");
    set_network_state(MQTT_ESTABLISHED);
    subscribe(MQTTHandle, MOTOR_CHANNEL);
    subscribe(MQTTHandle, HEATER_CHANNEL);
    subscribe(MQTTHandle, LPWM_CHANNEL);
    subscribe(MQTTHandle, UPWM_CHANNEL);
    subscribe(MQTTHandle, OTA_CHANNEL);
    subscribe(MQTTHandle, DRY_CHANNEL);
    subscribe(MQTTHandle, TARE_CHANNEL);
    subscribe(MQTTHandle, CALIBRATE_CHANNEL);
    subscribe(MQTTHandle, FACTORYRESET_CHANNEL);
    mqtt_publish_hadiscovery();
  }else if(id == MQTT_EVENT_DATA){
    handle_mqtt_msg(data);
  }else{
    ESP_LOGE(TAG, "unhandled mqtt event %" PRId32, id);
  }
}

static esp_err_t
httpd_get_handler(httpd_req_t *req){
#define RESPBYTES 1024
  char* resp = malloc(RESPBYTES);
  if(!resp){
    ESP_LOGE(TAG, "couldn't allocate httpd response");
    return ESP_FAIL;
  }
  time_t now = time(NULL);
  // FIXME need locking or atomics now
  int slen = snprintf(resp, RESPBYTES, "<!DOCTYPE html><html><head><title>" DEVICE "</title></head>"
            "<body><h2>a drying comes across the sky</h2><br/>"
            "<b>lpwm:</b> %u<br/>"
            "<b>upwm:</b> %u<br/>"
            "<b>lrpm</b>: %u<br/>"
            "<b>urpm</b>: %u<br/>"
            "<b>srpm</b>: %" PRIu32 "<br/>"
            "<b>motor</b>: %s<br/>"
            "<b>heater</b>: %s<br/>"
            "<b>mass</b>: %.2f<br/>"
            "<b>tare</b>: %.2f<br/>"
            "<b>lm35</b>: %.2f<br/>"
            "<b>esp32s3</b>: %.2f<br/>"
            "<b>dryends</b>: %llu<br/>"
            "<b>target temp</b>: %lu<br/>"
            "<hr/>%s<br/>"
            "</body></html>",
            get_lower_pwm(), get_upper_pwm(),
            get_lower_rpm(), get_upper_rpm(),
            get_spool_rpm(),
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
    ESP_LOGE(TAG, "httpd response too large (%d)", slen);
  }else{
    esp_err_t e = httpd_resp_send(req, resp, slen);
    if(e != ESP_OK){
      ESP_LOGE(TAG, "failure (%s) sending http response", esp_err_to_name(e));
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
    ESP_LOGE(TAG, "failure (%s) initializing httpd", esp_err_to_name(err));
    return -1;
  }
  const httpd_uri_t httpd_get = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = httpd_get_handler,
    .user_ctx = NULL,
  };
  if((err = httpd_register_uri_handler(HTTPServ, &httpd_get)) != ESP_OK){
    ESP_LOGE(TAG, "failure (%s) preparing URI %s", esp_err_to_name(err), httpd_get.uri);
    return -1;
  }
  return 0;
}

// we want to use the NTP servers provided by DHCP, so don't provide any
// static ones
static int
setup_sntp(void){
  esp_sntp_config_t sconf = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
  sconf.start = false;
  sconf.server_from_dhcp = true;
  sconf.renew_servers_after_new_IP = true;
  sconf.index_of_first_server = 1;
  sconf.ip_event_to_renew = IP_EVENT_STA_GOT_IP;
  esp_err_t e = esp_netif_sntp_init(&sconf);
  if(e != ESP_OK){
    ESP_LOGE(TAG, "error (%s) initializing SNTP", esp_err_to_name(e));
    return -1;
  }
  return 0;
}

static int
setup_mdns(void){
  esp_err_t err;
  if((err = mdns_init()) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) initializing mDNS", esp_err_to_name(err));
    return -1;
  }
  if((err = mdns_hostname_set(clientID)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) initializing mDNS", esp_err_to_name(err));
    return -1;
  }
  mdns_instance_name_set(clientID);
  mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
  mdns_service_instance_name_set("_http", "_tcp", clientID);
  return 0;
}

static void
wifi_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data){
  esp_err_t err;
  if(strcmp(base, WIFI_EVENT)){
    ESP_LOGE(TAG, "non-wifi event %s in wifi handler", base);
    return;
  }
  if(id == WIFI_EVENT_STA_START || id == WIFI_EVENT_STA_DISCONNECTED){
    set_network_state(WIFI_CONNECTING);
    if((err = esp_wifi_connect()) != ESP_OK){
      ESP_LOGE(TAG, "error (%s) connecting to wifi", esp_err_to_name(err));
    }else{
      ESP_LOGI(TAG, "attempting to connect to wifi");
    }
  }else if(id == WIFI_EVENT_STA_CONNECTED){
    set_network_state(NET_CONNECTING);
    ESP_LOGI(TAG, "connected to wifi, looking for ip");
    uint16_t aid = 65535u;
    esp_wifi_sta_get_aid(&aid);
    ESP_LOGI(TAG, "association id: %u", aid);
  }else if(id == WIFI_EVENT_HOME_CHANNEL_CHANGE){
    ESP_LOGI(TAG, "wifi channel changed");
  }else{
    ESP_LOGE(TAG, "unknown wifi event %" PRId32, id);
  }
}

static void
ip_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data){
  esp_err_t err;
  if(strcmp(base, IP_EVENT)){
    ESP_LOGE(TAG, "non-ip event %s in ip handler", base);
    return;
  }
  if(id == IP_EVENT_STA_GOT_IP || id == IP_EVENT_GOT_IP6){
    ESP_LOGI(TAG, "got network address, connecting to mqtt/sntp");
    if(SetupState != SETUP_STATE_CONFIGURED){
      SetupState = SETUP_STATE_CONFIGURED;
      write_wifi_config(WifiEssid, WifiPSK, SetupState);
    }
    if((err = esp_netif_sntp_start()) != ESP_OK){
      ESP_LOGE(TAG, "error (%s) starting SNTP", esp_err_to_name(err));
    }
    set_network_state(MQTT_CONNECTING);
    if((err = esp_mqtt_client_start(MQTTHandle)) != ESP_OK){
      ESP_LOGE(TAG, "error (%s) connecting to mqtt", esp_err_to_name(err));
    }
  }else if(id == IP_EVENT_STA_LOST_IP){
    ESP_LOGE(TAG, "lost ip address");
  }else{
    ESP_LOGE(TAG, "unknown ip event %ld", id);
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
  //ESP_LOGI(TAG, "[%s][%s]", stacfg.sta.ssid, stacfg.sta.password);
  if((err = esp_netif_init()) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) initializing tcp/ip", esp_err_to_name(err));
    return -1;
  }
  if((err = esp_event_loop_create_default()) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) creating loop", esp_err_to_name(err));
    return -1;
  }
  if(!esp_netif_create_default_wifi_sta()){
    ESP_LOGE(TAG, "error creating default STA");
    return -1;
  }
  if((err = esp_wifi_init(&wificfg)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) initializing wifi", esp_err_to_name(err));
    return -1;
  }
  esp_event_handler_instance_t wid;
  if((err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, &wid)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) registering wifi events", esp_err_to_name(err));
    return -1;
  }
  esp_event_handler_instance_t ipd;
  if((err = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, ip_event_handler, NULL, &ipd)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) registering ip events", esp_err_to_name(err));
    return -1;
  }
  if((err = esp_mqtt_client_register_event(MQTTHandle, MQTT_EVENT_CONNECTED, mqtt_event_handler, NULL)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) registering mqtt events", esp_err_to_name(err));
    return -1;
  }
  if((err = esp_mqtt_client_register_event(MQTTHandle, MQTT_EVENT_DATA, mqtt_event_handler, NULL)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) registering mqtt events", esp_err_to_name(err));
    return -1;
  }
  if((err = esp_wifi_set_mode(WIFI_MODE_STA)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) setting STA mode", esp_err_to_name(err));
    return -1;
  }
  if((err = esp_wifi_set_config(WIFI_IF_STA, &stacfg)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) configuring wifi", esp_err_to_name(err));
    return -1;
  }
  if((err = esp_wifi_start()) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) starting wifi", esp_err_to_name(err));
    return -1;
  }
  const int8_t txpower = 84;
  if((err = esp_wifi_set_max_tx_power(txpower)) != ESP_OK){
    ESP_LOGW(TAG, "warning: error (%s) setting tx power to %hd", esp_err_to_name(err), txpower);
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

// write-only. if it gets a valid deviceID, it is written to the eFuse user
// block, and we reset, coming up with our new hostname (no return value
// will be provided to the client in this case; their connection will be
// interrupted. who cares? this is done at the factory). an invalid clientID
// will return an error.
static int
gatt_deviceid(uint16_t conn_handle, uint16_t attr_handle,
              struct ble_gatt_access_ctxt *ctxt, void *arg){
  ESP_LOGI(TAG, "access op %d conn %hu attr %hu", ctxt->op, conn_handle, attr_handle);
  int r = BLE_ATT_ERR_UNLIKELY;
  if(ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR){
    unsigned char devid[DEVICEIDLEN];
    uint16_t olen;
    if(ble_hs_mbuf_to_flat(ctxt->om, devid, sizeof(devid), &olen) == 0){
      devid[olen] = '\0';
      ESP_LOGI(TAG, "deviceID [%s]", devid);
      r = set_device_id(devid);
    }
  }
  return r;
}

static int
gatt_essid(uint16_t conn_handle, uint16_t attr_handle,
           struct ble_gatt_access_ctxt *ctxt, void *arg){
  ESP_LOGI(TAG, "essid] access op %d conn %hu attr %hu", ctxt->op, conn_handle, attr_handle);
  int r = BLE_ATT_ERR_UNLIKELY;
  if(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR){
    r = os_mbuf_append(ctxt->om, WifiEssid, strlen((const char*)WifiEssid) + 1);
    if(r){
      r = BLE_ATT_ERR_INSUFFICIENT_RES;
    }
  }else if(ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR){
    if(SetupState == SETUP_STATE_NEEDWIFI){
      uint16_t olen;
      ble_hs_mbuf_to_flat(ctxt->om, WifiEssid, sizeof(WifiEssid), &olen);
      WifiEssid[olen] = '\0';
      ESP_LOGI(TAG, "essid] [%s]", WifiEssid);
      if(strlen((const char*)WifiEssid) && strlen((const char*)WifiPSK)){
        connect_wifi();
      }
      r = 0;
    }
  }
  return r;
}

static int
ble_reply_characteristic(struct ble_gatt_access_ctxt* ctxt, const char* s){
  int r;
  if(mqtt_lock()){
    return -1; // FIXME probably want value from BLE return values
  }
  if(s){
    r = os_mbuf_append(ctxt->om, s, strlen(s) + 1);
  }else{
    r = os_mbuf_append(ctxt->om, "", 1);
  }
  mqtt_unlock();
  if(r){
    r = BLE_ATT_ERR_INSUFFICIENT_RES;
  }
  return r;
}

static int
ble_accept_characteristic(struct ble_gatt_access_ctxt* ctxt, char** s){
    #define MAX_CHARACTERISTIC 256
    char* buf;
    uint16_t olen;
    if((buf = malloc(MAX_CHARACTERISTIC)) == NULL){
      ESP_LOGE(TAG, "mqtt] allocation failure");
      return BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    ble_hs_mbuf_to_flat(ctxt->om, buf, MAX_CHARACTERISTIC, &olen);
    buf[olen] = '\0';
    ESP_LOGI(TAG, "mqtt] got [%s]", buf);
    if(mqtt_lock()){
      return -1;
    }
    free(*s);
    *s = buf;
    return reconfig_and_free_mqtt();
    #undef MAX_CHARACTERISTIC
}

static int
gatt_mqtt_user(uint16_t conn_handle, uint16_t attr_handle,
               struct ble_gatt_access_ctxt *ctxt, void *arg){
  ESP_LOGI(TAG, "mqttuser] access op %d conn %hu attr %hu", ctxt->op, conn_handle, attr_handle);
  int r = BLE_ATT_ERR_UNLIKELY;
  if(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR){
    r = ble_reply_characteristic(ctxt, BLEConfig.user);
  }else if(ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR){
    r = ble_accept_characteristic(ctxt, &BLEConfig.user);
  }
  return r;
}

static int
gatt_mqtt_pass(uint16_t conn_handle, uint16_t attr_handle,
               struct ble_gatt_access_ctxt *ctxt, void *arg){
  ESP_LOGI(TAG, "mqttpass] access op %d conn %hu attr %hu", ctxt->op, conn_handle, attr_handle);
  int r = BLE_ATT_ERR_UNLIKELY;
  if(ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR){
    r = ble_accept_characteristic(ctxt, &BLEConfig.pass);
  }
  return r;
}

static int
gatt_mqtt_topic(uint16_t conn_handle, uint16_t attr_handle,
                struct ble_gatt_access_ctxt *ctxt, void *arg){
  ESP_LOGI(TAG, "mqtttopic] access op %d conn %hu attr %hu", ctxt->op, conn_handle, attr_handle);
  int r = BLE_ATT_ERR_UNLIKELY;
  if(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR){
    r = ble_reply_characteristic(ctxt, BLEConfig.topic);
  }else if(ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR){
    r = ble_accept_characteristic(ctxt, &BLEConfig.topic);
  }
  return r;
}

static int
gatt_mqtt_broker(uint16_t conn_handle, uint16_t attr_handle,
                 struct ble_gatt_access_ctxt *ctxt, void *arg){
  ESP_LOGI(TAG, "mqttbroker] access op %d conn %hu attr %hu", ctxt->op, conn_handle, attr_handle);
  int r = BLE_ATT_ERR_UNLIKELY;
  if(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR){
    r = ble_reply_characteristic(ctxt, BLEConfig.broker);
  }else if(ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR){
    r = ble_accept_characteristic(ctxt, &BLEConfig.broker);
  }
  return r;
}

static int
gatt_psk(uint16_t conn_handle, uint16_t attr_handle,
         struct ble_gatt_access_ctxt *ctxt, void *arg){
  ESP_LOGI(TAG, "psk] access op %d conn %hu attr %hu", ctxt->op, conn_handle, attr_handle);
  int r = BLE_ATT_ERR_UNLIKELY;
  if(SetupState == SETUP_STATE_NEEDWIFI){
    if(ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR){
      uint16_t olen;
      ble_hs_mbuf_to_flat(ctxt->om, WifiPSK, sizeof(WifiPSK), &olen);
      WifiPSK[olen] = '\0';
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
  ESP_LOGI(TAG, "setupstate] access op %d conn %hu attr %hu", ctxt->op, conn_handle, attr_handle);
  int r = BLE_ATT_ERR_UNLIKELY;
  if(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR){
    const char* s = state_str();
    r = os_mbuf_append(ctxt->om, s, strlen(s));
  }
  return r;
}

// until we have our device ID loaded, we only expose one characteristic
static const struct ble_gatt_svc_def gatt_svr_svcs_early[] = {
  {
    .type = BLE_GATT_SVC_TYPE_PRIMARY,
    .uuid = &setup_svc_uuid.u,
    .includes = NULL,
    .characteristics = (const struct ble_gatt_chr_def[]){
      {
        .uuid = &deviceid_uuid.u,
        .access_cb = gatt_deviceid,
        .arg = NULL,
        .descriptors = NULL,
        .flags = BLE_GATT_CHR_F_WRITE,
        .min_key_size = 0,
        .val_handle = NULL,
        .cpfd = NULL,
      }, { 0 }
    }
  }, { 0 },
};


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

static const struct ble_gatt_svc_def gatt_mqtt_svcs[] = {
  {
    .type = BLE_GATT_SVC_TYPE_PRIMARY,
    .uuid = &mqtt_svc_uuid.u,
    .includes = NULL,
    .characteristics = (const struct ble_gatt_chr_def[]){
      {
        .uuid = &mqttbroker_chr_uuid.u,
        .access_cb = gatt_mqtt_broker,
        .arg = NULL,
        .descriptors = NULL,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
        .min_key_size = 0,
        .val_handle = NULL,
        .cpfd = NULL,
      }, {
        .uuid = &mqttuser_chr_uuid.u,
        .access_cb = gatt_mqtt_user,
        .arg = NULL,
        .descriptors = NULL,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
        .min_key_size = 0,
        .val_handle = NULL,
        .cpfd = NULL,
      }, {
        .uuid = &mqttpass_chr_uuid.u,
        .access_cb = gatt_mqtt_pass,
        .arg = NULL,
        .descriptors = NULL,
        .flags = BLE_GATT_CHR_F_WRITE,
        .min_key_size = 0,
        .val_handle = NULL,
        .cpfd = NULL,
      }, {
        .uuid = &mqtttopic_chr_uuid.u,
        .access_cb = gatt_mqtt_topic,
        .arg = NULL,
        .descriptors = NULL,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
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
  ESP_LOGI(TAG, "bletask] NimBLE stack exited, deinitializing task");
  nimble_port_freertos_deinit();
}

static void
ble_sync_cb(void){
  ESP_LOGI(TAG, "sync] enabling advertisements");
  ble_addr_t addr;
  esp_err_t e;
  if((e = ble_hs_id_gen_rnd(1, &addr)) != ESP_OK){
    ESP_LOGE(TAG, "sync] error (%s) generating address", esp_err_to_name(e));
  }else{
    if((e = ble_hs_id_set_rnd(addr.val)) != ESP_OK){
      ESP_LOGE(TAG, "sync] error (%s) setting address", esp_err_to_name(e));
    }
  }
  struct ble_hs_adv_fields fields;
  memset(&fields, 0, sizeof(fields));
  fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP; 
  fields.tx_pwr_lvl_is_present = 1;
  fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
  fields.name = (const uint8_t*)ble_svc_gap_device_name();
  ESP_LOGI(TAG, "sync] got gap device name [%s]", fields.name);
  fields.name_len = strlen((const char*)fields.name);
  fields.name_is_complete = 1;
  memset(&fields.uuids16, 0, sizeof(ble_uuid16_t));
  fields.num_uuids16 = 1;
  fields.uuids16_is_complete = 1;
  if((e = ble_gap_adv_set_fields(&fields)) != ESP_OK){
    ESP_LOGE(TAG, "sync] failure (%s) enabling ibeacon", esp_err_to_name(e));
  }
  struct ble_gap_adv_params advcfg = { 0 };
  advcfg.conn_mode = BLE_GAP_CONN_MODE_UND;
  advcfg.disc_mode = BLE_GAP_DISC_MODE_GEN;
  if((e = ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, BLE_HS_FOREVER, &advcfg, NULL, NULL)) != ESP_OK){
    ESP_LOGE(TAG, "sync] failure (%s) enabling advertisements", esp_err_to_name(e));
  }
}

static void
ble_reset_cb(int reason){
  ESP_LOGI(TAG, "reset] callback (reason: %d)", reason);
}

static void
ble_gatts_register_cb(struct ble_gatt_register_ctxt *bctx, void *v){
  ESP_LOGI(TAG, "gatts] register callback %p %p", bctx, v);
}

static int
ble_store_status_cb(struct ble_store_status_event *bevent, void *v){
  ESP_LOGI(TAG, "storestatus] callback %p %p", bevent, v);
  return 0;
}

static int
setup_ble(void){
  esp_err_t err;
  if((err = nimble_port_init()) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) initializing NimBLE", esp_err_to_name(err));
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
  // wifi service depends on state
  if(deviceid_configured()){
    if((err = ble_gatts_count_cfg(gatt_svr_svcs)) != ESP_OK){
      return -1;
    }
    if((err = ble_gatts_add_svcs(gatt_svr_svcs)) != ESP_OK){
      return -1;
    }
  }else{
    if((err = ble_gatts_count_cfg(gatt_svr_svcs_early)) != ESP_OK){
      return -1;
    }
    if((err = ble_gatts_add_svcs(gatt_svr_svcs_early)) != ESP_OK){
      return -1;
    }
  }
  // always add mqtt service
  if((err = ble_gatts_count_cfg(gatt_mqtt_svcs)) != ESP_OK){
    return -1;
  }
  if((err = ble_gatts_add_svcs(gatt_mqtt_svcs)) != ESP_OK){
    return -1;
  }
  if((err = ble_svc_gap_device_name_set(clientID)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) setting BLE name", esp_err_to_name(err));
    return -1;
  }
  ble_gatts_show_local();
  nimble_port_freertos_init(bletask);
  ESP_LOGI(TAG, "successfully initialized BLE");
  return 0;
}

int setup_network(void){
  mqttconfig mqttconf;
  set_client_name();
  int sstate;
  read_mqtt_config(&mqttconf);
  if(mqtt_lock() == 0){
    // don't call reconfig_and_free_mqtt() here; it would force a useless
    // write per boot of what we just read
    // FIXME need to seed BLEConfig
    reconfig_mqtt(&mqttconf);
    mqtt_unlock();
  }
  mqttconfig_free(&mqttconf);
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
