#include "sip.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "storage.h"

typedef struct {
  pjsua_call_id call_id;
  time_t start_time;
  time_t confirmed_time;
  char remote_uri[256];
  char remote_cname[128];
  char direction[16];
} call_session_t;

static sip_client_t *g_client = NULL;
static call_session_t g_sessions[PJSUA_MAX_CALLS];

static void sip_clear_session(call_session_t *session) {
  session->call_id = PJSUA_INVALID_ID;
  session->start_time = 0;
  session->confirmed_time = 0;
  session->remote_uri[0] = '\0';
  session->remote_cname[0] = '\0';
  session->direction[0] = '\0';
}

static call_session_t *sip_get_session(pjsua_call_id call_id) {
  for (size_t i = 0; i < PJSUA_MAX_CALLS; i++) {
    if (g_sessions[i].call_id == call_id) {
      return &g_sessions[i];
    }
  }
  return NULL;
}

static call_session_t *sip_alloc_session(pjsua_call_id call_id) {
  call_session_t *session = sip_get_session(call_id);
  if (session) {
    return session;
  }
  for (size_t i = 0; i < PJSUA_MAX_CALLS; i++) {
    if (g_sessions[i].call_id == PJSUA_INVALID_ID) {
      g_sessions[i].call_id = call_id;
      g_sessions[i].start_time = time(NULL);
      return &g_sessions[i];
    }
  }
  return NULL;
}

static void sip_pj_str_to_cstr(char *out, size_t out_size, const pj_str_t *input) {
  size_t len;
  if (!out || out_size == 0) {
    return;
  }
  if (!input || !input->ptr || input->slen <= 0) {
    out[0] = '\0';
    return;
  }
  len = (size_t)input->slen;
  if (len >= out_size) {
    len = out_size - 1;
  }
  memcpy(out, input->ptr, len);
  out[len] = '\0';
}

static void sip_extract_remote_info(const pjsua_call_info *info,
                                    char *out_cname, size_t cname_size,
                                    char *out_uri, size_t uri_size) {
  char remote_info[256];
  const char *name_start;
  const char *name_end;
  const char *uri_start;
  const char *uri_end;

  if (!info) {
    if (out_cname) {
      out_cname[0] = '\0';
    }
    if (out_uri) {
      out_uri[0] = '\0';
    }
    return;
  }

  sip_pj_str_to_cstr(remote_info, sizeof(remote_info), &info->remote_info);
  name_start = strchr(remote_info, '"');
  if (name_start) {
    name_start++;
    name_end = strchr(name_start, '"');
    if (name_end && out_cname) {
      size_t len = (size_t)(name_end - name_start);
      if (len >= cname_size) {
        len = cname_size - 1;
      }
      memcpy(out_cname, name_start, len);
      out_cname[len] = '\0';
    }
  } else if (out_cname) {
    name_end = strchr(remote_info, '<');
    if (!name_end) {
      name_end = remote_info + strlen(remote_info);
    }
    if (name_end > remote_info) {
      size_t len = (size_t)(name_end - remote_info);
      if (len >= cname_size) {
        len = cname_size - 1;
      }
      memcpy(out_cname, remote_info, len);
      out_cname[len] = '\0';
    } else {
      out_cname[0] = '\0';
    }
  }

  uri_start = strchr(remote_info, '<');
  uri_end = strchr(remote_info, '>');
  if (uri_start && uri_end && uri_end > uri_start + 1) {
    uri_start++;
    if (out_uri) {
      size_t len = (size_t)(uri_end - uri_start);
      if (len >= uri_size) {
        len = uri_size - 1;
      }
      memcpy(out_uri, uri_start, len);
      out_uri[len] = '\0';
    }
  } else if (out_uri) {
    strncpy(out_uri, remote_info, uri_size - 1);
    out_uri[uri_size - 1] = '\0';
  }
}

static void sip_on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
                                 pjsip_rx_data *rdata) {
  pjsua_call_info info;
  call_session_t *session;
  char cname[128];
  char uri[256];

  PJ_UNUSED_ARG(acc_id);
  PJ_UNUSED_ARG(rdata);

  pjsua_call_get_info(call_id, &info);
  sip_extract_remote_info(&info, cname, sizeof(cname), uri, sizeof(uri));

  session = sip_alloc_session(call_id);
  if (session) {
    strncpy(session->remote_cname, cname, sizeof(session->remote_cname) - 1);
    session->remote_cname[sizeof(session->remote_cname) - 1] = '\0';
    strncpy(session->remote_uri, uri, sizeof(session->remote_uri) - 1);
    session->remote_uri[sizeof(session->remote_uri) - 1] = '\0';
    strcpy(session->direction, "incoming");
  }

  if (g_client && g_client->active_call_id == PJSUA_INVALID_ID) {
    g_client->active_call_id = call_id;
  }
  if (g_client && g_client->callbacks.on_incoming_call) {
    g_client->callbacks.on_incoming_call(g_client->callback_user_data,
                                         call_id, cname, uri);
  }
}

static void sip_on_call_state(pjsua_call_id call_id, pjsip_event *e) {
  pjsua_call_info info;
  call_session_t *session;
  time_t end_time;
  int duration_sec = 0;
  char cname[128];
  char uri[256];

  PJ_UNUSED_ARG(e);

  pjsua_call_get_info(call_id, &info);
  sip_extract_remote_info(&info, cname, sizeof(cname), uri, sizeof(uri));

  session = sip_get_session(call_id);
  if (!session) {
    session = sip_alloc_session(call_id);
  }
  if (session) {
    if (session->remote_uri[0] == '\0') {
      strncpy(session->remote_uri, uri, sizeof(session->remote_uri) - 1);
      session->remote_uri[sizeof(session->remote_uri) - 1] = '\0';
    }
    if (session->remote_cname[0] == '\0') {
      strncpy(session->remote_cname, cname, sizeof(session->remote_cname) - 1);
      session->remote_cname[sizeof(session->remote_cname) - 1] = '\0';
    }
  }

  if (info.state == PJSIP_INV_STATE_CONFIRMED && session) {
    session->confirmed_time = time(NULL);
  }

  if (g_client && g_client->callbacks.on_call_state) {
    char state_text[64];
    sip_pj_str_to_cstr(state_text, sizeof(state_text), &info.state_text);
    g_client->callbacks.on_call_state(g_client->callback_user_data,
                                      call_id, info.state, state_text);
  }

  if (info.state == PJSIP_INV_STATE_DISCONNECTED) {
    if (session) {
      end_time = time(NULL);
      if (session->confirmed_time != 0) {
        duration_sec = (int)difftime(end_time, session->confirmed_time);
      } else if (session->start_time != 0) {
        duration_sec = (int)difftime(end_time, session->start_time);
      }
      if (g_client && g_client->callbacks.on_call_end) {
        g_client->callbacks.on_call_end(g_client->callback_user_data,
                                        call_id,
                                        duration_sec,
                                        session->direction,
                                        session->remote_cname,
                                        session->remote_uri);
      }
    }
    if (g_client && g_client->active_call_id == call_id) {
      g_client->active_call_id = PJSUA_INVALID_ID;
    }
    if (g_client && g_client->warm_transfer_call_id == call_id) {
      g_client->warm_transfer_call_id = PJSUA_INVALID_ID;
    }
    if (session) {
      sip_clear_session(session);
    }
  }
}

static void sip_on_call_media_state(pjsua_call_id call_id) {
  pjsua_call_info info;
  PJ_UNUSED_ARG(call_id);
  pjsua_call_get_info(call_id, &info);
  if (info.media_status == PJSUA_CALL_MEDIA_ACTIVE) {
    pjsua_conf_connect(info.conf_slot, 0);
    pjsua_conf_connect(0, info.conf_slot);
  }
}

static void sip_configure_codecs(void) {
  pj_str_t codec_g729 = pj_str("G729/8000/1");
  pj_str_t codec_pcmu = pj_str("PCMU/8000/1");
  pj_str_t codec_pcma = pj_str("PCMA/8000/1");
  pjsua_codec_set_priority(&codec_g729, PJMEDIA_CODEC_PRIO_HIGHEST);
  pjsua_codec_set_priority(&codec_pcmu, PJMEDIA_CODEC_PRIO_HIGHEST);
  pjsua_codec_set_priority(&codec_pcma, PJMEDIA_CODEC_PRIO_HIGHEST);
}

int sip_init(sip_client_t *client) {
  pjsua_config cfg;
  pjsua_logging_config log_cfg;
  pjsua_media_config media_cfg;
  pj_status_t status;

  if (!client) {
    return -1;
  }
  memset(client, 0, sizeof(*client));
  client->acc_id = PJSUA_INVALID_ID;
  client->active_call_id = PJSUA_INVALID_ID;
  client->warm_transfer_call_id = PJSUA_INVALID_ID;
  client->recorder_id = PJSUA_INVALID_ID;
  client->recorder_active = 0;
  client->muted = 0;
  client->last_tx_level = 1.0f;

  for (size_t i = 0; i < PJSUA_MAX_CALLS; i++) {
    sip_clear_session(&g_sessions[i]);
  }

  pjsua_config_default(&cfg);
  cfg.cb.on_incoming_call = &sip_on_incoming_call;
  cfg.cb.on_call_state = &sip_on_call_state;
  cfg.cb.on_call_media_state = &sip_on_call_media_state;

  pjsua_logging_config_default(&log_cfg);
  log_cfg.console_level = 4;

  pjsua_media_config_default(&media_cfg);
  media_cfg.ec_options = PJMEDIA_ECHO_SIMPLE;
  media_cfg.ec_tail_len = 200;

  status = pjsua_create();
  if (status != PJ_SUCCESS) {
    return -1;
  }
  status = pjsua_init(&cfg, &log_cfg, &media_cfg);
  if (status != PJ_SUCCESS) {
    return -1;
  }
  sip_configure_codecs();
  g_client = client;
  return 0;
}

int sip_start(sip_client_t *client) {
  pj_status_t status;
  PJ_UNUSED_ARG(client);
  status = pjsua_start();
  return status == PJ_SUCCESS ? 0 : -1;
}

int sip_register_account(sip_client_t *client,
                         const char *domain,
                         const char *user,
                         const char *password,
                         const char *proxy,
                         const char *transport) {
  pjsua_acc_config acc_cfg;
  char id[256];
  char reg_uri[256];
  pj_status_t status;

  if (!client || !domain || !user) {
    return -1;
  }

  pjsua_acc_config_default(&acc_cfg);
  snprintf(id, sizeof(id), "sip:%s@%s", user, domain);
  snprintf(reg_uri, sizeof(reg_uri), "sip:%s", domain);
  acc_cfg.id = pj_str(id);
  acc_cfg.reg_uri = pj_str(reg_uri);
  acc_cfg.cred_count = 1;
  acc_cfg.cred_info[0].realm = pj_str("*");
  acc_cfg.cred_info[0].scheme = pj_str("digest");
  acc_cfg.cred_info[0].username = pj_str((char *)user);
  acc_cfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
  acc_cfg.cred_info[0].data = pj_str((char *)password);

  if (proxy && proxy[0] != '\0') {
    acc_cfg.proxy_cnt = 1;
    acc_cfg.proxy[0] = pj_str((char *)proxy);
  }

  if (transport && (_stricmp(transport, "tcp") == 0 ||
                    _stricmp(transport, "tls") == 0)) {
    pjsua_transport_id transport_id;
    pjsua_transport_config transport_cfg;
    pjsip_transport_type_e transport_type =
        (_stricmp(transport, "tls") == 0) ? PJSIP_TRANSPORT_TLS
                                          : PJSIP_TRANSPORT_TCP;
    pjsua_transport_config_default(&transport_cfg);
    status = pjsua_transport_create(transport_type, &transport_cfg, &transport_id);
    if (status == PJ_SUCCESS) {
      acc_cfg.transport_id = transport_id;
    }
  }

  status = pjsua_acc_add(&acc_cfg, PJ_TRUE, &client->acc_id);
  return status == PJ_SUCCESS ? 0 : -1;
}

void sip_shutdown(sip_client_t *client) {
  PJ_UNUSED_ARG(client);
  if (pjsua_get_state() != PJSUA_STATE_NULL) {
    pjsua_destroy();
  }
  g_client = NULL;
}

void sip_set_callbacks(sip_client_t *client,
                       const sip_callbacks_t *callbacks,
                       void *user_data) {
  if (!client) {
    return;
  }
  if (callbacks) {
    client->callbacks = *callbacks;
  } else {
    memset(&client->callbacks, 0, sizeof(client->callbacks));
  }
  client->callback_user_data = user_data;
}

int sip_make_call(sip_client_t *client, const char *dest_uri) {
  pj_status_t status;
  pj_str_t uri;
  if (!client || !dest_uri || client->acc_id == PJSUA_INVALID_ID) {
    return -1;
  }
  uri = pj_str((char *)dest_uri);
  status = pjsua_call_make_call(client->acc_id, &uri, 0, NULL, NULL,
                                &client->active_call_id);
  if (status == PJ_SUCCESS) {
    call_session_t *session = sip_alloc_session(client->active_call_id);
    if (session) {
      strcpy(session->direction, "outgoing");
    }
    return 0;
  }
  return -1;
}

int sip_hangup(sip_client_t *client) {
  if (!client || client->active_call_id == PJSUA_INVALID_ID) {
    return -1;
  }
  return pjsua_call_hangup(client->active_call_id, 0, NULL, NULL) == PJ_SUCCESS
             ? 0
             : -1;
}

int sip_hold(sip_client_t *client) {
  if (!client || client->active_call_id == PJSUA_INVALID_ID) {
    return -1;
  }
  return pjsua_call_set_hold(client->active_call_id, NULL) == PJ_SUCCESS
             ? 0
             : -1;
}

int sip_unhold(sip_client_t *client) {
  if (!client || client->active_call_id == PJSUA_INVALID_ID) {
    return -1;
  }
  return pjsua_call_reinvite(client->active_call_id, PJ_TRUE, NULL) == PJ_SUCCESS
             ? 0
             : -1;
}

int sip_transfer_blind(sip_client_t *client, const char *dest_uri) {
  pj_str_t uri;
  if (!client || client->active_call_id == PJSUA_INVALID_ID || !dest_uri) {
    return -1;
  }
  uri = pj_str((char *)dest_uri);
  return pjsua_call_xfer(client->active_call_id, &uri, NULL) == PJ_SUCCESS
             ? 0
             : -1;
}

int sip_start_warm_transfer(sip_client_t *client, const char *dest_uri) {
  pj_str_t uri;
  pj_status_t status;
  if (!client || client->active_call_id == PJSUA_INVALID_ID || !dest_uri) {
    return -1;
  }
  pjsua_call_set_hold(client->active_call_id, NULL);
  uri = pj_str((char *)dest_uri);
  status = pjsua_call_make_call(client->acc_id, &uri, 0, NULL, NULL,
                                &client->warm_transfer_call_id);
  return status == PJ_SUCCESS ? 0 : -1;
}

int sip_complete_warm_transfer(sip_client_t *client) {
  if (!client || client->active_call_id == PJSUA_INVALID_ID ||
      client->warm_transfer_call_id == PJSUA_INVALID_ID) {
    return -1;
  }
  return pjsua_call_xfer_replaces(client->active_call_id,
                                  client->warm_transfer_call_id, 0, NULL) == PJ_SUCCESS
             ? 0
             : -1;
}

int sip_send_dtmf(sip_client_t *client, const char *digits) {
  pj_str_t dtmf;
  if (!client || client->active_call_id == PJSUA_INVALID_ID || !digits) {
    return -1;
  }
  dtmf = pj_str((char *)digits);
  return pjsua_call_dial_dtmf(client->active_call_id, &dtmf) == PJ_SUCCESS
             ? 0
             : -1;
}

int sip_start_recording(sip_client_t *client, const char *path) {
  pj_status_t status;
  pjsua_call_info info;
  pj_str_t file_name;
  pjsua_conf_port_id rec_slot;

  if (!client || client->active_call_id == PJSUA_INVALID_ID || !path) {
    return -1;
  }
  if (client->recorder_active) {
    return 0;
  }

  file_name = pj_str((char *)path);
  status = pjsua_recorder_create(&file_name, 0, NULL, 0, 0,
                                 &client->recorder_id);
  if (status != PJ_SUCCESS) {
    return -1;
  }
  pjsua_call_get_info(client->active_call_id, &info);
  rec_slot = pjsua_recorder_get_conf_port(client->recorder_id);
  pjsua_conf_connect(info.conf_slot, rec_slot);
  client->recorder_active = 1;
  return 0;
}

int sip_stop_recording(sip_client_t *client) {
  if (!client || !client->recorder_active) {
    return -1;
  }
  pjsua_recorder_destroy(client->recorder_id);
  client->recorder_id = PJSUA_INVALID_ID;
  client->recorder_active = 0;
  return 0;
}

int sip_list_audio_devices(sip_audio_device_t *devices, unsigned *count) {
  unsigned dev_count = 0;
  pjmedia_aud_dev_info info[64];
  pj_status_t status;
  if (!count) {
    return -1;
  }
  status = pjsua_enum_aud_devs(info, &dev_count);
  if (status != PJ_SUCCESS) {
    return -1;
  }
  if (!devices) {
    *count = dev_count;
    return 0;
  }
  if (*count < dev_count) {
    dev_count = *count;
  }
  for (unsigned i = 0; i < dev_count; i++) {
    devices[i].id = info[i].id;
    devices[i].input_count = info[i].input_count;
    devices[i].output_count = info[i].output_count;
    snprintf(devices[i].name, sizeof(devices[i].name), "%s", info[i].name);
  }
  *count = dev_count;
  return 0;
}

int sip_set_audio_devices(int capture_id, int playback_id) {
  return pjsua_set_snd_dev(capture_id, playback_id) == PJ_SUCCESS ? 0 : -1;
}

int sip_set_volume_levels(float rx_level, float tx_level) {
  pjsua_conf_adjust_rx_level(0, rx_level);
  pjsua_conf_adjust_tx_level(0, tx_level);
  return 0;
}

int sip_set_mute(sip_client_t *client, int muted) {
  if (!client) {
    return -1;
  }
  if (muted && !client->muted) {
    client->muted = 1;
    pjsua_conf_adjust_tx_level(0, 0.0f);
  } else if (!muted && client->muted) {
    client->muted = 0;
    pjsua_conf_adjust_tx_level(0, client->last_tx_level);
  }
  return 0;
}
