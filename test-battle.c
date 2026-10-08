#include "g3d.h"

#include <math.h>

#include "test-battle-shader.h"

static struct vs_in g_vsin;

static g3d_buffer_t   * g_vsin_buf;
static g3d_pipeline_t * g_ppl;

int g3d_init(const g3d_init_t * t) {
  g_ppl = t->new_pipeline(t->ptr, "test-battle-shader", 1, 0);
  g_vsin_buf = t->new_buffer(t->ptr, sizeof(struct vs_in));
  g_vsin.mouse = (float2){ 1e8, 1e8 };
  g_vsin.pick = (int2){ 1e8, 1e8 };
  return g_ppl ? 0 : 1;
}
void g3d_deinit(void) {
}

static float dt = 0;
int g3d_frame(const g3d_frame_t * t) {
  dt += 0.02;
  g_vsin.scale = 6 + 2 * sin(dt);
  g_vsin.trans.x = cos(dt) * g_vsin.scale;
  g_vsin.trans.y = sin(dt) * g_vsin.scale;

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

void g3d_resize(unsigned sw, unsigned sh) {
  g_vsin.scr_sz = (float2){ sw, sh };
}

void g3d_mouse_move(int x, int y) {
  g_vsin.mouse = (float2){ x, y };
}
void g3d_mouse_down(int x, int y) {
  g3d_mouse_move(x, y);

  float2 p = { x, y };
  g_vsin.pick = frag_scr_id(p, g_vsin);
}
void g3d_mouse_up(int x, int y) {
  g3d_mouse_move(x, y);
}
void g3d_mouse_cancel(int x, int y) {
  g3d_mouse_move(x, y);
}

void g3d_key(g3d_key_t key, int down) {
}
