#include "test-battle.com.shd.h"

#if HLSL
StructuredBuffer<glb_t> b0 : register(t0);
StructuredBuffer<mov_t> b1 : register(t1);
#endif
#define B0 B(const device glb_t * b0)
#define B1 B(const device mov_t * b1)

struct vs_out {
  float2 pp  LOC(0);
  float2 p0  LOC(1);
  float2 p1  LOC(2);
  float4 pos POS;
};
struct fs_out {
  float4 colour TGT(0);
};

vertex vs_out vs_main(B0 B1 uint vid VID, uint iid IID) {
  float2 fp = float2(vid & 1, (vid >> 1) & 1);

  float2 p0 = float2(b1[iid].from) + 0.5;
  float2 p1 = float2(b1[iid].to) + 0.5;
  float2 aa = min(p0, p1) - 0.5;
  float2 bb = max(p0, p1) + 0.5;

  float2 pp = aa + fp * (bb - aa);
  float ax = aspect(b0[0].scr_sz.x, b0[0].scr_sz.y);
  float ay = aspect(b0[0].scr_sz.y, b0[0].scr_sz.x);
  float2 qq = (pp - b0[0].trans) / (b0[0].scale * float2(ax, ay));

  vs_out res;
  res.p0 = p0;
  res.p1 = p1;
  res.pp = pp;
  res.pos = float4(qq * float2(1, -1), 0.0f, 1.0f);
  return res;
}
fragment fs_out fs_main(vs_out vs IN) {
  float2 pa = vs.pp - vs.p0;
  float2 ba = vs.p1 - vs.p0;
  float h = clamp(dot(pa, ba) / dot(ba, ba), 0.f, 1.f);
  float d = step(0, length(pa - ba * h) - 0.05);

  d = min(d, step(0, length(vs.pp - vs.p1) - 0.15));

  fs_out res;
  res.colour = float4(0.5, 0, 0, 1.0) * (1 - d);
  return res;
}
