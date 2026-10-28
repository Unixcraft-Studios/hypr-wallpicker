#define _POSIX_C_SOURCE 200809L

#include "wallpaper.h"
#include "fs.h"
#include "raylib.h"

#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static Image GenerateHexMask(int size, float radius, float angle_deg);
static int CountWallpapers(const char *dir_path);
static bool LoadSingleWallpaper(App *app, const char *filename);
static bool ExportThumbnail(Image image, const char *cache_path);

bool InitWallpaperResources(App *app) {
  if (app == NULL) {
    return false;
  }

  app->img_size = (int)(HEX_RADIUS * 2.0f);
  app->hex_mask = GenerateHexMask(app->img_size, HEX_RADIUS, app->angle);

  return true;
}

bool LoadWallpapers(App *app) {
  DIR *dir = NULL;
  struct dirent *ent = NULL;

  if (app == NULL || app->wp_dir == NULL) {
    fprintf(stderr, "Error: invalid app state for wallpaper loading\n");
    return false;
  }

  app->capacity = CountWallpapers(app->wp_dir);

  if (app->capacity < 0) {
    fprintf(stderr, "Error: Unable to open directory %s\n", app->wp_dir);
    return false;
  }

  if (app->capacity == 0) {
    app->wallpapers = NULL;
    app->wp_count = 0;
    return true;
  }

  app->wallpapers = calloc((size_t)app->capacity, sizeof(Wallpaper));
  if (app->wallpapers == NULL) {
    fprintf(stderr,
            "Fatal Error: memory allocation failed (attempted to allocate %d "
            "wallpaper slots)\n",
            app->capacity);
    return false;
  }

  app->wp_count = 0;

  dir = opendir(app->wp_dir);
  if (dir == NULL) {
    fprintf(stderr, "Error: Unable to reopen directory %s\n", app->wp_dir);
    UnloadWallpapers(app);
    return false;
  }

  while ((ent = readdir(dir)) != NULL) {
    if (!IsSupportedWallpaperFile(ent->d_name)) {
      continue;
    }

    if (WindowShouldClose()) {
      break;
    }

    BeginDrawing();
    ClearBackground(BLANK);
    DrawText("Loading & Caching Wallpapers...", GetScreenWidth() / 2 - 250,
             GetScreenHeight() / 2, 30, WHITE);
    DrawText(ent->d_name, GetScreenWidth() / 2 - 250,
             GetScreenHeight() / 2 + 40, 20, GRAY);
    EndDrawing();

    (void)LoadSingleWallpaper(app, ent->d_name);
  }

  closedir(dir);
  return true;
}

void UnloadWallpapers(App *app) {
  if (app == NULL) {
    return;
  }

  if (app->wallpapers != NULL) {
    for (int i = 0; i < app->wp_count; i++) {
      if (app->wallpapers[i].tex.id != 0) {
        UnloadTexture(app->wallpapers[i].tex);
      }
      if (app->wallpapers[i].filename != NULL) {
        free(app->wallpapers[i].filename);
      }
    }

    free(app->wallpapers);
    app->wallpapers = NULL;
  }

  app->wp_count = 0;
  app->capacity = 0;

  if (app->hex_mask.data != NULL) {
    UnloadImage(app->hex_mask);
    app->hex_mask = (Image){0};
  }

  app->img_size = 0;
}

static int CountWallpapers(const char *dir_path) {
  DIR *dir = opendir(dir_path);
  struct dirent *ent;
  int count = 0;

  if (dir == NULL) {
    return -1;
  }

  while ((ent = readdir(dir)) != NULL) {
    if (IsSupportedWallpaperFile(ent->d_name)) {
      count++;
    }
  }

  closedir(dir);
  return count;
}

static bool LoadSingleWallpaper(App *app, const char *filename) {
  char *full_img_path = NULL;
  char *cache_img_path = NULL;
  Image img = (Image){0};
  bool ok = false;

  if (app == NULL || filename == NULL) {
    return false;
  }

  if (app->wp_count >= app->capacity) {
    return false;
  }

  full_img_path = JoinPath(app->wp_dir, filename);
  cache_img_path = BuildCacheImagePath(app->cache_dir, full_img_path,
                                       app->img_size, app->angle);

  if (full_img_path == NULL || cache_img_path == NULL) {
    goto cleanup;
  }

  if (access(cache_img_path, F_OK) == 0) {
    img = LoadImage(cache_img_path);
    if (img.data != NULL &&
        (img.width != app->img_size || img.height != app->img_size)) {
      UnloadImage(img);
      img = (Image){0};
    }
  }

  if (img.data == NULL) {
    img = LoadImage(full_img_path);

    if (img.data != NULL && img.width > 1) {
      float scaleX = (float)app->img_size / (float)img.width;
      float scaleY = (float)app->img_size / (float)img.height;
      float scale = (scaleX > scaleY) ? scaleX : scaleY;

      int newW = (int)roundf((float)img.width * scale);
      int newH = (int)roundf((float)img.height * scale);

      if (newW < app->img_size) {
        newW = app->img_size;
      }
      if (newH < app->img_size) {
        newH = app->img_size;
      }

      ImageResize(&img, newW, newH);

      {
        int cropX = (newW - app->img_size) / 2;
        int cropY = (newH - app->img_size) / 2;

        ImageCrop(&img,
                  (Rectangle){(float)cropX, (float)cropY, (float)app->img_size,
                              (float)app->img_size});
      }

      ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
      ImageAlphaMask(&img, app->hex_mask);

      if (!ExportThumbnail(img, cache_img_path)) {
        fprintf(stderr, "Warning: failed to write cache image: %s\n",
                cache_img_path);
      }
    }
  }

  if (img.data != NULL && img.width > 1) {
    Wallpaper *wp = &app->wallpapers[app->wp_count];

    wp->filename = strdup(filename);
    if (wp->filename == NULL) {
      fprintf(stderr, "Warning: out of memory allocating filename for %s\n",
              filename);
      goto cleanup;
    }

    wp->tex = LoadTextureFromImage(img);
    if (wp->tex.id == 0) {
      fprintf(stderr, "Warning: failed to create texture for %s\n", filename);
      free(wp->filename);
      wp->filename = NULL;
      goto cleanup;
    }

    wp->currentScale = 1.0f;
    wp->currentColor = 130.0f;
    app->wp_count++;
    ok = true;
  }

cleanup:
  if (img.data != NULL) {
    UnloadImage(img);
  }

  free(cache_img_path);
  free(full_img_path);
  return ok;
}


static bool ExportThumbnail(Image image, const char *cache_path) {
  int needed;
  char *temp_path;
  bool ok;

  if (cache_path == NULL) {
    return false;
  }

  needed = snprintf(NULL, 0, "%s.tmp.%ld.png", cache_path, (long)getpid());
  if (needed < 0) {
    return false;
  }

  temp_path = malloc((size_t)needed + 1);
  if (temp_path == NULL) {
    return false;
  }

  snprintf(temp_path, (size_t)needed + 1, "%s.tmp.%ld.png", cache_path,
           (long)getpid());
  ok = ExportImage(image, temp_path);

  if (ok && rename(temp_path, cache_path) != 0) {
    fprintf(stderr, "Warning: failed to publish thumbnail '%s': %s\n",
            cache_path, strerror(errno));
    ok = false;
  }

  if (!ok) {
    unlink(temp_path);
  }

  free(temp_path);
  return ok;
}

static Image GenerateHexMask(int size, float radius, float angle_deg) {
  Image mask = GenImageColor(size, size, BLANK);
  float cx = (float)size / 2.0f;
  float cy = (float)size / 2.0f;

  float rad = -angle_deg * DEG2RAD;
  float cosA = cosf(rad);
  float sinA = sinf(rad);

  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      float tx = (float)x - cx;
      float ty = (float)y - cy;

      float rot_x = tx * cosA - ty * sinA;
      float rot_y = tx * sinA + ty * cosA;

      float dx = fabsf(rot_x);
      float dy = fabsf(rot_y);

      if (dx <= 0.866025f * radius && dy <= radius - dx * 0.57735f) {
        ImageDrawPixel(&mask, x, y, WHITE);
      } else {
        ImageDrawPixel(&mask, x, y, BLANK);
      }
    }
  }

  return mask;
}
