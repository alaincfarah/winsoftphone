#include "history.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "json_utils.h"
#include "jsmn.h"
#include "storage.h"

#define HISTORY_FILE_NAME "history.json"
#define DEFAULT_HISTORY_RELATIVE "config\\default_history.json"

static int history_file_exists(const char *path) {
  DWORD attrs = GetFileAttributesA(path);
  return (attrs != INVALID_FILE_ATTRIBUTES &&
          (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

static int history_get_path(char *out_path, size_t out_size) {
  char appdata[MAX_PATH];
  if (storage_get_appdata_path(appdata, sizeof(appdata)) != 0) {
    return -1;
  }
  return storage_join_path(appdata, HISTORY_FILE_NAME, out_path, out_size);
}

static int history_copy_default(const char *destination_path) {
  char exe_dir[MAX_PATH];
  char default_path[MAX_PATH];
  char *data = NULL;
  size_t data_size = 0;
  if (storage_get_exe_dir(exe_dir, sizeof(exe_dir)) != 0) {
    return -1;
  }
  if (storage_join_path(exe_dir, DEFAULT_HISTORY_RELATIVE,
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

static int json_token_span(jsmntok_t *tokens, int index) {
  int span = 1;
  int i;
  if (tokens[index].type == JSMN_OBJECT) {
    int count = tokens[index].size;
    i = index + 1;
    for (int j = 0; j < count; j++) {
      int key_span = json_token_span(tokens, i);
      i += key_span;
      span += key_span;
      int val_span = json_token_span(tokens, i);
      i += val_span;
      span += val_span;
    }
  } else if (tokens[index].type == JSMN_ARRAY) {
    int count = tokens[index].size;
    i = index + 1;
    for (int j = 0; j < count; j++) {
      int elem_span = json_token_span(tokens, i);
      i += elem_span;
      span += elem_span;
    }
  }
  return span;
}

static void history_init_empty(history_list_t *history) {
  history->items = NULL;
  history->count = 0;
}

static int history_append(history_list_t *history,
                          const history_entry_t *entry) {
  history_entry_t *new_items = (history_entry_t *)realloc(
      history->items, sizeof(history_entry_t) * (history->count + 1));
  if (!new_items) {
    return -1;
  }
  history->items = new_items;
  history->items[history->count] = *entry;
  history->count++;
  return 0;
}

int history_load(history_list_t *history) {
  char history_path[MAX_PATH];
  char appdata[MAX_PATH];
  char *json = NULL;
  size_t json_size = 0;
  jsmn_parser parser;
  jsmntok_t tokens[512];
  int token_count;
  int index;

  if (!history) {
    return -1;
  }
  history_init_empty(history);

  if (storage_get_appdata_path(appdata, sizeof(appdata)) != 0) {
    return -1;
  }
  if (storage_ensure_dir_recursive(appdata) != 0) {
    return -1;
  }
  if (history_get_path(history_path, sizeof(history_path)) != 0) {
    return -1;
  }
  if (!history_file_exists(history_path)) {
    history_copy_default(history_path);
  }

  if (storage_read_file(history_path, &json, &json_size) != 0) {
    return 0;
  }

  jsmn_init(&parser);
  token_count = jsmn_parse(&parser, json, (unsigned int)json_size,
                           tokens, (unsigned int)(sizeof(tokens) / sizeof(tokens[0])));
  if (token_count <= 0 || tokens[0].type != JSMN_ARRAY) {
    free(json);
    return 0;
  }

  index = 1;
  for (int i = 0; i < tokens[0].size; i++) {
    if (tokens[index].type == JSMN_OBJECT) {
      history_entry_t entry;
      int value_index;
      memset(&entry, 0, sizeof(entry));
      value_index = json_find_key(json, tokens, token_count, index, "number");
      if (value_index >= 0) {
        json_token_copy(json, &tokens[value_index], entry.number, sizeof(entry.number));
      }
      value_index = json_find_key(json, tokens, token_count, index, "cname");
      if (value_index >= 0) {
        json_token_copy(json, &tokens[value_index], entry.cname, sizeof(entry.cname));
      }
      value_index = json_find_key(json, tokens, token_count, index, "direction");
      if (value_index >= 0) {
        json_token_copy(json, &tokens[value_index], entry.direction, sizeof(entry.direction));
      }
      value_index = json_find_key(json, tokens, token_count, index, "timestamp");
      if (value_index >= 0) {
        json_token_copy(json, &tokens[value_index], entry.timestamp, sizeof(entry.timestamp));
      }
      value_index = json_find_key(json, tokens, token_count, index, "duration_sec");
      if (value_index >= 0) {
        json_token_to_int(json, &tokens[value_index], &entry.duration_sec);
      }
      history_append(history, &entry);
    }
    index += json_token_span(tokens, index);
  }

  free(json);
  return 0;
}

int history_save(const history_list_t *history) {
  char history_path[MAX_PATH];
  char appdata[MAX_PATH];
  FILE *file;

  if (!history) {
    return -1;
  }
  if (storage_get_appdata_path(appdata, sizeof(appdata)) != 0) {
    return -1;
  }
  if (storage_ensure_dir_recursive(appdata) != 0) {
    return -1;
  }
  if (history_get_path(history_path, sizeof(history_path)) != 0) {
    return -1;
  }

  file = fopen(history_path, "wb");
  if (!file) {
    return -1;
  }

  fprintf(file, "[\n");
  for (size_t i = 0; i < history->count; i++) {
    char escaped_number[128];
    char escaped_cname[256];
    char escaped_direction[32];
    char escaped_timestamp[64];
    const history_entry_t *entry = &history->items[i];
    json_escape_string(entry->number, escaped_number, sizeof(escaped_number));
    json_escape_string(entry->cname, escaped_cname, sizeof(escaped_cname));
    json_escape_string(entry->direction, escaped_direction, sizeof(escaped_direction));
    json_escape_string(entry->timestamp, escaped_timestamp, sizeof(escaped_timestamp));
    fprintf(file,
            "  {\n"
            "    \"number\": \"%s\",\n"
            "    \"cname\": \"%s\",\n"
            "    \"direction\": \"%s\",\n"
            "    \"timestamp\": \"%s\",\n"
            "    \"duration_sec\": %d\n"
            "  }%s\n",
            escaped_number,
            escaped_cname,
            escaped_direction,
            escaped_timestamp,
            entry->duration_sec,
            (i + 1 < history->count) ? "," : "");
  }
  fprintf(file, "]\n");
  fclose(file);
  return 0;
}

void history_free(history_list_t *history) {
  if (!history) {
    return;
  }
  free(history->items);
  history->items = NULL;
  history->count = 0;
}

int history_add(history_list_t *history, const history_entry_t *entry) {
  if (!history || !entry) {
    return -1;
  }
  if (history_append(history, entry) != 0) {
    return -1;
  }
  return history_save(history);
}
