#include "json_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int json_token_eq(const char *json, const jsmntok_t *tok, const char *s) {
  size_t len;
  if (!json || !tok || !s) {
    return 0;
  }
  len = (size_t)(tok->end - tok->start);
  return (tok->type == JSMN_STRING &&
          strlen(s) == len &&
          strncmp(json + tok->start, s, len) == 0);
}

int json_token_copy(const char *json, const jsmntok_t *tok,
                    char *out, size_t out_size) {
  size_t len;
  if (!json || !tok || !out || out_size == 0) {
    return -1;
  }
  len = (size_t)(tok->end - tok->start);
  if (len + 1 > out_size) {
    return -1;
  }
  memcpy(out, json + tok->start, len);
  out[len] = '\0';
  return 0;
}

int json_token_to_int(const char *json, const jsmntok_t *tok, int *out_value) {
  char buffer[64];
  char *endptr;
  if (json_token_copy(json, tok, buffer, sizeof(buffer)) != 0) {
    return -1;
  }
  *out_value = (int)strtol(buffer, &endptr, 10);
  return endptr == buffer ? -1 : 0;
}

int json_token_to_float(const char *json, const jsmntok_t *tok,
                        float *out_value) {
  char buffer[64];
  char *endptr;
  if (json_token_copy(json, tok, buffer, sizeof(buffer)) != 0) {
    return -1;
  }
  *out_value = strtof(buffer, &endptr);
  return endptr == buffer ? -1 : 0;
}

int json_find_key(const char *json, jsmntok_t *tokens, int count,
                  int object_index, const char *key) {
  int i;
  int remaining;
  if (!json || !tokens || object_index < 0 || object_index >= count) {
    return -1;
  }
  if (tokens[object_index].type != JSMN_OBJECT) {
    return -1;
  }
  i = object_index + 1;
  remaining = tokens[object_index].size;
  while (remaining > 0 && i < count) {
    jsmntok_t *key_tok = &tokens[i];
    int key_span = json_token_span(tokens, i);
    int value_index = i + key_span;
    if (value_index >= count) {
      return -1;
    }
    if (json_token_eq(json, key_tok, key)) {
      return value_index;
    }
    i = value_index + json_token_span(tokens, value_index);
    remaining--;
  }
  return -1;
}

int json_escape_string(const char *input, char *out, size_t out_size) {
  size_t i;
  size_t pos = 0;
  if (!input || !out || out_size == 0) {
    return -1;
  }
  for (i = 0; input[i] != '\0'; i++) {
    char c = input[i];
    const char *escape = NULL;
    char temp[2] = {0};
    switch (c) {
      case '\"':
        escape = "\\\"";
        break;
      case '\\':
        escape = "\\\\";
        break;
      case '\n':
        escape = "\\n";
        break;
      case '\r':
        escape = "\\r";
        break;
      case '\t':
        escape = "\\t";
        break;
      default:
        temp[0] = c;
        escape = temp;
        break;
    }
    if (pos + strlen(escape) + 1 > out_size) {
      return -1;
    }
    strcpy(out + pos, escape);
    pos += strlen(escape);
  }
  out[pos] = '\0';
  return 0;
}
