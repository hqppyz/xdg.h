#ifndef XDG_H
#define XDG_H

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static int xdg_verbose = 0;

static inline int xdg__dir_exists(const char *path) {
  if (path == NULL || path[0] == '\0') {
    return 0;
  }

  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static inline int xdg_dir_absolute(const char *path) {
  return path != NULL && path[0] == '/';
}

// Thanks to Carl @ StackOverflow (https://stackoverflow.com/a/2336245)
static inline int xdg__mkdirs(const char *dir, mode_t mode) {
  char tmp[PATH_MAX];
  char *p = NULL;
  size_t len;

  if (dir == NULL || dir[0] == '\0') return 0;

  if (strnlen(dir, sizeof(tmp)) >= sizeof(tmp)) return 0;

  snprintf(tmp, sizeof(tmp), "%s", dir);
  len = strlen(tmp);
  if (tmp[len - 1] == '/') {
    tmp[len - 1] = 0;
  }

  for (p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = 0;
      if (mkdir(tmp, mode) == -1 && !xdg__dir_exists(tmp)) {
        *p = '/';
        return 0;
      }
      *p = '/';
    }
  }

  return mkdir(tmp, mode) != -1 || xdg__dir_exists(tmp);
}

static inline char *xdg__paths_join(const char *base, const char *dir) {
  size_t base_len = strnlen(base, PATH_MAX);
  size_t dir_len = strnlen(dir, PATH_MAX);
  size_t path_len = base_len + dir_len + 2;

  if (base_len == PATH_MAX || dir_len == PATH_MAX || path_len > PATH_MAX) {
    if (xdg_verbose) fprintf(stderr, "DEBUG: Ignoring '%s/%s' as it exceeds PATH_MAX.\n", base, dir);
    return NULL;
  }

  char *path = malloc(path_len);
  if (!path) {
    fprintf(stderr, "ERROR: Out of memory.\n");
    return NULL;
  }

  snprintf(path, path_len, "%s/%s", base, dir);
  return path;
}

static inline char *xdg__get_or_create_path(const char *base, const char *dir, int search) {
  char *path = xdg__paths_join(base, dir);
  if (path == NULL) return NULL;

  if (search) {
    if (xdg__dir_exists(path)) return path;
  } else {
    if (!xdg__mkdirs(path, S_IRWXU)) {
      fprintf(stderr, "ERROR: Could not create directory '%s'.\n", path);
      free(path);
      return NULL;
    }

    return path;
  }

  free(path);
  return NULL;
}

static inline char *xdg__get_user_home_path(const char *home_sub_dir, const char *dir, int search) {
  char *home = getenv("HOME");
  if (!xdg__dir_exists(home)) return NULL;

  size_t home_len = strnlen(home, PATH_MAX);
  size_t home_sub_dir_len = strnlen(home_sub_dir, PATH_MAX);
  size_t base_len = home_len + home_sub_dir_len + 2;

  if (home_len == PATH_MAX || home_sub_dir_len == PATH_MAX || base_len > PATH_MAX) {
    if (xdg_verbose) fprintf(stderr, "DEBUG: Ignoring '%s/%s' as it exceeds PATH_MAX.\n", home, home_sub_dir);
    return NULL;
  }

  char *base = malloc(base_len);
  if (!base) {
    fprintf(stderr, "ERROR: Out of memory.\n");
    return NULL;
  }

  snprintf(base, base_len, "%s/%s", home, home_sub_dir);

  char *path = xdg__get_or_create_path(base, dir, search);
  free(base);
  return path;
}

static inline char *xdg__get_xdg_home_path(const char *xdg_home_env, const char *home_sub_dir, const char *dir, int search) {
  char *xdg_home = getenv(xdg_home_env);

  if (xdg_home != NULL && xdg_home[0] != '\0') {
    if (xdg_dir_absolute(xdg_home)) return xdg__get_or_create_path(xdg_home, dir, search);

    if (xdg_verbose) fprintf(stderr, "DEBUG: Ignoring path in %s as it is not absolute.\n", xdg_home_env);
    return xdg__get_user_home_path(home_sub_dir, dir, search);
  }

  return xdg__get_user_home_path(home_sub_dir, dir, search);
}

static inline char *xdg__get_path(const char *xdg_home_env, const char *xdg_dirs_env, const char *xdg_dirs_default, const char *home_sub_dir, const char *dir, int search) {
  char *path = xdg__get_xdg_home_path(xdg_home_env, home_sub_dir, dir, search);
  if (path != NULL) return path;

  const char *xdg_dirs = getenv(xdg_dirs_env);
  if (search) {
    if (xdg_dirs == NULL || xdg_dirs[0] == '\0') {
      xdg_dirs = xdg_dirs_default;
    }

    const char *xdg_dir = xdg_dirs;
    while (xdg_dir != NULL && *xdg_dir != '\0') {
      const char *next = strchr(xdg_dir, ':');
      size_t xdg_dir_len = next == NULL ? strlen(xdg_dir) : (size_t) (next - xdg_dir);

      if (xdg_dir_len == 0) {
        xdg_dir = next == NULL ? NULL : next + 1;
        continue;
      }

      if (xdg_dir_len >= PATH_MAX) {
        if (xdg_verbose) fprintf(stderr, "DEBUG: Ignoring path in %s as it exceeds PATH_MAX.\n", xdg_dirs_env);
      } else {
        char xdg_dir_buf[PATH_MAX];
        memcpy(xdg_dir_buf, xdg_dir, xdg_dir_len);
        xdg_dir_buf[xdg_dir_len] = '\0';

        if (!xdg_dir_absolute(xdg_dir_buf)) {
          if (xdg_verbose) fprintf(stderr, "DEBUG: Ignoring path in %s as it is not absolute.\n", xdg_dirs_env);
        } else {
          char *found_path = xdg__get_or_create_path(xdg_dir_buf, dir, search);
          if (found_path != NULL) return found_path;
        }
      }

      xdg_dir = next == NULL ? NULL : next + 1;
    }
  }

  return search ? xdg__get_xdg_home_path(xdg_home_env, home_sub_dir, dir, 0) : NULL;
}

static inline char *xdg_get_path(const char *xdg_home_env, const char *xdg_dirs_env, const char *xdg_dirs_default, const char *home_sub_dir, const char *dir) {
  return xdg__get_path(xdg_home_env, xdg_dirs_env, xdg_dirs_default, home_sub_dir, dir, 1);
}

static inline char *xdg_get_config_path(const char *config_dir) {
  return xdg_get_path("XDG_CONFIG_HOME", "XDG_CONFIG_DIRS", "/etc/xdg", ".config", config_dir);
}

static inline char *xdg_get_data_path(const char *data_dir) {
  return xdg_get_path("XDG_DATA_HOME", "XDG_DATA_DIRS", "/usr/local/share:/usr/share", ".local/share", data_dir);
}

#endif // XDG_H
