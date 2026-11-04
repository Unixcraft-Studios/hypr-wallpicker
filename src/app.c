#define _POSIX_C_SOURCE 200809L

#include "app.h"
#include "apply.h"
#include "fs.h"
#include "raylib.h"
#include "ui.h"
#include "wallpaper.h"

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void AppShutdown(App *app);

SessionBackend DetectSessionBackend(void) {
  const char *xdg_session_type = getenv("XDG_SESSION_TYPE");
  const char *wayland_display = getenv("WAYLAND_DISPLAY");
  const char *display = getenv("DISPLAY");

  if (xdg_session_type != NULL) {
    if (strcmp(xdg_session_type, "wayland") == 0) {
      return SESSION_BACKEND_WAYLAND;
    }
    if (strcmp(xdg_session_type, "x11") == 0) {
      return SESSION_BACKEND_X11;
    }
  }

  if (wayland_display != NULL && wayland_display[0] != '\0') {
    return SESSION_BACKEND_WAYLAND;
  }

  if (display != NULL && display[0] != '\0') {
    return SESSION_BACKEND_X11;
  }

  return SESSION_BACKEND_UNKNOWN;
}

const char *SessionBackendName(SessionBackend backend) {
  switch (backend) {
  case SESSION_BACKEND_WAYLAND:
    return "Wayland";
  case SESSION_BACKEND_X11:
    return "X11";
  case SESSION_BACKEND_UNKNOWN:
  default:
    return "Unknown";
  }
}

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

AppConfig AppConfigFromArgs(int argc, char **argv) {
  AppConfig config = {0};

  config.window_width = DEFAULT_WINDOW_WIDTH;
  config.window_height = DEFAULT_WINDOW_HEIGHT;
  config.renderer.cols = DEFAULT_COLS;
  config.renderer.spacing = DEFAULT_SPACING;
  config.renderer.angle = 0.0f; // vertical by default
  config.backend = DetectSessionBackend();

  static struct option long_options[] = {{"cols", required_argument, 0, 'c'},
                                         {"spacing", required_argument, 0, 's'},
                                         {"angle", required_argument, 0, 'a'},
                                         {0, 0, 0, 0}};

  int opt;
  int option_index = 0;
  char *endptr;

  opterr = 0;

  while ((opt = getopt_long(argc, argv, "c:s:a:", long_options,
                            &option_index)) != -1) {
    switch (opt) {
    case 'c': {
      errno = 0;
      long val = strtol(optarg, &endptr, 10);
      if (errno == 0 && *endptr == '\0' && val >= 1) {
        config.renderer.cols = (int)val;
      } else {
        fprintf(stderr,
                "Warning: Invalid columns '%s', falling back to default %d\n",
                optarg, DEFAULT_COLS);
      }
      break;
    }
    case 's': {
      errno = 0;
      float val = strtof(optarg, &endptr);
      if (errno == 0 && *endptr == '\0' && val >= 0.0f) {
        config.renderer.spacing = val;
      } else {
        fprintf(stderr,
                "Warning: Invalid spacing '%s', falling back to default %.1f\n",
                optarg, DEFAULT_SPACING);
      }
      break;
    }
    case 'a': {
      errno = 0;
      float val = strtof(optarg, &endptr);
      if (errno == 0 && *endptr == '\0') {
        config.renderer.angle = val;
      } else {
        fprintf(stderr,
                "Warning: Invalid angle '%s', falling back to default 0.0\n",
                optarg);
      }
      break;
    }
    case '?':
      fprintf(stderr, "Warning: Ignored unknown or incomplete option.\n");
      break;
    default:
      break;
    }
  }

  if (optind < argc) {
    config.wallpaper.directory = argv[optind];
  } else {
    config.wallpaper.directory = NULL;
  }
  return config;
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
