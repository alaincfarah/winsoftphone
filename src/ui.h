#ifndef UI_H
#define UI_H

#include <windows.h>

#include "app.h"

#ifdef __cplusplus
extern "C" {
#endif

int ui_init(app_state_t *app);
int ui_run(void);
void ui_set_status_text(const char *text);
void ui_set_line_text(const char *text);
void ui_open_url(const char *url);

#ifdef __cplusplus
}
#endif

#endif
