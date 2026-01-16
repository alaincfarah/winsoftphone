#include "contacts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "json_utils.h"
#include "jsmn.h"
#include "storage.h"

#define CONTACTS_FILE_NAME "contacts.json"
#define DEFAULT_CONTACTS_RELATIVE "config\\default_contacts.json"

static int contacts_file_exists(const char *path) {
  DWORD attrs = GetFileAttributesA(path);
  return (attrs != INVALID_FILE_ATTRIBUTES &&
          (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

static int contacts_get_path(char *out_path, size_t out_size) {
  char appdata[MAX_PATH];
  if (storage_get_appdata_path(appdata, sizeof(appdata)) != 0) {
    return -1;
  }
  return storage_join_path(appdata, CONTACTS_FILE_NAME, out_path, out_size);
}

static int contacts_copy_default(const char *destination_path) {
  char exe_dir[MAX_PATH];
  char default_path[MAX_PATH];
  char *data = NULL;
  size_t data_size = 0;
  if (storage_get_exe_dir(exe_dir, sizeof(exe_dir)) != 0) {
    return -1;
  }
  if (storage_join_path(exe_dir, DEFAULT_CONTACTS_RELATIVE,
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

static void contacts_init_empty(contact_list_t *contacts) {
  contacts->items = NULL;
  contacts->count = 0;
}

static int contacts_append(contact_list_t *contacts, const contact_t *entry) {
  contact_t *new_items = (contact_t *)realloc(
      contacts->items, sizeof(contact_t) * (contacts->count + 1));
  if (!new_items) {
    return -1;
  }
  contacts->items = new_items;
  contacts->items[contacts->count] = *entry;
  contacts->count++;
  return 0;
}

int contacts_load(contact_list_t *contacts) {
  char contacts_path[MAX_PATH];
  char appdata[MAX_PATH];
  char *json = NULL;
  size_t json_size = 0;
  jsmn_parser parser;
  jsmntok_t tokens[512];
  int token_count;
  int index;

  if (!contacts) {
    return -1;
  }
  contacts_init_empty(contacts);

  if (storage_get_appdata_path(appdata, sizeof(appdata)) != 0) {
    return -1;
  }
  if (storage_ensure_dir_recursive(appdata) != 0) {
    return -1;
  }
  if (contacts_get_path(contacts_path, sizeof(contacts_path)) != 0) {
    return -1;
  }
  if (!contacts_file_exists(contacts_path)) {
    contacts_copy_default(contacts_path);
  }

  if (storage_read_file(contacts_path, &json, &json_size) != 0) {
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
      contact_t entry;
      int value_index;
      memset(&entry, 0, sizeof(entry));
      value_index = json_find_key(json, tokens, token_count, index, "name");
      if (value_index >= 0) {
        json_token_copy(json, &tokens[value_index], entry.name, sizeof(entry.name));
      }
      value_index = json_find_key(json, tokens, token_count, index, "number");
      if (value_index >= 0) {
        json_token_copy(json, &tokens[value_index], entry.number, sizeof(entry.number));
      }
      value_index = json_find_key(json, tokens, token_count, index, "cname");
      if (value_index >= 0) {
        json_token_copy(json, &tokens[value_index], entry.cname, sizeof(entry.cname));
      }
      value_index = json_find_key(json, tokens, token_count, index, "url");
      if (value_index >= 0) {
        json_token_copy(json, &tokens[value_index], entry.url, sizeof(entry.url));
      }
      if (entry.cname[0] != '\0') {
        contacts_append(contacts, &entry);
      }
    }
    index += json_token_span(tokens, index);
  }

  free(json);
  return 0;
}

int contacts_save(const contact_list_t *contacts) {
  char contacts_path[MAX_PATH];
  char appdata[MAX_PATH];
  FILE *file;

  if (!contacts) {
    return -1;
  }
  if (storage_get_appdata_path(appdata, sizeof(appdata)) != 0) {
    return -1;
  }
  if (storage_ensure_dir_recursive(appdata) != 0) {
    return -1;
  }
  if (contacts_get_path(contacts_path, sizeof(contacts_path)) != 0) {
    return -1;
  }

  file = fopen(contacts_path, "wb");
  if (!file) {
    return -1;
  }
  fprintf(file, "[\n");
  for (size_t i = 0; i < contacts->count; i++) {
    char escaped_name[256];
    char escaped_number[128];
    char escaped_cname[256];
    char escaped_url[512];
    const contact_t *entry = &contacts->items[i];
    json_escape_string(entry->name, escaped_name, sizeof(escaped_name));
    json_escape_string(entry->number, escaped_number, sizeof(escaped_number));
    json_escape_string(entry->cname, escaped_cname, sizeof(escaped_cname));
    json_escape_string(entry->url, escaped_url, sizeof(escaped_url));
    fprintf(file,
            "  {\n"
            "    \"name\": \"%s\",\n"
            "    \"number\": \"%s\",\n"
            "    \"cname\": \"%s\",\n"
            "    \"url\": \"%s\"\n"
            "  }%s\n",
            escaped_name,
            escaped_number,
            escaped_cname,
            escaped_url,
            (i + 1 < contacts->count) ? "," : "");
  }
  fprintf(file, "]\n");
  fclose(file);
  return 0;
}

void contacts_free(contact_list_t *contacts) {
  if (!contacts) {
    return;
  }
  free(contacts->items);
  contacts->items = NULL;
  contacts->count = 0;
}

const contact_t *contacts_find_by_cname(const contact_list_t *contacts,
                                        const char *cname) {
  if (!contacts || !cname) {
    return NULL;
  }
  for (size_t i = 0; i < contacts->count; i++) {
    if (_stricmp(contacts->items[i].cname, cname) == 0) {
      return &contacts->items[i];
    }
  }
  return NULL;
}
