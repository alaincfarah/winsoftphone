#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "json_utils.h"
#include "jsmn.h"
#include "storage.h"

#define CONFIG_FILE_NAME "config.json"
#define DEFAULT_CONFIG_RELATIVE "config\\default_config.json"

static int config_file_exists(const char *path) {
  DWORD attrs = GetFileAttributesA(path);
  return (attrs != INVALID_FILE_ATTRIBUTES &&
          (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

static int config_write_defaults_to_path(const char *path,
                                         const app_config_t *config) {
  char escaped_url[512];
  char output[2048];
  if (json_escape_string(config->incoming_url_template,
                         escaped_url, sizeof(escaped_url)) != 0) {
    return -1;
  }
  snprintf(output, sizeof(output),
           "{\n"
           "  \"sip\": {\n"
           "    \"domain\": \"%s\",\n"
           "    \"user\": \"%s\",\n"
           "    \"password\": \"%s\",\n"
           "    \"proxy\": \"%s\",\n"
           "    \"transport\": \"%s\"\n"
           "  },\n"
           "  \"audio\": {\n"
           "    \"capture_device\": %d,\n"
           "    \"playback_device\": %d,\n"
           "    \"rx_level\": %.2f,\n"
           "    \"tx_level\": %.2f\n"
           "  },\n"
           "  \"ui\": {\n"
           "    \"incoming_url_template\": \"%s\"\n"
           "  }\n"
           "}\n",
           config->sip_domain,
           config->sip_user,
           config->sip_password,
           config->sip_proxy,
           config->sip_transport,
           config->capture_device,
           config->playback_device,
           config->rx_level,
           config->tx_level,
           escaped_url);
  return storage_write_file(path, output);
}

void config_set_defaults(app_config_t *config) {
  if (!config) {
    return;
  }
  memset(config, 0, sizeof(*config));
  strcpy(config->sip_domain, "pbx.example.com");
  strcpy(config->sip_user, "1001");
  strcpy(config->sip_password, "changeme");
  strcpy(config->sip_proxy, "sip:pbx.example.com");
  strcpy(config->sip_transport, "udp");
  config->capture_device = -1;
  config->playback_device = -1;
  config->rx_level = 1.0f;
  config->tx_level = 1.0f;
  strcpy(config->incoming_url_template,
         "https://crm.example.com/lookup?cname={cname}");
}

int config_get_path(char *out_path, size_t out_size) {
  char appdata[MAX_PATH];
  if (storage_get_appdata_path(appdata, sizeof(appdata)) != 0) {
    return -1;
  }
  return storage_join_path(appdata, CONFIG_FILE_NAME, out_path, out_size);
}

static int config_copy_default(const char *destination_path) {
  char exe_dir[MAX_PATH];
  char default_path[MAX_PATH];
  char *data = NULL;
  size_t data_size = 0;
  if (storage_get_exe_dir(exe_dir, sizeof(exe_dir)) != 0) {
    return -1;
  }
  if (storage_join_path(exe_dir, DEFAULT_CONFIG_RELATIVE,
                        default_path, sizeof(default_path)) != 0) {
    return -1;
  }
  if (storage_read_file(default_path, &data, &data_size) != 0) {
    return -1;
  }
  if (storage_write_file(destination_path, data) != 0) {
    free(data);
    return -1;
  }
  free(data);
  return 0;
}

static int config_parse_json(app_config_t *config, const char *json,
                             jsmntok_t *tokens, int token_count) {
  int root = 0;
  int sip_index;
  int audio_index;
  int ui_index;

  if (token_count <= 0 || tokens[root].type != JSMN_OBJECT) {
    return -1;
  }

  sip_index = json_find_key(json, tokens, token_count, root, "sip");
  if (sip_index >= 0 && tokens[sip_index].type == JSMN_OBJECT) {
    int value_index;
    value_index = json_find_key(json, tokens, token_count, sip_index, "domain");
    if (value_index >= 0) {
      json_token_copy(json, &tokens[value_index],
                      config->sip_domain, sizeof(config->sip_domain));
    }
    value_index = json_find_key(json, tokens, token_count, sip_index, "user");
    if (value_index >= 0) {
      json_token_copy(json, &tokens[value_index],
                      config->sip_user, sizeof(config->sip_user));
    }
    value_index = json_find_key(json, tokens, token_count, sip_index, "password");
    if (value_index >= 0) {
      json_token_copy(json, &tokens[value_index],
                      config->sip_password, sizeof(config->sip_password));
    }
    value_index = json_find_key(json, tokens, token_count, sip_index, "proxy");
    if (value_index >= 0) {
      json_token_copy(json, &tokens[value_index],
                      config->sip_proxy, sizeof(config->sip_proxy));
    }
    value_index = json_find_key(json, tokens, token_count, sip_index, "transport");
    if (value_index >= 0) {
      json_token_copy(json, &tokens[value_index],
                      config->sip_transport, sizeof(config->sip_transport));
    }
  }

  audio_index = json_find_key(json, tokens, token_count, root, "audio");
  if (audio_index >= 0 && tokens[audio_index].type == JSMN_OBJECT) {
    int value_index;
    value_index = json_find_key(json, tokens, token_count,
                                audio_index, "capture_device");
    if (value_index >= 0) {
      json_token_to_int(json, &tokens[value_index], &config->capture_device);
    }
    value_index = json_find_key(json, tokens, token_count,
                                audio_index, "playback_device");
    if (value_index >= 0) {
      json_token_to_int(json, &tokens[value_index], &config->playback_device);
    }
    value_index = json_find_key(json, tokens, token_count,
                                audio_index, "rx_level");
    if (value_index >= 0) {
      json_token_to_float(json, &tokens[value_index], &config->rx_level);
    }
    value_index = json_find_key(json, tokens, token_count,
                                audio_index, "tx_level");
    if (value_index >= 0) {
      json_token_to_float(json, &tokens[value_index], &config->tx_level);
    }
  }

  ui_index = json_find_key(json, tokens, token_count, root, "ui");
  if (ui_index >= 0 && tokens[ui_index].type == JSMN_OBJECT) {
    int value_index = json_find_key(json, tokens, token_count,
                                    ui_index, "incoming_url_template");
    if (value_index >= 0) {
      json_token_copy(json, &tokens[value_index],
                      config->incoming_url_template,
                      sizeof(config->incoming_url_template));
    }
  }
  return 0;
}

int config_load(app_config_t *config) {
  char config_path[MAX_PATH];
  char appdata[MAX_PATH];
  char *json = NULL;
  size_t json_size = 0;
  jsmn_parser parser;
  jsmntok_t tokens[256];
  int token_count;

  if (!config) {
    return -1;
  }
  config_set_defaults(config);

  if (storage_get_appdata_path(appdata, sizeof(appdata)) != 0) {
    return -1;
  }
  if (storage_ensure_dir_recursive(appdata) != 0) {
    return -1;
  }
  if (config_get_path(config_path, sizeof(config_path)) != 0) {
    return -1;
  }

  if (!config_file_exists(config_path)) {
    if (config_copy_default(config_path) != 0) {
      if (config_write_defaults_to_path(config_path, config) != 0) {
        return -1;
      }
    }
  }

  if (storage_read_file(config_path, &json, &json_size) != 0) {
    return -1;
  }
  jsmn_init(&parser);
  token_count = jsmn_parse(&parser, json, (unsigned int)json_size,
                           tokens, (unsigned int)(sizeof(tokens) / sizeof(tokens[0])));
  if (token_count > 0) {
    config_parse_json(config, json, tokens, token_count);
  }
  free(json);
  return 0;
}

int config_save(const app_config_t *config) {
  char config_path[MAX_PATH];
  char appdata[MAX_PATH];
  if (!config) {
    return -1;
  }
  if (storage_get_appdata_path(appdata, sizeof(appdata)) != 0) {
    return -1;
  }
  if (storage_ensure_dir_recursive(appdata) != 0) {
    return -1;
  }
  if (config_get_path(config_path, sizeof(config_path)) != 0) {
    return -1;
  }
  return config_write_defaults_to_path(config_path, config);
}
