#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "app.h"
#include "ui.h"

static void main_on_incoming_call(void *user_data, pjsua_call_id call_id,
                                  const char *cname, const char *uri) {
  app_state_t *app = (app_state_t *)user_data;
  app_on_incoming_call(app, call_id, cname, uri);
}

static void main_on_call_state(void *user_data, pjsua_call_id call_id,
                               pjsip_inv_state state, const char *state_text) {
  app_state_t *app = (app_state_t *)user_data;
  app_on_call_state(app, call_id, state, state_text);
}

static void main_on_call_end(void *user_data, pjsua_call_id call_id,
                             int duration_sec, const char *direction,
                             const char *cname, const char *uri) {
  app_state_t *app = (app_state_t *)user_data;
  app_on_call_end(app, call_id, duration_sec, direction, cname, uri);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
  app_state_t app;
  sip_callbacks_t callbacks;
  int result;

  PJ_UNUSED_ARG(hPrevInstance);
  PJ_UNUSED_ARG(lpCmdLine);
  PJ_UNUSED_ARG(nCmdShow);

  if (app_initialize(&app, hInstance) != 0) {
    MessageBoxA(NULL, "Failed to initialize application", "Error", MB_OK);
    return -1;
  }
  if (ui_init(&app) != 0) {
    MessageBoxA(NULL, "Failed to initialize UI", "Error", MB_OK);
    app_shutdown(&app);
    return -1;
  }

  callbacks.on_incoming_call = main_on_incoming_call;
  callbacks.on_call_state = main_on_call_state;
  callbacks.on_call_end = main_on_call_end;
  sip_set_callbacks(&app.sip, &callbacks, &app);

  if (sip_start(&app.sip) != 0) {
    MessageBoxA(NULL, "Failed to start SIP stack", "Error", MB_OK);
    app_shutdown(&app);
    return -1;
  }

  if (sip_register_account(&app.sip,
                           app.config.sip_domain,
                           app.config.sip_user,
                           app.config.sip_password,
                           app.config.sip_proxy,
                           app.config.sip_transport) != 0) {
    MessageBoxA(NULL, "Failed to register SIP account", "Error", MB_OK);
  }

  sip_set_audio_devices(app.config.capture_device, app.config.playback_device);
  sip_set_volume_levels(app.config.rx_level, app.config.tx_level);
  app.sip.last_tx_level = app.config.tx_level;

  ui_set_status_text("Ready");
  result = ui_run();
  app_shutdown(&app);
  return result;
}
