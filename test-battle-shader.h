#if HLSL
#  define LOC(n) : TEXCOORD##n
#  define POS    : SV_Position
#  define VID    : SV_VertexID
#  define IID    : SV_InstanceID
#  define TGT(n) : SV_Target##n
#  define IN
#  define BUF(...)
#  define B(...)
#  define VERTEX   [shader("vertex")]
#  define FRAGMENT [shader("pixel")]
#  define fract frac
#  define mix   lerp
#elif METAL
#  pragma clang diagnostic ignored "-Wmissing-prototypes"
#  include <metal_stdlib>
#  include <simd/simd.h>
using namespace metal;
#  define LOC(n) [[user(loc##n)]]
#  define TGT(n) [[color(n)]]
#  define POS    [[position]]
#  define VID    [[vertex_id]]
#  define IID    [[instance_id]]
#  define IN     [[stage_in]]
#  define BUF(N, X) X [[buffer(N)]],
#  define B(N) N,
#  define VERTEX   vertex
#  define FRAGMENT fragment
#else
typedef struct { float x, y; } float2;
typedef struct { float x, y, z, w; } float4;
typedef struct { int x, y; } int2;
#endif

struct vs_in {
  int2   brd_sz;
  float2 scr_sz;
  float2 mouse;
  int2   pick;
  float2 trans;
  float  scale;
  float  p0;
};

static inline float aspect(float a, float b) {
  return (a > b) ? a / b : 1;
}
static inline float frag_pos_x(float x, struct vs_in vsin) {
  return vsin.scale * aspect(vsin.scr_sz.x, vsin.scr_sz.y) * (x * 2.f - 1.f) + vsin.trans.x;
}
static inline float frag_pos_y(float y, struct vs_in vsin) {
  return vsin.scale * aspect(vsin.scr_sz.y, vsin.scr_sz.x) * (y * 2.f - 1.f) + vsin.trans.y;
}

static inline int2 frag_scr_id(float2 p, struct vs_in vsin) {
  int2 res;
  res.x = (int)floor(frag_pos_x(p.x / vsin.scr_sz.x, vsin));
  res.y = (int)floor(frag_pos_y(p.y / vsin.scr_sz.y, vsin));
  return res;
}

#if HLSL || METAL
#if HLSL
StructuredBuffer<vs_in> b0 : register(t0);
#endif

struct vs_out {
  float2 frag_pos LOC(0);
  float4 pos      POS;
};
struct fs_out {
  float4 colour TGT(0);
};

VERTEX vs_out vs_main(uint vid VID) {
  float2 fp = float2(vid & 1, (vid >> 1) & 1);
  float2 p = fp * 2 - 1;

  vs_out res;
  res.frag_pos = fp;
  res.pos = float4(p * float2(1, -1), 0.0f, 1.0f);
  return res;
}

static inline float3 c_border(float3 c, float2 p) {
  float2 dd = step(0.98, abs(p));
  return c * (1 - 0.8 * max(dd.x, dd.y));
}

static inline float3 c_hover(float3 c, float2 p) {
  return c * 0.1;
}
static inline float3 c_pick(float3 c, float2 p) {
  return float3(c.x, c.yz * 0.1);
}

static inline float3 c_inside(
    B(const device uint  * b1)
    float3 c, float2 p, vs_in vsin) {
  if (any(p < 0)) return c;
  if (any(p > float2(vsin.brd_sz))) return c;

  int2 hp = frag_scr_id(vsin.mouse, vsin);

  int2 id = int2(floor(p));
  float2 uv = fract(p) * 2 - 1;

  c = mix(
      float3(0.1, 0.15, 0.2),
      float3(0.15, 0.2, 0.25),
      (id.x + id.y) & 1);

  c = c_border(c, uv);
  if (all(id == hp)) c = c_hover(c, uv);
  if (all(id == vsin.pick)) c = c_pick(c, uv);

  uint i = id.y * uint(vsin.brd_sz.x) + id.x;
  switch (b1[i]) {
    case 1: c = 0.3; break;
    case 2: c = mix(0.5, c, step(0, length(uv) - 0.3)); break;
    case 3: c = mix(0.8, c, step(0, length(uv) - 0.3)); break;
  }

  return c;
}

static inline float2 frag_pos(float2 p, vs_in vsin) {
  return float2(frag_pos_x(p.x, vsin), frag_pos_y(p.y, vsin));
}
FRAGMENT fs_out fs_main(
    BUF(0, const device vs_in * b0)
    BUF(1, const device uint  * b1)
    vs_out vs IN) {
  float2 p = frag_pos(vs.frag_pos, b0[0]);

  float3 c = float3(0.05, 0.10, 0.15);
  c = c_inside(B(b1) c, p, b0[0]);

  fs_out res;
  res.colour = float4(c, 1);
  return res;
}
#endif
