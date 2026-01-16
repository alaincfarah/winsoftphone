#ifndef SIP_H
#define SIP_H

#include <stddef.h>
#include <pjsua.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int id;
  char name[128];
  int input_count;
  int output_count;
} sip_audio_device_t;

typedef struct {
  void (*on_incoming_call)(void *user_data, pjsua_call_id call_id,
                           const char *cname, const char *uri);
  void (*on_call_state)(void *user_data, pjsua_call_id call_id,
                        pjsip_inv_state state, const char *state_text);
  void (*on_call_end)(void *user_data, pjsua_call_id call_id,
                      int duration_sec, const char *direction,
                      const char *cname, const char *uri);
} sip_callbacks_t;

typedef struct {
  pjsua_acc_id acc_id;
  pjsua_call_id active_call_id;
  pjsua_call_id warm_transfer_call_id;
  pjsua_recorder_id recorder_id;
  int recorder_active;
  int muted;
  float last_tx_level;
  sip_callbacks_t callbacks;
  void *callback_user_data;
} sip_client_t;

int sip_init(sip_client_t *client);
int sip_start(sip_client_t *client);
int sip_register_account(sip_client_t *client,
                         const char *domain,
                         const char *user,
                         const char *password,
                         const char *proxy,
                         const char *transport);
void sip_shutdown(sip_client_t *client);

void sip_set_callbacks(sip_client_t *client,
                       const sip_callbacks_t *callbacks,
                       void *user_data);

int sip_make_call(sip_client_t *client, const char *dest_uri);
int sip_hangup(sip_client_t *client);
int sip_hold(sip_client_t *client);
int sip_unhold(sip_client_t *client);
int sip_transfer_blind(sip_client_t *client, const char *dest_uri);
int sip_start_warm_transfer(sip_client_t *client, const char *dest_uri);
int sip_complete_warm_transfer(sip_client_t *client);
int sip_send_dtmf(sip_client_t *client, const char *digits);
int sip_start_recording(sip_client_t *client, const char *path);
int sip_stop_recording(sip_client_t *client);

int sip_list_audio_devices(sip_audio_device_t *devices, unsigned *count);
int sip_set_audio_devices(int capture_id, int playback_id);
int sip_set_volume_levels(float rx_level, float tx_level);
int sip_set_mute(sip_client_t *client, int muted);

#ifdef __cplusplus
}
#endif

#endif
