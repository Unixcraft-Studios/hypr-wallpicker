#define _POSIX_C_SOURCE 200809L

#include "fs.h"
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define THUMBNAIL_CACHE_VERSION 2U

static uint64_t HashBytes(uint64_t hash, const void *data, size_t size) {
  const unsigned char *bytes = data;

  for (size_t i = 0; i < size; i++) {
    hash ^= bytes[i];
    hash *= UINT64_C(1099511628211);
  }

  return hash;
}

static bool EnsureDirExists(const char *dir_path) {
  struct stat st = {0};

  if (stat(dir_path, &st) == 0) {
    if (!S_ISDIR(st.st_mode)) {
      fprintf(stderr, "Error: '%s' exists but is not a directory\n", dir_path);

      return false;
    }
    return true;
  }

  if (mkdir(dir_path, 0755) != 0) {
    fprintf(stderr, "Error trying to create directory '%s': %s\n", dir_path,
            strerror(errno));

    return false;
  }

  return true;
}

bool GetCacheDir(char *out_path, size_t max_len) {
  const char *cache_home = getenv("XDG_CACHE_HOME");
  const char *home = getenv("HOME");
  char root_path[PATH_MAX];
  int n;

  if ((cache_home == NULL || cache_home[0] == '\0') && home == NULL) {
    fprintf(stderr, "Error: unable to find HOME environment variable\n");
    return false;
  }

  if (cache_home != NULL && cache_home[0] != '\0') {
    n = snprintf(root_path, sizeof(root_path), "%s/wallpicker", cache_home);
  } else {
    n = snprintf(root_path, sizeof(root_path), "%s/.cache/wallpicker", home);
  }

  if (n < 0 || (size_t)n >= sizeof(root_path)) {
    fprintf(stderr, "Error: cache path is too long\n");
    return false;
  }

  if (!EnsureDirExists(root_path)) {
    return false;
  }

  n = snprintf(out_path, max_len, "%s/thumbnails-v2", root_path);
  if (n < 0 || (size_t)n >= max_len) {
    fprintf(stderr, "Error: thumbnail cache path is too long\n");
    return false;
  }

  return EnsureDirExists(out_path);
}

char *GetDefaultWallpaperDir(void) {
  const char *home = getenv("HOME");
  if (home == NULL) {
    fprintf(stderr, "Error: unable to find HOME environment variable\n");
    return NULL;
  }

  int needed = snprintf(NULL, 0, "%s/Pictures/wallpapers", home);
  if (needed < 0) {
    fprintf(stderr, "Error: failed to build wallpaper directory path\n");
    return NULL;
  }

  char *out = malloc((size_t)needed + 1);
  if (out == NULL) {
    fprintf(stderr, "Error: out of memory\n");
    return NULL;
  }

  snprintf(out, (size_t)needed + 1, "%s/Pictures/wallpapers", home);
  return out;
}

char *JoinPath(const char *dir, const char *name) {
  size_t len_dir = strlen(dir);
  size_t len_name = strlen(name);
  bool need_slash = (len_dir > 0 && dir[len_dir - 1] != '/');

  size_t total = len_dir + (need_slash ? 1u : 0u) + len_name + 1u;

  char *out = malloc(total);
  if (out == NULL) {
    fprintf(stderr, "Error: out of memory\n");
    return NULL;
  }

  memcpy(out, dir, len_dir);
  size_t pos = len_dir;

  if (need_slash) {
    out[pos++] = '/';
  }

  memcpy(out + pos, name, len_name);
  out[pos + len_name] = '\0';

  return out;
}

char *BuildCacheImagePath(const char *cache_dir, const char *source_path,
                          int thumbnail_size, float mask_angle) {
  const uint64_t fnv_offset = UINT64_C(14695981039346656037);
  const char cache_namespace[] = "wallpicker-thumbnail-v2";
  struct stat source_stat;
  uint32_t version = THUMBNAIL_CACHE_VERSION;
  uint32_t angle_bits = 0;
  uint64_t hash;
  int needed;

  if (cache_dir == NULL || source_path == NULL || thumbnail_size < 1) {
    return NULL;
  }

  if (stat(source_path, &source_stat) != 0) {
    fprintf(stderr, "Warning: unable to stat wallpaper '%s': %s\n", source_path,
            strerror(errno));
    return NULL;
  }

  memcpy(&angle_bits, &mask_angle, sizeof(angle_bits));

  hash = HashBytes(fnv_offset, cache_namespace, sizeof(cache_namespace));
  hash = HashBytes(hash, source_path, strlen(source_path) + 1);
  hash = HashBytes(hash, &source_stat.st_dev, sizeof(source_stat.st_dev));
  hash = HashBytes(hash, &source_stat.st_ino, sizeof(source_stat.st_ino));
  hash = HashBytes(hash, &source_stat.st_size, sizeof(source_stat.st_size));
  hash = HashBytes(hash, &source_stat.st_mtim.tv_sec,
                   sizeof(source_stat.st_mtim.tv_sec));
  hash = HashBytes(hash, &source_stat.st_mtim.tv_nsec,
                   sizeof(source_stat.st_mtim.tv_nsec));
  hash = HashBytes(hash, &thumbnail_size, sizeof(thumbnail_size));
  hash = HashBytes(hash, &angle_bits, sizeof(angle_bits));
  hash = HashBytes(hash, &version, sizeof(version));

  needed = snprintf(NULL, 0, "%s/%016" PRIx64 ".png", cache_dir, hash);
  if (needed < 0) {
    fprintf(stderr, "Error: failed to build cache image path\n");
    return NULL;
  }

  char *out = malloc((size_t)needed + 1);
  if (out == NULL) {
    fprintf(stderr, "Error: out of memory\n");
    return NULL;
  }

  snprintf(out, (size_t)needed + 1, "%s/%016" PRIx64 ".png", cache_dir, hash);
  return out;
}

bool HasExtension(const char *filename, const char *ext) {
  const char *dot = strrchr(filename, '.');
  if (!dot || dot == filename)
    return false;
  return strcasecmp(dot, ext) == 0;
}

bool IsSupportedWallpaperFile(const char *filename) {
  return HasExtension(filename, ".png") || HasExtension(filename, ".jpg") ||
         HasExtension(filename, ".jpeg");
}
