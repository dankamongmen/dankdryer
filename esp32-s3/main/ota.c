#include "ota.h"
#include <esp_ota_ops.h>

int ota_init(void){
  const esp_partition_t *boot = esp_ota_get_boot_partition();
  printf("OTA boot partition: %s %lu:%lu size %ld type %d:%d\n",
         boot->label, boot->address, boot->address + boot->size - 1,
         boot->size, boot->type, boot->subtype);
  boot = esp_ota_get_running_partition();
  printf("OTA running partition: %s %lu:%lu size %ld type %d:%d\n",
         boot->label, boot->address, boot->address + boot->size - 1,
         boot->size, boot->type, boot->subtype);
  uint8_t pcount = esp_ota_get_app_partition_count();
  printf("OTA partition count: %u\n", pcount);
  if(pcount > 1){
    boot = esp_ota_get_next_update_partition(boot);
    printf("OTA next partition: %s %lu:%lu size %ld type %d:%d\n",
          boot->label, boot->address, boot->address + boot->size - 1,
          boot->size, boot->type, boot->subtype);
  }
  return 0;
}
