#ifndef CONTACTS_H
#define CONTACTS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  char name[128];
  char number[64];
  char cname[128];
  char url[256];
} contact_t;

typedef struct {
  contact_t *items;
  size_t count;
} contact_list_t;

int contacts_load(contact_list_t *contacts);
int contacts_save(const contact_list_t *contacts);
void contacts_free(contact_list_t *contacts);
const contact_t *contacts_find_by_cname(const contact_list_t *contacts,
                                        const char *cname);

#ifdef __cplusplus
}
#endif

#endif
