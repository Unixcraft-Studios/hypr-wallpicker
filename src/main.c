#include "app.h"
#include "runtime_config.h"

#include <stdio.h>

int main(int argc, char **argv) {
  RuntimeConfig config = {0};
  if (!LoadRuntimeConfig(&config, argc, argv))
    return 2;

  int result = 0;
  if (config.show_help)
    PrintUsage();
  else if (config.check_only)
    PrintRuntimeConfig(&config);
  else
    result = AppRun(&config.app);

  if ((config.show_help || config.check_only) && fflush(stdout) == EOF) {
    perror("Error: cannot write stdout");
    result = 1;
  }

  FreeRuntimeConfig(&config);
  return result;
}
