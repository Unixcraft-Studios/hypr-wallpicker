#define _POSIX_C_SOURCE 200809L

#include "runtime_config.h"
#include "fs.h"
#include "tomlc17.h"

#include <errno.h>
#include <float.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static SessionBackend DetectSessionBackend(void) {
  const char *session = getenv("XDG_SESSION_TYPE");
  const char *wayland = getenv("WAYLAND_DISPLAY");
  const char *x11 = getenv("DISPLAY");
  if (session != NULL) {
    if (strcmp(session, "wayland") == 0)
      return SESSION_BACKEND_WAYLAND;
    if (strcmp(session, "x11") == 0)
      return SESSION_BACKEND_X11;
  }
  if (wayland != NULL && wayland[0] != '\0')
    return SESSION_BACKEND_WAYLAND;
  if (x11 != NULL && x11[0] != '\0')
    return SESSION_BACKEND_X11;
  return SESSION_BACKEND_UNKNOWN;
}

static const char *SessionBackendName(SessionBackend backend) {
  switch (backend) {
  case SESSION_BACKEND_WAYLAND:
    return "Wayland";
  case SESSION_BACKEND_X11:
    return "X11";
  default:
    return "Unknown";
  }
}

void FreeRuntimeConfig(RuntimeConfig *config) {
  if (config != NULL) {
    free(config->wallpaper_directory);
    *config = (RuntimeConfig){0};
  }
}

static bool SetDirectory(RuntimeConfig *config, const char *path) {
  if (path[0] == '\0') {
    fprintf(stderr, "Error: wallpaper directory must not be empty\n");
    return false;
  }

  char *copy;
  if (path[0] == '/') {
    copy = strdup(path);
  } else {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
      perror("Error: cannot resolve wallpaper directory against current "
             "directory");
      return false;
    }
    copy = JoinPath(cwd, path);
  }
  if (copy == NULL) {
    fprintf(stderr, "Error: out of memory loading wallpaper directory\n");
    return false;
  }
  free(config->wallpaper_directory);
  config->wallpaper_directory = copy;
  config->app.wallpaper.directory = copy;
  return true;
}

static bool ConfigError(const char *path, const char *key,
                        const char *message) {
  fprintf(stderr, "Error: %s: %s: %s\n", path, key, message);
  return false;
}

static bool KnownKeys(toml_datum_t table, const char *const *allowed,
                      const char *path, const char *section) {
  if (table.type == TOML_UNKNOWN)
    return true;
  if (table.type != TOML_TABLE)
    return ConfigError(path, section, "expected a table");
  for (int i = 0; i < table.u.tab.size; ++i) {
    const char *key = table.u.tab.key[i];
    bool known = false;
    for (int j = 0; allowed[j] != NULL; ++j) {
      if (table.u.tab.len[i] == (int)strlen(allowed[j]) &&
          strcmp(key, allowed[j]) == 0) {
        known = true;
        break;
      }
    }
    if (!known) {
      fprintf(stderr, "Error: %s: unknown key in %s: %s\n", path, section, key);
      return false;
    }
  }
  return true;
}

static bool ConvertFloat(double number, float *out, bool nonnegative) {
  if (!isfinite(number) || fabs(number) > FLT_MAX ||
      (nonnegative && number < 0))
    return false;
  float value = (float)number;
  if (number != 0 && value == 0)
    return false;
  *out = value;
  return true;
}

static bool ReadFloat(toml_datum_t root, const char *key, float *out,
                      bool nonnegative, const char *path) {
  toml_datum_t value = toml_seek(root, key);
  if (value.type == TOML_UNKNOWN)
    return true;
  if (value.type != TOML_INT64 && value.type != TOML_FP64)
    return ConfigError(path, key, "expected a number");
  double number =
      value.type == TOML_INT64 ? (double)value.u.int64 : value.u.fp64;
  if (!ConvertFloat(number, out, nonnegative))
    return ConfigError(path, key, "number out of range or not finite");
  return true;
}

static bool ReadDirectory(RuntimeConfig *config, toml_datum_t table,
                          const char *path) {
  toml_datum_t value = toml_get(table, "directory");
  if (value.type == TOML_UNKNOWN)
    return true;
  if (value.type != TOML_STRING || value.u.str.len == 0 ||
      strlen(value.u.s) != (size_t)value.u.str.len)
    return ConfigError(path, "wallpaper.directory",
                       "expected a nonempty path string");
  if (value.u.s[0] == '/')
    return SetDirectory(config, value.u.s);
  if (strncmp(value.u.s, "~/", 2) == 0) {
    const char *home = getenv("HOME");
    char expanded[PATH_MAX];
    if (home == NULL || home[0] != '/')
      return ConfigError(path, "wallpaper.directory",
                         "~/ requires an absolute HOME");
    int n = snprintf(expanded, sizeof(expanded), "%s/%s", home, value.u.s + 2);
    if (n < 0 || (size_t)n >= sizeof(expanded))
      return ConfigError(path, "wallpaper.directory",
                         "expanded path is too long");
    return SetDirectory(config, expanded);
  }
  return ConfigError(path, "wallpaper.directory",
                     "use an absolute path or ~/path");
}

static bool ReadFile(RuntimeConfig *config, const char *path, bool optional) {
  FILE *file = fopen(path, "r");
  if (file == NULL) {
    if (optional && errno == ENOENT)
      return true;
    fprintf(stderr, "Error: cannot open runtime config '%s': %s\n", path,
            strerror(errno));
    return false;
  }
  toml_result_t parsed = toml_parse_file_named(file, path);
  bool io_ok = !ferror(file);
  if (fclose(file) != 0)
    io_ok = false;
  bool ok = false;
  if (!io_ok) {
    ConfigError(path, "read", "unable to read configuration");
    goto done;
  }
  if (!parsed.ok) {
    ConfigError(path, "TOML", parsed.errmsg);
    goto done;
  }

  static const char *const root_keys[] = {"schema_version", "wallpaper",
                                          "renderer", NULL};
  static const char *const wallpaper_keys[] = {"directory", NULL};
  static const char *const renderer_keys[] = {"cols", "spacing", "angle", NULL};
  toml_datum_t root = parsed.toptab;
  toml_datum_t wallpaper = toml_get(root, "wallpaper");
  toml_datum_t renderer = toml_get(root, "renderer");
  if (!KnownKeys(root, root_keys, path, "root") ||
      !KnownKeys(wallpaper, wallpaper_keys, path, "wallpaper") ||
      !KnownKeys(renderer, renderer_keys, path, "renderer"))
    goto done;
  toml_datum_t version = toml_get(root, "schema_version");
  if (version.type != TOML_INT64 || version.u.int64 != 1) {
    ConfigError(path, "schema_version", "expected integer 1");
    goto done;
  }
  toml_datum_t cols = toml_get(renderer, "cols");
  if (cols.type != TOML_UNKNOWN) {
    if (cols.type != TOML_INT64 || cols.u.int64 < 1 || cols.u.int64 > INT_MAX) {
      ConfigError(path, "renderer.cols", "expected a positive C int");
      goto done;
    }
    config->app.renderer.cols = (int)cols.u.int64;
  }
  ok = ReadDirectory(config, wallpaper, path) &&
       ReadFloat(root, "renderer.spacing", &config->app.renderer.spacing, true,
                 path) &&
       ReadFloat(root, "renderer.angle", &config->app.renderer.angle, false,
                 path);
done:
  toml_free(parsed);
  return ok;
}

static bool ReadSessionConfig(RuntimeConfig *config) {
  const char *session;
  switch (config->app.backend) {
  case SESSION_BACKEND_WAYLAND:
    session = "wayland";
    break;
  case SESSION_BACKEND_X11:
    session = "x11";
    break;
  default:
    return true;
  }
  const char *xdg = getenv("XDG_CONFIG_HOME");
  const char *home = getenv("HOME");
  char path[PATH_MAX];
  int n;
  if (xdg != NULL && xdg[0] == '/') {
    n = snprintf(path, sizeof(path),
                 "%s/hypr-wallpicker/runtime/runtime-%s.toml", xdg, session);
  } else if (home != NULL && home[0] == '/') {
    n = snprintf(path, sizeof(path),
                 "%s/.config/hypr-wallpicker/runtime/runtime-%s.toml", home,
                 session);
  } else {
    return true;
  }
  if (n < 0 || (size_t)n >= sizeof(path))
    return ConfigError("runtime config", "path", "path is too long");
  return ReadFile(config, path, true);
}

static bool CliNumber(const char *text, float *out, bool nonnegative) {
  char *end;
  errno = 0;
  double value = strtod(text, &end);
  if (errno != 0 || end == text || *end != '\0')
    return false;
  return ConvertFloat(value, out, nonnegative);
}

bool LoadRuntimeConfig(RuntimeConfig *config, int argc, char **argv) {
  RuntimeConfig loaded = {0};
  loaded.app.window_width = DEFAULT_WINDOW_WIDTH;
  loaded.app.window_height = DEFAULT_WINDOW_HEIGHT;
  loaded.app.renderer = (RendererConfig){DEFAULT_COLS, DEFAULT_SPACING, 0.0f};
  loaded.app.backend = DetectSessionBackend();
  RendererConfig overrides = {0};
  bool has_cols = false, has_spacing = false, has_angle = false;
  const char *explicit_path = NULL;
  static const struct option options[] = {
      {"cols", required_argument, NULL, 'c'},
      {"spacing", required_argument, NULL, 's'},
      {"angle", required_argument, NULL, 'a'},
      {"config", required_argument, NULL, 'C'},
      {"check-config", no_argument, NULL, 't'},
      {"help", no_argument, NULL, 'h'},
      {NULL, 0, NULL, 0}};
  optind = 0;
  opterr = 0;
  int option;
  while ((option = getopt_long(argc, argv, "c:s:a:C:th", options, NULL)) !=
         -1) {
    switch (option) {
    case 'c': {
      char *end;
      errno = 0;
      long value = strtol(optarg, &end, 10);
      if (errno != 0 || end == optarg || *end != '\0' || value < 1 ||
          value > INT_MAX)
        goto invalid_option;
      overrides.cols = (int)value;
      has_cols = true;
      break;
    }
    case 's':
      if (!CliNumber(optarg, &overrides.spacing, true))
        goto invalid_option;
      has_spacing = true;
      break;
    case 'a':
      if (!CliNumber(optarg, &overrides.angle, false))
        goto invalid_option;
      has_angle = true;
      break;
    case 'C':
      explicit_path = optarg;
      break;
    case 't':
      loaded.check_only = true;
      break;
    case 'h':
      loaded.show_help = true;
      break;
    default:
      fprintf(stderr, "Error: unknown or incomplete option; see --help\n");
      goto fail;
    }
  }
  if (loaded.show_help) {
    *config = loaded;
    return true;
  }
  if (argc - optind > 1) {
    fprintf(stderr, "Error: expected at most one wallpaper directory\n");
    goto fail;
  }
  if (explicit_path != NULL ? !ReadFile(&loaded, explicit_path, false)
                            : !ReadSessionConfig(&loaded))
    goto fail;
  if (has_cols)
    loaded.app.renderer.cols = overrides.cols;
  if (has_spacing)
    loaded.app.renderer.spacing = overrides.spacing;
  if (has_angle)
    loaded.app.renderer.angle = overrides.angle;
  if (optind < argc && !SetDirectory(&loaded, argv[optind]))
    goto fail;
  *config = loaded;
  return true;

invalid_option:
  fprintf(stderr, "Error: invalid value '%s' for -%c\n", optarg, option);
fail:
  FreeRuntimeConfig(&loaded);
  return false;
}

static void PrintString(const char *text) {
  putchar('"');
  for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; ++p) {
    if (*p == '"' || *p == '\\')
      printf("\\%c", *p);
    else if (*p < 0x20 || *p == 0x7f)
      printf("\\u%04X", *p);
    else
      putchar(*p);
  }
  putchar('"');
}

static void PrintFloat(const char *key, float value) {
  char number[64];
  snprintf(number, sizeof(number), "%.*g", DBL_DECIMAL_DIG, (double)value);
  printf("%s = %s%s\n", key, number,
         strpbrk(number, ".eE") == NULL ? ".0" : "");
}

void PrintRuntimeConfig(const RuntimeConfig *config) {
  printf("# Session: %s\nschema_version = 1\n",
         SessionBackendName(config->app.backend));
  if (config->app.wallpaper.directory != NULL) {
    printf("\n[wallpaper]\ndirectory = ");
    PrintString(config->app.wallpaper.directory);
    putchar('\n');
  } else {
    printf("# wallpaper.directory uses the built-in $HOME/Pictures/wallpapers "
           "default.\n");
  }
  printf("\n[renderer]\ncols = %d\n", config->app.renderer.cols);
  PrintFloat("spacing", config->app.renderer.spacing);
  PrintFloat("angle", config->app.renderer.angle);
}

void PrintUsage(void) {
  puts("Usage: wallpicker [options] [directory]\n"
       "  -c, --cols N          Positive number of columns\n"
       "  -s, --spacing PX      Non-negative finite grid spacing\n"
       "  -a, --angle DEG       Finite grid angle\n"
       "  -C, --config PATH     Read this runtime TOML instead of session "
       "config\n"
       "  -t, --check-config    Validate and print settings without opening a "
       "window\n"
       "  -h, --help            Show this help\n"
       "Precedence: built-in defaults < runtime TOML < command-line options.");
}
