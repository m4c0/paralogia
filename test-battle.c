#include "g3d.h"

#include <math.h>

#include "test-battle-shader.h"

static struct vs_in g_vsin;

static brd_t g_board[20 * 20] = {0};
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
  g_ppl = t->new_pipeline(t->ptr, "test-battle-shader", 2, 0);

  g_vsin_buf = t->new_buffer(t->ptr, sizeof(struct vs_in));
  g_vsin.brd_sz = (int2){ 20, 20 };
  g_vsin.mouse = (int2){ -1, -1 };
  g_vsin.pick = (int2){ -1, -1 };
  g_vsin.scale = 6;
  g_vsin.trans = (float2){ 3, 3 };

  g_board_buf = t->new_buffer(t->ptr, sizeof(int2) * 20 * 20);

  brd_t * brd = g_board;
  brd[21] = brd[22] = brd[23] = brd[24] = (brd_t){ 1 };
  brd[41] = brd[61] = brd[64] = (brd_t){ 1 };
  brd[42] = (brd_t) { 3 };
  brd[81] = (brd_t) { 2 };
  brd[104] = (brd_t) { 2 };

  return g_ppl ? 0 : 1;
}
void g3d_deinit(void) {
}

int g3d_frame(const g3d_frame_t * t) {
  if (!g_board_loaded) {
    t->load_buffer(g_board_buf, g_board, sizeof(int2) * 20 * 20);
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

static inline int2 frag_scr_id(float2 p) {
  int2 res;
  res.x = (int)floor(frag_pos_x(p.x / g_vsin.scr_sz.x, g_vsin));
  res.y = (int)floor(frag_pos_y(p.y / g_vsin.scr_sz.y, g_vsin));
  if (res.x < 0 || res.x >= g_vsin.brd_sz.x) res.x = res.y = -1;
  if (res.y < 0 || res.y >= g_vsin.brd_sz.y) res.x = res.y = -1;
  return res;
}
static void cancel() {
  g_vsin.pick = (int2){ -1, -1 };
}

static inline int abs(int n) { return n > 0 ? n : -n; }
static inline int sign(int n) { return n < 0 ? -1 : n > 0 ? 1 : 0; }
void g3d_mouse_move(int x, int y) {
  float2 p = { x, y };
  g_vsin.mouse = frag_scr_id(p);
  if (g_vsin.pick.x < 0) return;
  if (g_vsin.mouse.x < 0) return;

  int dx = g_vsin.mouse.x - g_vsin.pick.x;
  int dy = g_vsin.mouse.y - g_vsin.pick.y;
  int sx = sign(dx);
  int sy = sign(dy);
  if ((sx && sy) || (!sx && !sy)) {
    g_vsin.mouse = (int2){ -1, -1 };
    return;
  }

  int valid = 1;
  for (int d = 1; valid && d <= abs(dx + dy); d++) {
    int ix = g_vsin.pick.x + d * sx;
    int iy = g_vsin.pick.y + d * sy;
    int i = ix + g_vsin.brd_sz.x * iy;
    switch (g_board[i].piece) {
      case 1:
      case 2: valid = 0; break;
      default: break;
    }
  }
  if (!valid) {
    g_vsin.mouse = (int2){ -1, -1 };
    return;
  }
}
void g3d_mouse_down(int x, int y) {
  g3d_mouse_move(x, y);

  float2 p = { x, y };
  g_vsin.pick = frag_scr_id(p);
  if (g_vsin.pick.x < 0) return;

  brd_t id = g_board[g_vsin.brd_sz.x * g_vsin.pick.y + g_vsin.pick.x];
  if (id.piece == 2) {
    g_vsin.mouse = (int2){ -1, -1 };
    return;
  }

  g_vsin.pick = (int2){ -1, -1 };
}
void g3d_mouse_up(int x, int y) {
  if (g_vsin.pick.x < 0 || g_vsin.mouse.x < 0) return cancel();

  unsigned i = g_vsin.brd_sz.x * g_vsin.pick.y + g_vsin.pick.x;
  unsigned j = g_vsin.brd_sz.x * g_vsin.mouse.y + g_vsin.mouse.x;
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
  if (g_vsin.mouse.x < 0) return;
  if (who > 3) return;

  g_board[g_vsin.brd_sz.x * g_vsin.mouse.y + g_vsin.mouse.x] = (brd_t){ who };
  g_board_loaded = 0;
}
void g3d_key(g3d_key_t key, int down) {
  if (key == g3d_key_cancel) cancel();
  if (down && key >= g3d_key_0 && key <= g3d_key_9) place(key - g3d_key_0);
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
