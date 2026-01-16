#include "app.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "storage.h"
#include "ui.h"

static int app_url_encode(const char *input, char *out, size_t out_size) {
  const char *hex = "0123456789ABCDEF";
  size_t pos = 0;
  for (size_t i = 0; input[i] != '\0'; i++) {
    unsigned char c = (unsigned char)input[i];
    if ((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~') {
      if (pos + 1 >= out_size) {
        return -1;
      }
      out[pos++] = (char)c;
    } else {
      if (pos + 3 >= out_size) {
        return -1;
      }
      out[pos++] = '%';
      out[pos++] = hex[(c >> 4) & 0x0F];
      out[pos++] = hex[c & 0x0F];
    }
  }
  if (pos >= out_size) {
    return -1;
  }
  out[pos] = '\0';
  return 0;
}

int app_initialize(app_state_t *app, HINSTANCE instance) {
  if (!app) {
    return -1;
  }
  memset(app, 0, sizeof(*app));
  app->instance = instance;
  app->main_hwnd = NULL;
  app->call_on_hold = 0;
  app->recording = 0;
  app->muted = 0;

  if (config_load(&app->config) != 0) {
    return -1;
  }
  contacts_load(&app->contacts);
  history_load(&app->history);

  if (sip_init(&app->sip) != 0) {
    return -1;
  }
  return 0;
}

void app_shutdown(app_state_t *app) {
  if (!app) {
    return;
  }
  sip_shutdown(&app->sip);
  contacts_free(&app->contacts);
  history_free(&app->history);
}

int app_build_incoming_url(app_state_t *app, const char *cname,
                           char *out_url, size_t out_size) {
  char encoded_cname[256];
  const contact_t *contact;
  const char *template_str;
  const char *token = "{cname}";
  const char *token_pos;

  if (!app || !cname || cname[0] == '\0' || !out_url || out_size == 0) {
    return -1;
  }
  contact = contacts_find_by_cname(&app->contacts, cname);
  if (contact && contact->url[0] != '\0') {
    strncpy(out_url, contact->url, out_size - 1);
    out_url[out_size - 1] = '\0';
    return 0;
  }

  if (app_url_encode(cname, encoded_cname, sizeof(encoded_cname)) != 0) {
    return -1;
  }
  template_str = app->config.incoming_url_template;
  token_pos = strstr(template_str, token);
  if (!token_pos) {
    strncpy(out_url, template_str, out_size - 1);
    out_url[out_size - 1] = '\0';
    return 0;
  }

  {
    size_t prefix_len = (size_t)(token_pos - template_str);
    size_t suffix_len = strlen(token_pos + strlen(token));
    if (prefix_len + strlen(encoded_cname) + suffix_len + 1 > out_size) {
      return -1;
    }
    memcpy(out_url, template_str, prefix_len);
    memcpy(out_url + prefix_len, encoded_cname, strlen(encoded_cname));
    memcpy(out_url + prefix_len + strlen(encoded_cname),
           token_pos + strlen(token), suffix_len);
    out_url[prefix_len + strlen(encoded_cname) + suffix_len] = '\0';
  }
  return 0;
}

int app_build_recording_path(app_state_t *app, char *out_path, size_t out_size) {
  char appdata[MAX_PATH];
  char recordings_dir[MAX_PATH];
  char filename[64];
  time_t now = time(NULL);
  struct tm local_time;

  if (!app || !out_path || out_size == 0) {
    return -1;
  }
  if (storage_get_appdata_path(appdata, sizeof(appdata)) != 0) {
    return -1;
  }
  if (storage_join_path(appdata, "recordings",
                        recordings_dir, sizeof(recordings_dir)) != 0) {
    return -1;
  }
  if (storage_ensure_dir_recursive(recordings_dir) != 0) {
    return -1;
  }
  localtime_s(&local_time, &now);
  strftime(filename, sizeof(filename), "call-%Y%m%d-%H%M%S.wav", &local_time);
  return storage_join_path(recordings_dir, filename, out_path, out_size);
}

void app_on_incoming_call(app_state_t *app, pjsua_call_id call_id,
                          const char *cname, const char *uri) {
  char status[256];
  char url[512];
  PJ_UNUSED_ARG(call_id);

  if (!app) {
    return;
  }
  if (cname && cname[0] != '\0') {
    snprintf(status, sizeof(status), "Incoming call from %s", cname);
  } else if (uri && uri[0] != '\0') {
    snprintf(status, sizeof(status), "Incoming call from %s", uri);
  } else {
    snprintf(status, sizeof(status), "Incoming call");
  }
  ui_set_status_text(status);

  if (cname && app_build_incoming_url(app, cname, url, sizeof(url)) == 0) {
    ui_open_url(url);
  }
}

void app_on_call_state(app_state_t *app, pjsua_call_id call_id,
                       pjsip_inv_state state, const char *state_text) {
  char status[256];
  PJ_UNUSED_ARG(call_id);

  if (!app) {
    return;
  }
  snprintf(status, sizeof(status), "Call state: %s", state_text ? state_text : "");
  ui_set_status_text(status);

  if (state == PJSIP_INV_STATE_DISCONNECTED) {
    app->call_on_hold = 0;
  }
}

void app_on_call_end(app_state_t *app, pjsua_call_id call_id,
                     int duration_sec, const char *direction,
                     const char *cname, const char *uri) {
  history_entry_t entry;
  time_t now = time(NULL);
  struct tm local_time;
  char timestamp[32];

  PJ_UNUSED_ARG(call_id);

  if (!app) {
    return;
  }
  localtime_s(&local_time, &now);
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &local_time);
  memset(&entry, 0, sizeof(entry));
  strncpy(entry.number, uri ? uri : "", sizeof(entry.number) - 1);
  strncpy(entry.cname, cname ? cname : "", sizeof(entry.cname) - 1);
  strncpy(entry.direction, direction ? direction : "unknown",
          sizeof(entry.direction) - 1);
  strncpy(entry.timestamp, timestamp, sizeof(entry.timestamp) - 1);
  entry.duration_sec = duration_sec;
  history_add(&app->history, &entry);
  ui_set_status_text("Call ended");
}
