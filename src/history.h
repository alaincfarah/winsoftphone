#ifndef HISTORY_H
#define HISTORY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  char number[64];
  char cname[128];
  char direction[16];
  char timestamp[32];
  int duration_sec;
} history_entry_t;

typedef struct {
  history_entry_t *items;
  size_t count;
} history_list_t;

int history_load(history_list_t *history);
int history_save(const history_list_t *history);
void history_free(history_list_t *history);
int history_add(history_list_t *history, const history_entry_t *entry);

#ifdef __cplusplus
}
#endif

#endif
