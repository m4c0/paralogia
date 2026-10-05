#include "g3d.h"

static g3d_pipeline_t * g_ppl;

int g3d_init(const g3d_init_t * t) {
  g_ppl = t->new_pipeline(t->ptr, "test-battle-shader", 0, 0);
  return g_ppl ? 0 : 1;
}
void g3d_deinit(void) {
}

int g3d_frame(const g3d_frame_t * t) {
  return 0;
}

void g3d_resize(unsigned sw, unsigned sh) {
}

void g3d_mouse_move(int x, int y) {
}
void g3d_mouse_down(int x, int y) {
}
void g3d_mouse_up(int x, int y) {
}
void g3d_mouse_cancel(int x, int y) {
}

void g3d_key(g3d_key_t key, int down) {
}
