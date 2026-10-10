#include "g3d.h"

#include <math.h>

#include "test-battle-shader.h"

static struct vs_in g_vsin;

static int g_board_loaded;

static g3d_buffer_t   * g_vsin_buf;
static g3d_buffer_t   * g_board_buf;
static g3d_pipeline_t * g_ppl;

static inline float clamp(float x, float a, float b) {
  if (x < a) return a;
  if (x > b) return b;
  return x;
}

int g3d_init(const g3d_init_t * t) {
  g_ppl = t->new_pipeline(t->ptr, "test-battle-shader", 1, 0);

  g_vsin_buf = t->new_buffer(t->ptr, sizeof(struct vs_in));
  g_vsin.brd_sz = (int2){ 20, 20 };
  g_vsin.mouse = (float2){ 1e8, 1e8 };
  g_vsin.pick = (int2){ 1e8, 1e8 };
  g_vsin.scale = 6;
  g_vsin.trans = (float2){ 3, 3 };

  g_board_buf = t->new_buffer(t->ptr, sizeof(unsigned) * 20 * 20);

  return g_ppl ? 0 : 1;
}
void g3d_deinit(void) {
}

int g3d_frame(const g3d_frame_t * t) {
  if (!g_board_loaded) {
    unsigned brd[20 * 20] = {0};
    brd[21] = brd[22] = brd[23] = brd[24] = 1;
    brd[41] = brd[61] = brd[64] = 1;
    brd[42] = 3;
    brd[81] = 2;
    brd[104] = 2;
    t->load_buffer(g_board_buf, brd, sizeof(unsigned) * 20 * 20);
    g_board_loaded = 1;
  }

  g_vsin.trans.x = clamp(g_vsin.trans.x, 0, g_vsin.brd_sz.x);
  g_vsin.trans.y = clamp(g_vsin.trans.y, 0, g_vsin.brd_sz.y);

  t->load_buffer(g_vsin_buf, &g_vsin, sizeof(struct vs_in));

  g3d_frame_render_t rnd = {
    .ptr       = t->ptr,
    .pipeline  = g_ppl,
    .buffers   = (g3d_buffer_t *[]) { g_vsin_buf, g_board_buf, 0 },
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

void g3d_scroll(float x, float y) {
  g_vsin.trans.x = g_vsin.trans.x - g_vsin.scale * x / g_vsin.scr_sz.x;
  g_vsin.trans.y = g_vsin.trans.y - g_vsin.scale * y / g_vsin.scr_sz.y;
}

void g3d_zoom(float z) {
  // TODO: zoom centered by mouse hover
  float s = g_vsin.scale - z;
  g_vsin.scale = clamp(s, 2, 12);
}
