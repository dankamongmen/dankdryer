#include "ble.h"
#include <nimble/ble.h>
#include <host/ble_hs.h>
#include <host/ble_gatt.h>
#include <host/ble_hs_id.h>
#include <esp_nimble_hci.h>
#include <host/ble_hs_mbuf.h>
#include <nimble/nimble_port.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>
#include <nimble/nimble_port_freertos.h>

// generated randomly with uuidgen; ought we conform to some authority?
// 693c9ea2cb764ac38e6bb05d0bb57845
static const ble_uuid128_t gatt_svr_svc_uuid =
    BLE_UUID128_INIT(0x69, 0x3c, 0x9e, 0xa2, 0xcb, 0x76, 0x4a, 0xc3, 0x8e, 0x6b, 0xb0, 0x5d, 0x0b, 0xb5, 0x78, 0x45);

// c3868812-e56b-4059-8c0c-54695d0baf6d
static const ble_uuid128_t gatt_svr_chr_uuid =
    BLE_UUID128_INIT(0xc3, 0x86, 0x88, 0x12, 0xe5, 0x6b, 0x40, 0x59, 0x8c, 0x0c, 0x54, 0x69, 0x5d, 0x0b, 0xaf, 0x6d);

static int
gatt_svr_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                    struct ble_gatt_access_ctxt *ctxt, void *arg){
  fprintf(stderr, "chr] access op %d conn %hu attr %hu\n", ctxt->op, conn_handle, attr_handle);
  switch(ctxt->op){
  case BLE_GATT_ACCESS_OP_READ_CHR:
    fprintf(stderr, "chr] BLE read attempted\n");
    break;
  case BLE_GATT_ACCESS_OP_WRITE_CHR:
    fprintf(stderr, "chr] BLE write attempted\n");
    break;
  default:
    return BLE_ATT_ERR_UNLIKELY;
  }
  return 0;
}

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
  {
    .type = BLE_GATT_SVC_TYPE_PRIMARY,
    .uuid = &gatt_svr_svc_uuid.u,
    .includes = NULL,
    .characteristics = (const struct ble_gatt_chr_def[]){
      {
        .uuid = &gatt_svr_chr_uuid.u,
        .access_cb = gatt_svr_chr_access,
        .arg = NULL,
        .descriptors = NULL,
        .flags = BLE_GATT_CHR_F_READ,
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

int setup_ble(void){
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
