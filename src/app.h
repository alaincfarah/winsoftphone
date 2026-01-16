#ifndef APP_H
#define APP_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "config.h"
#include "contacts.h"
#include "history.h"
#include "sip.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  HINSTANCE instance;
  HWND main_hwnd;
  app_config_t config;
  contact_list_t contacts;
  history_list_t history;
  sip_client_t sip;
  int call_on_hold;
  int recording;
  int muted;
  int shutdown_called;
} app_state_t;

int app_initialize(app_state_t *app, HINSTANCE instance);
void app_shutdown(app_state_t *app);
int app_build_incoming_url(app_state_t *app, const char *cname,
                           char *out_url, size_t out_size);
int app_build_recording_path(app_state_t *app, char *out_path, size_t out_size);

void app_on_incoming_call(app_state_t *app, pjsua_call_id call_id,
                          const char *cname, const char *uri);
void app_on_call_state(app_state_t *app, pjsua_call_id call_id,
                       pjsip_inv_state state, const char *state_text);
void app_on_call_end(app_state_t *app, pjsua_call_id call_id,
                     int duration_sec, const char *direction,
                     const char *cname, const char *uri);
void app_on_reg_state(app_state_t *app, int reg_status,
                      const char *status_text, int is_active);

#ifdef __cplusplus
}
#endif

#endif
