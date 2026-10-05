#if HLSL
#  define LOC(n) : TEXCOORD##n
#  define POS    : SV_Position
#  define VID    : SV_VertexID
#  define IID    : SV_InstanceID
#  define TGT(n) : SV_Target##n
#  define VERTEX   [shader("vertex")]
#  define FRAGMENT [shader("pixel")]
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
#  define VERTEX   vertex
#  define FRAGMENT fragment
#else
#  error Unsupported
#endif

struct vs2fs {
  float4 pos POS;
};
struct fs_out {
  float4 colour TGT(0);
};

VERTEX vs2fs vs_main(uint vid VID) {
  float2 p = float2(vid & 1, (vid >> 1) & 1);

  vs2fs stage_output;
  stage_output.pos = float4(p * 2 - 1, 0.0f, 1.0f);
  return stage_output;
}

FRAGMENT fs_out fs_main() {
  fs_out res;
  res.colour = float4(0.1, 0.2, 1, 1);
  return res;
}
