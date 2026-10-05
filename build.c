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

#ifdef __APPLE__
#  define SHADER(x) RUN("xcrun", "-sdk", "macosx", "metal", "-DMETAL", "-x", "metal", x".h", "-o", "app.app/Contents/Resources/"x".metallib");
#elif _WIN32
static char * dxc(void) {
  char * win_kit_version = getenv("WIN_KIT_VERSION");
  if (!win_kit_version) return (fprintf(stderr, "missing environment WIN_KIT_VERSION"), NULL);

  char argv0[1024];
  snprintf(argv0, 1024,
      "c:\\Program Files (x86)\\Windows Kits\\10\\bin\\%s\\x64\\dxc.exe",
      win_kit_version);
  return strdup(argv0);
}
#define SHADER(x) \
  RUN(dxc(), "-D", "HLSL", "-T", "vs_5_0", "-E", "vs_main", x".h", "-Fo", x".vert.dxil"); \
  RUN(dxc(), "-D", "HLSL", "-T", "ps_5_0", "-E", "fs_main", x".h", "-Fo", x".frag.dxil");
#endif

int main() {
#ifdef __APPLE__
#define EXE(x) "app.app/Contents/MacOS/"x
  mkdir("app.app", 0777);
  mkdir("app.app/Contents", 0777);
  mkdir("app.app/Contents/MacOS", 0777);
  mkdir("app.app/Contents/Resources", 0777);

  RUN("clang", "-Wall", "-g", "-fmodules", "-c", "-o", "app.o", "app-osx.m");
#elif _WIN32
#define EXE(x) x".exe", x".res"
  RUN("clang", "-Wall", "-gdwarf", "-c", "-o", "app.o", "app-win.c");
#endif

  SHADER("test-battle-shader");

#if _WIN32
  RUN("llvm-rc", "/FO", "test-battle.res", "test-battle.rc");
#endif

  RUN("clang", "-Wall", "-g", "-c", "-o", "test-battle.o", "test-battle.c");
  RUN("clang", "-o", EXE("test-battle"), "app.o", "test-battle.o");
  return 0;
}

