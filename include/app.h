#ifndef APP_H
#define APP_H

#include "raylib.h"
#include <limits.h>
#include <stdbool.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define HEX_RADIUS 200.0f
#define DEFAULT_COLS 5
#define DEFAULT_SPACING 15.0f
#define DEFAULT_WINDOW_WIDTH 1920
#define DEFAULT_WINDOW_HEIGHT 1080

typedef struct {
  Texture2D tex;
  char *filename;
  float currentScale;
  float currentColor;
  // Cached render coordinates to avoid redundant layout calculations in the
  // render loop
  float render_x;
  float render_y;
} Wallpaper;

typedef struct {
  bool valid;
  char *full_target_path;
  float rel_x;
  float rel_y;
} SelectionResult;

typedef enum {
  SESSION_BACKEND_UNKNOWN = 0,
  SESSION_BACKEND_WAYLAND,
  SESSION_BACKEND_X11
} SessionBackend;

typedef struct {
  const char *directory;
} WallpaperConfig;

typedef struct {
  int cols;
  float spacing;
  float angle;
} RendererConfig;

typedef struct {
  WallpaperConfig wallpaper;
  int window_width;
  int window_height;
  RendererConfig renderer;
  SessionBackend backend;
} AppConfig;

typedef struct {
  char *wp_dir;
  char cache_dir[PATH_MAX];

  Wallpaper *wallpapers;
  int wp_count;
  int capacity;

  Image hex_mask;
  int img_size;

  float angle;
} App;

AppConfig AppConfigFromArgs(int argc, char **argv);
int AppRun(const AppConfig *config);

SessionBackend DetectSessionBackend(void);
const char *SessionBackendName(SessionBackend backend);

void FreeSelectionResult(SelectionResult *result);
#endif
