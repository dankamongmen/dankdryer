#include "ota.h"
#include <esp_ota_ops.h>

// name is functional name, not partition label (which is discovered)
static void
part_info(const char* name, const esp_partition_t* p){
  printf("%s: %s 0x%lx:0x%lx size 0x%lx type %d:%d\n", name,
         p->label, p->address, p->address + p->size - 1,
         p->size, p->type, p->subtype);
  esp_ota_img_states_t state;
  esp_err_t e = esp_ota_get_state_partition(p, &state);
  if(e == ESP_OK){
    printf("\treported ota state %d\n", state);
  }else{
    fprintf(stderr, "error (%s) getting %s state\n", esp_err_to_name(e), name);
  }
  esp_app_desc_t pi;
  e = esp_ota_get_partition_description(p, &pi);
  if(e == ESP_OK){
    printf("\treported app %s version %s\n", pi.project_name, pi.version);
  }else{
    fprintf(stderr, "error (%s) getting %s info\n", esp_err_to_name(e), name);
  }
}

int ota_init(void){
  const esp_partition_t *boot = esp_ota_get_boot_partition();
  part_info("OTA boot partition", boot);
  const esp_partition_t *runp = esp_ota_get_running_partition();
  part_info("OTA running partition", runp);
  uint8_t pcount = esp_ota_get_app_partition_count();
  printf("OTA partition count: %u\n", pcount);
  if(pcount > 1){
    const esp_partition_t* next = esp_ota_get_next_update_partition(runp);
    part_info("OTA next partition", next);
  }
  return 0;
}

int attempt_ota(void){
  // FIXME check for new package
  return -1;
}
