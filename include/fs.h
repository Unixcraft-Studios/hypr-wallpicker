#ifndef FS_H
#define FS_H

#include <stdbool.h>
#include <stddef.h>

bool GetCacheDir(char *out_path, size_t max_len);
char *GetDefaultWallpaperDir(void);
char *JoinPath(const char *dir, const char *name);
char *BuildCacheImagePath(const char *cache_dir, const char *source_path,
                          int thumbnail_size, float mask_angle);
bool IsSupportedWallpaperFile(const char *filename);
bool HasExtension(const char *filename, const char *ext);

#endif
