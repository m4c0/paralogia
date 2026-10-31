#include "g3d.h"

#include <math.h>

#include "test-battle.com.shd.h"

#define MAX_PIECES 20

static pce_t g_pces[MAX_PIECES] = {0};
static unsigned g_pces_n;

static glb_t g_glb;

static brd_t g_board[20 * 20] = {0};
static int g_board_loaded;

static g3d_buffer_t * g_buf_brd;
static g3d_buffer_t * g_buf_glb;
static g3d_buffer_t * g_buf_pce;

static g3d_pipeline_t * g_ppl_brd;
static g3d_pipeline_t * g_ppl_mov;
static g3d_pipeline_t * g_ppl_pce;

static inline float clamp(float x, float a, float b) {
  if (x < a) return a;
  if (x > b) return b;
  return x;
}

static inline brd_t * xy2brd(int x, int y) {
  return g_board + x + g_glb.brd_sz.x * y;
}
static inline brd_t * p2brd(int2 p) {
  return xy2brd(p.x, p.y);
}

int g3d_init(const g3d_init_t * t) {
  g_ppl_brd = t->new_pipeline(t->ptr, "test-battle.brd", 2, 0);
  g_ppl_mov = t->new_pipeline(t->ptr, "test-battle.mov", 2, 0);
  g_ppl_pce = t->new_pipeline(t->ptr, "test-battle.pce", 2, 0);

  g_buf_glb = t->new_buffer(t->ptr, sizeof(glb_t));
  g_glb.brd_sz = (int2){ 20, 20 };
  g_glb.mouse = (int2){ -1, -1 };
  g_glb.pick = (int2){ -1, -1 };
  g_glb.scale = 6;
  g_glb.trans = (float2){ 3, 3 };

  g_buf_brd = t->new_buffer(t->ptr, sizeof(brd_t) * 20 * 20);
  g_buf_pce = t->new_buffer(t->ptr, sizeof(pce_t) * MAX_PIECES);

  brd_t * brd = g_board;
  brd[21] = brd[22] = brd[23] = brd[24] = (brd_t){ 1 };
  brd[41] = brd[61] = brd[64] = (brd_t){ 1 };

  g_pces[0] = (pce_t) { {1,4}, {1,4}, 2, 10 };
  g_pces[1] = (pce_t) { {4,5}, {4,5}, 2, 21 };
  g_pces[2] = (pce_t) { {2,2}, {2,2}, 3, 32 };
  g_pces_n = 3;

  for (int i = 0; i < g_pces_n; i++) {
    pce_t p = g_pces[i];
    *p2brd(p.pos) = (brd_t){
      .piece = p.piece,
      .index = i,
    };
  }

  return g_ppl_brd ? 0 : 1;
}
void g3d_deinit(void) {
}

int g3d_frame(const g3d_frame_t * t) {
  if (!g_board_loaded) {
    t->load_buffer(g_buf_brd, g_board, sizeof(brd_t) * 20 * 20);
    t->load_buffer(g_buf_pce, g_pces, sizeof(pce_t) * MAX_PIECES);
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
    .buffers   = (g3d_buffer_t *[]) { g_buf_glb, g_buf_pce, 0 },
    .samplers  = (g3d_sampler_t *[]) { 0 },
    .textures  = (g3d_texture_t *[]) { 0 },
    .instances = g_pces_n,
  };
  t->render(&rnd);

  rnd = (g3d_frame_render_t) {
    .ptr       = t->ptr,
    .pipeline  = g_ppl_pce,
    .buffers   = (g3d_buffer_t *[]) { g_buf_glb, g_buf_pce, 0 },
    .samplers  = (g3d_sampler_t *[]) { 0 },
    .textures  = (g3d_texture_t *[]) { 0 },
    .instances = g_pces_n,
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

// https://bryceboe.com/2006/10/23/line-segment-intersection-algorithm/
static inline int ccw(int2 a, int2 b, int2 c) {
  return (c.y - a.y) * (b.x - a.x) > (b.y - a.y) * (c.x - a.x);
}
static inline int intersects(int2 a1, int2 a2, int2 b1, int2 b2) {
  return
    ccw(a1, b1, b2) != ccw(a2, b1, b2) &&
    ccw(a1, a2, b1) != ccw(a1, a2, b2);
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
  if (sx && sy) {
    g_glb.mouse = (int2){ -1, -1 };
    return;
  }

  brd_t * pick  = p2brd(g_glb.pick);
  brd_t * mouse = p2brd(g_glb.mouse);
  if (pick == mouse) return;

  int valid = 1;

  for (int i = 0; valid && i < g_pces_n; i++) {
    pce_t * piece = g_pces + i;
    if (p2brd(piece->pos) == pick) continue;
    if (p2brd(piece->target) == mouse) valid = 0;

    if (intersects(piece->pos, piece->target, g_glb.pick, g_glb.mouse)) valid = 0;
  }

  for (int d = 1; valid && d <= abs(dx + dy); d++) {
    int ix = g_glb.pick.x + d * sx;
    int iy = g_glb.pick.y + d * sy;
    brd_t * b = xy2brd(ix, iy);
    switch (b->piece) {
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

  brd_t * id = p2brd(g_glb.pick);
  if (id->piece == 2) {
    g_glb.mouse = (int2){ -1, -1 };
    return;
  }

  g_glb.pick = (int2){ -1, -1 };
}
void g3d_mouse_up(int x, int y) {
  if (g_glb.pick.x < 0 || g_glb.mouse.x < 0) return cancel();

  brd_t * pick  = p2brd(g_glb.pick);
  brd_t * mouse = p2brd(g_glb.mouse);

  int valid = 0;
  switch (mouse->piece) {
    case 0:
    case 3:
      valid = 1;
      break;
    case 2:
      valid = 1;
      //if (pick == mouse) valid = 1;
      break;
  }

  if (valid) {
    g_pces[pick->index].target = g_glb.mouse;
    g_board_loaded = 0;
  }

  cancel();
}
void g3d_mouse_cancel(int x, int y) {
  cancel();
}

static void place(int who) {
  if (g_glb.mouse.x < 0) return;
  if (who > 3) return;

  brd_t * m = p2brd(g_glb.mouse);
  if (who == 0) {
    if (!m->piece) return;

    pce_t * p = g_pces + m->index;
    if (p->piece == m->piece) *p = (pce_t) {0}; // Avoid wall impacting piece

    m->piece = 0;
  } else if (m->piece) {
    return;
  } else if (who == 1) {
    *m = (brd_t){
      .piece = who,
    };
  } else {
    int order = 1;
    for (int i = 0; i < g_pces_n; i++) {
      if (g_pces[i].order >= order) order = g_pces[i].order + 1;
    }
    g_pces[g_pces_n] = (pce_t) {
      .pos    = g_glb.mouse,
      .target = g_glb.mouse,
      .piece  = who,
      .order  = order,
    };
    *m = (brd_t){
      .piece = who,
      .index = g_pces_n,
    };
    g_pces_n++;
  }

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
