#if HLSL
#  define LOC(n) : TEXCOORD##n
#  define POS    : SV_Position
#  define VID    : SV_VertexID
#  define IID    : SV_InstanceID
#  define TGT(n) : SV_Target##n
#  define IN
#  define BUF(...)
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
#  define VERTEX   vertex
#  define FRAGMENT fragment
#else
typedef struct { float x, y; } float2;
typedef struct { float x, y, z, w; } float4;
typedef struct { int x, y; } int2;
#endif

struct vs_in {
  float2 aspect;
  int2   hover;
  float  scale;
  float  p0, p1, p2;
};

static inline float aspect(float a, float b) {
  return (a > b) ? a / b : 1;
}
static inline float frag_pos_x(float2 p, struct vs_in vsin) {
  return vsin.scale * vsin.aspect.x * (p.x * 2.f - 1.f);
}
static inline float frag_pos_y(float2 p, struct vs_in vsin) {
  return vsin.scale * vsin.aspect.y * (p.y * 2.f - 1.f);
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

static inline float3 border(float3 c, float2 p) {
  float2 dd = step(0.98, abs(p));
  return c * (1 - 0.8 * max(dd.x, dd.y));
}

static inline float3 hover(float3 c, float2 p) {
  return c * 0.1;
}

FRAGMENT fs_out fs_main(BUF(0, const device vs_in * b0) vs_out vs IN) {
  float2 p;
  p.x = frag_pos_x(vs.frag_pos, b0[0]);
  p.y = frag_pos_y(vs.frag_pos, b0[0]);

  int2 id = int2(floor(p));
  float2 uv = fract(p) * 2 - 1;

  float3 c = mix(
      float3(0.1, 0.15, 0.2),
      float3(0.15, 0.2, 0.25),
      (id.x + id.y) & 1);

  c = border(c, uv);
  if (all(id == b0[0].hover)) c = hover(c, uv);

  fs_out res;
  res.colour = float4(c, 1);
  return res;
}
#endif
