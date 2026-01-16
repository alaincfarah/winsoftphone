#ifndef STORAGE_H
#define STORAGE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int storage_read_file(const char *path, char **out_data, size_t *out_size);
int storage_write_file(const char *path, const char *data);
int storage_ensure_dir_recursive(const char *path);
int storage_get_appdata_path(char *out_path, size_t out_size);
int storage_get_exe_dir(char *out_path, size_t out_size);
int storage_join_path(const char *base, const char *leaf,
                      char *out_path, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif
