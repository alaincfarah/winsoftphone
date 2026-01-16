#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  char sip_domain[128];
  char sip_user[64];
  char sip_password[128];
  char sip_proxy[128];
  char sip_transport[16];
  int capture_device;
  int playback_device;
  float rx_level;
  float tx_level;
  char incoming_url_template[256];
  int auto_record;
  char recording_base_dir[260];
} app_config_t;

void config_set_defaults(app_config_t *config);
int config_load(app_config_t *config);
int config_save(const app_config_t *config);
int config_get_path(char *out_path, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif
