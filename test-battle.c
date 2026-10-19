#include "g3d.h"

#include <math.h>

#include "test-battle.com.shd.h"

static glb_t g_glb;

static mov_t g_movs[20] = {0};
static brd_t g_board[20 * 20] = {0};
static int g_board_loaded;

static g3d_buffer_t * g_buf_glb;
static g3d_buffer_t * g_buf_brd;
static g3d_buffer_t * g_buf_mov;

static g3d_pipeline_t * g_ppl_brd;
static g3d_pipeline_t * g_ppl_mov;

static inline float clamp(float x, float a, float b) {
  if (x < a) return a;
  if (x > b) return b;
  return x;
}

int g3d_init(const g3d_init_t * t) {
  g_ppl_brd = t->new_pipeline(t->ptr, "test-battle", 2, 0);
  g_ppl_mov = t->new_pipeline(t->ptr, "test-battle.mov", 2, 0);

  g_buf_glb = t->new_buffer(t->ptr, sizeof(glb_t));
  g_glb.brd_sz = (int2){ 20, 20 };
  g_glb.mouse = (int2){ -1, -1 };
  g_glb.pick = (int2){ -1, -1 };
  g_glb.scale = 6;
  g_glb.trans = (float2){ 3, 3 };

  g_buf_brd = t->new_buffer(t->ptr, sizeof(brd_t) * 20 * 20);
  g_buf_mov = t->new_buffer(t->ptr, sizeof(mov_t) * 20);

  brd_t * brd = g_board;
  brd[21] = brd[22] = brd[23] = brd[24] = (brd_t){ 1 };
  brd[41] = brd[61] = brd[64] = (brd_t){ 1 };
  brd[42] = (brd_t) { 3 };
  brd[81] = (brd_t) { 2 };
  brd[104] = (brd_t) { 2 };

  g_movs[0] = (mov_t) { {3,5}, {8,5} };
  g_movs[1] = (mov_t) { {4,8}, {9,8} };

  return g_ppl_brd ? 0 : 1;
}
void g3d_deinit(void) {
}

int g3d_frame(const g3d_frame_t * t) {
  if (!g_board_loaded) {
    t->load_buffer(g_buf_mov, g_movs, sizeof(mov_t) * 20);
    t->load_buffer(g_buf_brd, g_board, sizeof(brd_t) * 20 * 20);
    g_board_loaded = 1;
  }

  g_glb.trans.x = clamp(g_glb.trans.x, 0, g_glb.brd_sz.x);
  g_glb.trans.y = clamp(g_glb.trans.y, 0, g_glb.brd_sz.y);

  t->load_buffer(g_buf_glb, &g_glb, sizeof(glb_t));

  g3d_frame_render_t rnd = {
    .ptr       = t->ptr,
    .pipeline  = g_ppl_brd,
    .buffers   = (g3d_buffer_t *[]) { g_buf_glb, g_buf_brd, 0 },
    .samplers  = (g3d_sampler_t *[]) { 0 },
    .textures  = (g3d_texture_t *[]) { 0 },
    .instances = 1,
  };
  t->render(&rnd);

  rnd = (g3d_frame_render_t) {
    .ptr       = t->ptr,
    .pipeline  = g_ppl_mov,
    .buffers   = (g3d_buffer_t *[]) { g_buf_glb, g_buf_mov, 0 },
    .samplers  = (g3d_sampler_t *[]) { 0 },
    .textures  = (g3d_texture_t *[]) { 0 },
    .instances = 2,
  };
  t->render(&rnd);

  return 0;
}

void g3d_resize(unsigned sw, unsigned sh) {
  g_glb.scr_sz = (float2){ sw, sh };
}

static inline int2 frag_scr_id(float2 p) {
  int2 res;
  res.x = (int)floor(frag_pos_x(p.x / g_glb.scr_sz.x, g_glb));
  res.y = (int)floor(frag_pos_y(p.y / g_glb.scr_sz.y, g_glb));
  if (res.x < 0 || res.x >= g_glb.brd_sz.x) res.x = res.y = -1;
  if (res.y < 0 || res.y >= g_glb.brd_sz.y) res.x = res.y = -1;
  return res;
}
static void cancel() {
  g_glb.pick = (int2){ -1, -1 };
}

static inline int abs(int n) { return n > 0 ? n : -n; }
static inline int sign(int n) { return n < 0 ? -1 : n > 0 ? 1 : 0; }
void g3d_mouse_move(int x, int y) {
  float2 p = { x, y };
  g_glb.mouse = frag_scr_id(p);
  if (g_glb.pick.x < 0) return;
  if (g_glb.mouse.x < 0) return;

  int dx = g_glb.mouse.x - g_glb.pick.x;
  int dy = g_glb.mouse.y - g_glb.pick.y;
  int sx = sign(dx);
  int sy = sign(dy);
  if ((sx && sy) || (!sx && !sy)) {
    g_glb.mouse = (int2){ -1, -1 };
    return;
  }

  int valid = 1;
  for (int d = 1; valid && d <= abs(dx + dy); d++) {
    int ix = g_glb.pick.x + d * sx;
    int iy = g_glb.pick.y + d * sy;
    int i = ix + g_glb.brd_sz.x * iy;
    switch (g_board[i].piece) {
      case 1:
      case 2: valid = 0; break;
      default: break;
    }
  }
  if (!valid) {
    g_glb.mouse = (int2){ -1, -1 };
    return;
  }
}
void g3d_mouse_down(int x, int y) {
  g3d_mouse_move(x, y);

  float2 p = { x, y };
  g_glb.pick = frag_scr_id(p);
  if (g_glb.pick.x < 0) return;

  brd_t id = g_board[g_glb.brd_sz.x * g_glb.pick.y + g_glb.pick.x];
  if (id.piece == 2) {
    g_glb.mouse = (int2){ -1, -1 };
    return;
  }

  g_glb.pick = (int2){ -1, -1 };
}
void g3d_mouse_up(int x, int y) {
  if (g_glb.pick.x < 0 || g_glb.mouse.x < 0) return cancel();

  unsigned i = g_glb.brd_sz.x * g_glb.pick.y + g_glb.pick.x;
  unsigned j = g_glb.brd_sz.x * g_glb.mouse.y + g_glb.mouse.x;
  switch (g_board[j].piece) {
    case 0:
      g_board[j] = g_board[i];
      g_board[i] = (brd_t){ 0 };
      g_board_loaded = 0;
      break;
    case 3:
      g_board[j] = (brd_t){ 0 };
      g_board_loaded = 0;
      break;
  }

  cancel();
}
void g3d_mouse_cancel(int x, int y) {
  cancel();
}

static void place(int who) {
  if (g_glb.mouse.x < 0) return;
  if (who > 3) return;

  g_board[g_glb.brd_sz.x * g_glb.mouse.y + g_glb.mouse.x] = (brd_t){ who };
  g_board_loaded = 0;
}
void g3d_key(g3d_key_t key, int down) {
  if (key == g3d_key_cancel) cancel();
  if (down && key >= g3d_key_0 && key <= g3d_key_9) place(key - g3d_key_0);
}

void g3d_scroll(float x, float y) {
  g_glb.trans.x = g_glb.trans.x - g_glb.scale * x / g_glb.scr_sz.x;
  g_glb.trans.y = g_glb.trans.y - g_glb.scale * y / g_glb.scr_sz.y;
}

void g3d_zoom(float z) {
  // TODO: zoom centered by mouse hover
  float s = g_glb.scale - z;
  g_glb.scale = clamp(s, 2, 12);
}
