#include "storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <shlobj.h>

static int storage_path_is_sep(char c) {
  return c == '\\' || c == '/';
}

int storage_read_file(const char *path, char **out_data, size_t *out_size) {
  FILE *file = fopen(path, "rb");
  long size;
  char *data;

  if (!file) {
    return -1;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return -1;
  }
  size = ftell(file);
  if (size < 0) {
    fclose(file);
    return -1;
  }
  if (fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return -1;
  }

  data = (char *)malloc((size_t)size + 1);
  if (!data) {
    fclose(file);
    return -1;
  }

  if (fread(data, 1, (size_t)size, file) != (size_t)size) {
    fclose(file);
    free(data);
    return -1;
  }
  data[size] = '\0';
  fclose(file);

  *out_data = data;
  if (out_size) {
    *out_size = (size_t)size;
  }
  return 0;
}

int storage_write_file(const char *path, const char *data) {
  FILE *file = fopen(path, "wb");
  size_t len;

  if (!file) {
    return -1;
  }
  len = strlen(data);
  if (fwrite(data, 1, len, file) != len) {
    fclose(file);
    return -1;
  }
  fclose(file);
  return 0;
}

static int storage_create_dir(const char *path) {
  if (CreateDirectoryA(path, NULL)) {
    return 0;
  }
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    return 0;
  }
  return -1;
}

int storage_ensure_dir_recursive(const char *path) {
  char buffer[MAX_PATH];
  size_t len;
  size_t i;

  if (!path || !path[0]) {
    return -1;
  }

  strncpy(buffer, path, sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';
  len = strlen(buffer);

  for (i = 0; i < len; i++) {
    if (storage_path_is_sep(buffer[i])) {
      buffer[i] = '\0';
      if (buffer[0] != '\0') {
        if (storage_create_dir(buffer) != 0) {
          return -1;
        }
      }
      buffer[i] = '\\';
    }
  }
  return storage_create_dir(buffer);
}

int storage_get_appdata_path(char *out_path, size_t out_size) {
  char base[MAX_PATH];
  if (SHGetFolderPathA(NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL,
                       SHGFP_TYPE_CURRENT, base) != S_OK) {
    return -1;
  }
  return storage_join_path(base, "WinSoftphone", out_path, out_size);
}

int storage_get_exe_dir(char *out_path, size_t out_size) {
  char buffer[MAX_PATH];
  DWORD len = GetModuleFileNameA(NULL, buffer, MAX_PATH);
  char *last_sep;
  if (len == 0 || len >= MAX_PATH) {
    return -1;
  }
  last_sep = buffer + len;
  while (last_sep > buffer) {
    if (storage_path_is_sep(*last_sep)) {
      break;
    }
    last_sep--;
  }
  if (last_sep == buffer) {
    return -1;
  }
  *last_sep = '\0';
  strncpy(out_path, buffer, out_size - 1);
  out_path[out_size - 1] = '\0';
  return 0;
}

int storage_join_path(const char *base, const char *leaf,
                      char *out_path, size_t out_size) {
  size_t base_len;
  if (!base || !leaf || !out_path || out_size == 0) {
    return -1;
  }
  base_len = strlen(base);
  if (base_len + 1 + strlen(leaf) + 1 > out_size) {
    return -1;
  }
  strcpy(out_path, base);
  if (base_len > 0 && !storage_path_is_sep(base[base_len - 1])) {
    strcat(out_path, "\\");
  }
  strcat(out_path, leaf);
  return 0;
}
