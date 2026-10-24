#include "test-battle.com.shd.h"

#if HLSL
StructuredBuffer<glb_t> b0 : register(t0);
StructuredBuffer<pce_t> b1 : register(t1);
#endif
#define B0 B(const device glb_t * b0)
#define B1 B(const device pce_t * b1)

struct vs_out {
  float2 pp  LOC(0);
  int2   p0  LOC(1);
  int2   p1  LOC(2);
  float4 pos POS;
};
struct fs_out {
  float4 colour TGT(0);
};

vertex vs_out vs_main(B0 B1 uint vid VID, uint iid IID) {
  int2 fp = int2(vid & 1, (vid >> 1) & 1);

  int2 p0 = b1[iid].pos;
  int2 p1 = b1[iid].target;
  int2 aa = min(p0, p1);
  int2 bb = max(p0, p1) + 1;

  float2 pp = float2(aa + fp * (bb - aa));
  float ax = aspect(b0[0].scr_sz.x, b0[0].scr_sz.y);
  float ay = aspect(b0[0].scr_sz.y, b0[0].scr_sz.x);
  float2 qq = (pp - b0[0].trans) / (b0[0].scale * float2(ax, ay));

  if (all(b1[iid].pos == b1[iid].target)) qq = float2(0, 0);

  vs_out res;
  res.p0 = p0;
  res.p1 = p1;
  res.pp = pp;
  res.pos = float4(qq * float2(1, -1), 0.0f, 1.0f);
  return res;
}

fragment fs_out fs_main(vs_out vs IN) {
  float4 c = float4(0, 0, 0, 0);
  c = c_movement(c, vs.pp, vs.p0, vs.p1);

  fs_out res;
  res.colour = c;
  return res;
}
