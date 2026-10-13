#define _POSIX_C_SOURCE 200809L

#include "app.h"
#include "apply.h"
#include "fs.h"
#include "raylib.h"
#include "ui.h"
#include "wallpaper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void AppShutdown(App *app);

void FreeSelectionResult(SelectionResult *result) {
  if (result != NULL) {
    if (result->full_target_path != NULL) {
      free(result->full_target_path);
      result->full_target_path = NULL;
    }
    result->valid = false;
    result->rel_x = 0.0f;
    result->rel_y = 0.0f;
  }
}

int AppRun(const AppConfig *config) {
  App app = {0};

  if (config == NULL) {
    fprintf(stderr, "Error: invalid app config\n");
    return 1;
  }

  if (!GetCacheDir(app.cache_dir, sizeof(app.cache_dir))) {
    return 1;
  }

  if (config->wallpaper.directory != NULL) {
    app.wp_dir = strdup(config->wallpaper.directory);
    if (app.wp_dir == NULL) {
      fprintf(stderr, "Error: out of memory\n");
      return 1;
    }
  } else {
    app.wp_dir = GetDefaultWallpaperDir();
    if (app.wp_dir == NULL) {
      return 1;
    }
  }

  SetConfigFlags(FLAG_WINDOW_TRANSPARENT | FLAG_WINDOW_UNDECORATED);
  InitWindow(config->window_width, config->window_height, "wallpicker");

  app.angle = config->renderer.angle;
  if (!InitWallpaperResources(&app)) {
    fprintf(stderr, "Error: failed to initialize wallpaper resources\n");
    AppShutdown(&app);
    return 1;
  }

  if (!LoadWallpapers(&app)) {
    AppShutdown(&app);
    return 1;
  }

  if (app.wp_count == 0) {
    printf("No wallpapers in .png, .jpg, or .jpeg format were found in %s.\n",
           app.wp_dir);
    AppShutdown(&app);
    return 0;
  }

  SetTargetFPS(60);

  SelectionResult result = RunUI(&app, config);

  if (result.valid) {
    ApplyWallpaper(result.full_target_path, result.rel_x, result.rel_y);
  }

  FreeSelectionResult(&result);

  AppShutdown(&app);
  return 0;
}

static void AppShutdown(App *app) {
  if (app == NULL) {
    return;
  }

  UnloadWallpapers(app);

  if (app->wp_dir != NULL) {
    free(app->wp_dir);
    app->wp_dir = NULL;
  }

  if (IsWindowReady()) {
    CloseWindow();
  }
}
