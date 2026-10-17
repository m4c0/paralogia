#if HLSL
#  define LOC(n) : TEXCOORD##n
#  define POS    : SV_Position
#  define VID    : SV_VertexID
#  define IID    : SV_InstanceID
#  define TGT(n) : SV_Target##n
#  define IN
#  define B(...)
#  define vertex   [shader("vertex")]
#  define fragment [shader("pixel")]
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
#  define B(N) N,
#else
typedef struct { float x, y; } float2;
typedef struct { float x, y, z, w; } float4;
typedef struct { int x, y; } int2;
#endif
