#include "g3d.h"

#include "test-battle-shader.h"

#include <math.h>

static struct vs_in g_vsin;
static float2 g_scr_sz;

static g3d_buffer_t   * g_vsin_buf;
static g3d_pipeline_t * g_ppl;

int g3d_init(const g3d_init_t * t) {
  g_ppl = t->new_pipeline(t->ptr, "test-battle-shader", 1, 0);
  g_vsin_buf = t->new_buffer(t->ptr, sizeof(struct vs_in));
  g_vsin.scale = 8;
  return g_ppl ? 0 : 1;
}
void g3d_deinit(void) {
}

int g3d_frame(const g3d_frame_t * t) {
  t->load_buffer(g_vsin_buf, &g_vsin, sizeof(struct vs_in));

  g3d_frame_render_t rnd = {
    .ptr       = t->ptr,
    .pipeline  = g_ppl,
    .buffers   = (g3d_buffer_t *[]) { g_vsin_buf, 0 },
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
  g_vsin.aspect = (float2){ aspect_x(sw, sh), aspect_y(sw, sh) };
  g_scr_sz = (float2){ sw, sh };
}

void g3d_mouse_move(int x, int y) {
  float fx = g_vsin.scale * g_vsin.aspect.x * (2.f * x / g_scr_sz.x - 1.f);
  float fy = g_vsin.scale * g_vsin.aspect.y * (2.f * y / g_scr_sz.y - 1.f);
  g_vsin.hover = (int2){ floorf(fx), floorf(fy) };
}
void g3d_mouse_down(int x, int y) {
  g3d_mouse_move(x, y);
}
void g3d_mouse_up(int x, int y) {
  g3d_mouse_move(x, y);
}
void g3d_mouse_cancel(int x, int y) {
  g3d_mouse_move(x, y);
}

void g3d_key(g3d_key_t key, int down) {
}
