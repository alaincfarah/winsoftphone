#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include <stddef.h>

#include "jsmn.h"

#ifdef __cplusplus
extern "C" {
#endif

int json_token_eq(const char *json, const jsmntok_t *tok, const char *s);
int json_token_copy(const char *json, const jsmntok_t *tok,
                    char *out, size_t out_size);
int json_token_to_int(const char *json, const jsmntok_t *tok, int *out_value);
int json_token_to_float(const char *json, const jsmntok_t *tok, float *out_value);
int json_find_key(const char *json, jsmntok_t *tokens, int count,
                  int object_index, const char *key);
int json_escape_string(const char *input, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif
