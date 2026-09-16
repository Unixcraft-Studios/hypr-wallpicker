#include "ui.h"
#include "app.h"
#include "fs.h"
#include "raylib.h"
#include "render.h"
#include <math.h>

typedef struct {
  float scroll_offset;
  float target_scroll_offset;
} UiState;

typedef struct {
  float inradius;
  float cos_angle;
  float sin_angle;
  int cols;
  float step_x;
  float step_y;
  float center_x;
  float center_y;
  float max_scroll_amplitude;
} GridLayout;

static GridLayout BuildGridLayout(const App *app,
                                  const RendererConfig *renderer) {
  GridLayout layout = {0};
  int total_rows;
  float total_height;
  float rad;

  layout.inradius = HEX_RADIUS * 0.866025f;
  layout.cols = renderer->cols < 1 ? 1 : renderer->cols;
  layout.step_x = 1.73205f * HEX_RADIUS + renderer->spacing;
  layout.step_y = 1.5f * HEX_RADIUS + renderer->spacing;

  total_rows = (app->wp_count + layout.cols - 1) / layout.cols;
  total_height = (2.0f * HEX_RADIUS) + (total_rows - 1) * layout.step_y;

  layout.center_x =
      ((layout.cols - 1) * layout.step_x + layout.step_x / 2.0f) / 2.0f;
  layout.center_y = (total_rows - 1) * layout.step_y / 2.0f;
  layout.max_scroll_amplitude = total_height / 2.0f;

  rad = renderer->angle * DEG2RAD;
  layout.cos_angle = cosf(rad);
  layout.sin_angle = sinf(rad);

  return layout;
}

SelectionResult RunUI(App *app, const AppConfig *config) {
  SelectionResult result = {0};
  const RendererConfig *renderer = &config->renderer;
  const GridLayout layout = BuildGridLayout(app, renderer);
  UiState ui = {0};

  result.valid = false;

  while (!WindowShouldClose()) {
    Vector2 mousePoint = GetMousePosition();

    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f && layout.max_scroll_amplitude > 0.0f) {
      ui.target_scroll_offset += wheel * 120.0f;
    }

    if (ui.target_scroll_offset > layout.max_scroll_amplitude)
      ui.target_scroll_offset = layout.max_scroll_amplitude;
    if (ui.target_scroll_offset < -layout.max_scroll_amplitude)
      ui.target_scroll_offset = -layout.max_scroll_amplitude;

    ui.scroll_offset += (ui.target_scroll_offset - ui.scroll_offset) * 0.15f;

    int hoveredIndex = -1;

    for (int i = 0; i < app->wp_count; i++) {
      int row = i / layout.cols;
      int col = i % layout.cols;

      float local_x = (float)col * layout.step_x;
      if ((row % 2) != 0)
        local_x += layout.step_x / 2.0f;
      float local_y = (float)row * layout.step_y;

      local_x -= layout.center_x;
      local_y = local_y - layout.center_y + ui.scroll_offset;

      float rot_x = local_x * layout.cos_angle - local_y * layout.sin_angle;
      float rot_y = local_x * layout.sin_angle + local_y * layout.cos_angle;

      float currentX = GetScreenWidth() / 2.0f + rot_x;
      float currentY = GetScreenHeight() / 2.0f + rot_y;

      app->wallpapers[i].render_x = currentX;
      app->wallpapers[i].render_y = currentY;

      float dx = mousePoint.x - currentX;
      float dy = mousePoint.y - currentY;

      float dx_abs = fabsf(dx);
      float dy_abs = fabsf(dy);

      float horiz_radius = layout.inradius;
      float vert_radius = HEX_RADIUS;

      if (dx_abs <= horiz_radius && dy_abs <= vert_radius) {
        if (1.73205f * dy_abs + dx_abs <= 1.73205f * HEX_RADIUS) {
          if (hoveredIndex == -1) {
            hoveredIndex = i;
          }
        }
      }

      float targetScale = (i == hoveredIndex) ? 1.15f : 1.0f;
      float targetColor = (i == hoveredIndex) ? 255.0f : 130.0f;

      app->wallpapers[i].currentScale +=
          (targetScale - app->wallpapers[i].currentScale) * 0.15f;
      app->wallpapers[i].currentColor +=
          (targetColor - app->wallpapers[i].currentColor) * 0.15f;
    }

    RenderWallpapers(app, hoveredIndex, renderer->angle);

    if (hoveredIndex != -1 && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
      result.valid = true;
      result.full_target_path =
          JoinPath(app->wp_dir, app->wallpapers[hoveredIndex].filename);

      float relX =
          app->wallpapers[hoveredIndex].render_x / (float)GetScreenWidth();
      float relY =
          (GetScreenHeight() - app->wallpapers[hoveredIndex].render_y) /
          (float)GetScreenHeight();
      result.rel_x = relX;
      result.rel_y = relY;

      break;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
      break;
    }
  }

  return result;
}
