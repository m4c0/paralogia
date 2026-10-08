#include "g3d.h"

#include "test-battle-shader.h"

#include <math.h>

static float2 g_scr_sz;
static float2 g_mouse;

static g3d_buffer_t   * g_vsin_buf;
static g3d_pipeline_t * g_ppl;

int g3d_init(const g3d_init_t * t) {
  g_ppl = t->new_pipeline(t->ptr, "test-battle-shader", 1, 0);
  g_vsin_buf = t->new_buffer(t->ptr, sizeof(struct vs_in));
  return g_ppl ? 0 : 1;
}
void g3d_deinit(void) {
}

static inline float aspect(float a, float b) {
  return (a > b) ? a / b : 1;
}

static float dt = 0;
int g3d_frame(const g3d_frame_t * t) {
  dt += 0.02;
  float scale = 6 + 2 * sin(dt);

  float ax = aspect(g_scr_sz.x, g_scr_sz.y);
  float ay = aspect(g_scr_sz.y, g_scr_sz.x);

  float mx = scale * ax * (2.f * g_mouse.x / g_scr_sz.x - 1.f);
  float my = scale * ay * (2.f * g_mouse.y / g_scr_sz.y - 1.f);

  struct vs_in vsin = {
    .aspect = (float2){ ax, ay },
    .hover = (int2){ floorf(mx), floorf(my) },
    .scale = scale,
  };
  t->load_buffer(g_vsin_buf, &vsin, sizeof(struct vs_in));

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

void g3d_resize(unsigned sw, unsigned sh) {
  g_scr_sz = (float2){ sw, sh };
}

void g3d_mouse_move(int x, int y) {
  g_mouse = (float2){ x, y };
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
