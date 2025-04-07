#ifndef DANKDRYER_OTA
#define DANKDRYER_OTA

int ota_init(void);

// check for a new OTA package upstream. if one is there, try to download
// and apply it. a return value of 0 indicates no new OTA. any other return
// value indicates an error. a successful OTA will result in a reboot, and
// thus there will be no return.
int attempt_ota(void);

#endif
