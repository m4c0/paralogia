#ifdef __APPLE__
#  include <sys/stat.h>
#  include <unistd.h>
#elif _WIN32
#  define _CRT_SECURE_NO_WARNINGS
#  define _CRT_NONSTDC_NO_WARNINGS
#  include <direct.h>
#  include <process.h>
#else
#  error Unsupported platform
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int run(char ** args) {
  assert(args && args[0]);

#ifdef __APPLE__
  pid_t pid = fork();
  if (pid == 0) {
    execvp(args[0], args);
    abort();
  } else if (pid > 0) {
    int sl = 0;
    assert(0 <= waitpid(pid, &sl, 0));
    if (WIFEXITED(sl)) return WEXITSTATUS(sl);
  }
#elif _WIN32
  if (0 == _spawnvp(_P_WAIT, args[0], (const char * const *)args)) {
    return 0;
  }
#endif

  fprintf(stderr, "failed to run child process: %s\n", args[0]);
  return 1;
}
#define RUN(...) do { char * args[] = { __VA_ARGS__, 0 }; if (run(args)) return 1; } while (0)

int main() {
#ifdef __APPLE__
#define EXE(x) "app.app/Contents/MacOS/"x
  mkdir("app.app", 0777);
  mkdir("app.app/Contents", 0777);
  mkdir("app.app/Contents/MacOS", 0777);

  RUN("clang", "-Wall", "-g", "-fmodules", "-c", "-o", "app.o", "app-osx.m");
#elif _WIN32
#define EXE(x) x".exe"
  RUN("clang", "-Wall", "-gdwarf", "-c", "-o", "app.o", "app-win.c");
#endif

  RUN("clang", "-Wall", "-g", "-c", "-o", "test-battle.o", "test-battle.c");
  RUN("clang", "-o", EXE("test-battle"), "app.o", "test-battle.o");
  return 0;
}

