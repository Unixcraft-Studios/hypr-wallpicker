#ifndef RUNTIME_CONFIG_H
#define RUNTIME_CONFIG_H

#include "app.h"

typedef struct {
  AppConfig app;
  char *wallpaper_directory;
  bool check_only;
  bool show_help;
} RuntimeConfig;

bool LoadRuntimeConfig(RuntimeConfig *config, int argc, char **argv);
void FreeRuntimeConfig(RuntimeConfig *config);
void PrintRuntimeConfig(const RuntimeConfig *config);
void PrintUsage(void);

#endif
