#include "g3d.h"

#include "test-battle-shader.h"

static float2 g_aspect;

static g3d_buffer_t   * g_vsin;
static g3d_pipeline_t * g_ppl;

int g3d_init(const g3d_init_t * t) {
  g_ppl = t->new_pipeline(t->ptr, "test-battle-shader", 1, 0);
  g_vsin = t->new_buffer(t->ptr, sizeof(struct vs_in));
  return g_ppl ? 0 : 1;
}
void g3d_deinit(void) {
}

int g3d_frame(const g3d_frame_t * t) {
  struct vs_in vsin = {
    .aspect = g_aspect,
  };
  t->load_buffer(g_vsin, &vsin, sizeof(struct vs_in));

  g3d_frame_render_t rnd = {
    .ptr       = t->ptr,
    .pipeline  = g_ppl,
    .buffers   = (g3d_buffer_t *[]) { g_vsin, 0 },
    .samplers  = (g3d_sampler_t *[]) { 0 },
    .textures  = (g3d_texture_t *[]) { 0 },
    .instances = 1,
  };
  t->render(&rnd);

  return 0;
}

static inline float aspect_x(float sw, float sh) {
  float a = sw / sh;
  return a > 1 ? a : 1;
}
static inline float aspect_y(float sw, float sh) {
  float a = sh / sw;
  return a > 1 ? a : 1;
}
void g3d_resize(unsigned sw, unsigned sh) {
  g_aspect = (float2){ aspect_x(sw, sh), aspect_y(sw, sh) };
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
