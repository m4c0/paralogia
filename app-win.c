#include "g3d.h"

#define WIN32_LEAN_AND_MEAN
#include <initguid.h> // Should come first

#include <d3dcompiler.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <stdint.h>
#include <stdio.h>
#include <windows.h>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "user32.lib")

#define BUFFER_COUNT 2

#define DEBUG_INTERFACE

#define COM(obj, method, ...) (obj)->lpVtbl->method(obj, __VA_ARGS__)
#define COM_OK(obj, method, ...) SUCCEEDED(COM(obj, method, __VA_ARGS__))
#define COM_CHK(obj, method, ...) if (FAILED(COM(obj, method, __VA_ARGS__))) return 1

static IDXGIFactory4             * d3d_factory;
static IDXGIAdapter1             * d3d_adapter;
static ID3D12Device              * d3d_device;
static ID3D12CommandQueue        * d3d_queue;
static IDXGISwapChain3           * d3d_swc;
static ID3D12DescriptorHeap      * d3d_rtv_heap;
static ID3D12CommandAllocator    * d3d_cmd_alloc;
static ID3D12GraphicsCommandList * d3d_cmd_list;

static ID3D12Resource * d3d_rt[BUFFER_COUNT];

static ID3D12Fence * d3d_fence;
static unsigned      d3d_frame_idx;
static HANDLE        d3d_fence_event;
static uint64_t      d3d_fence_value;

static unsigned d3d_swc_w;
static unsigned d3d_swc_h;

static void d3d_release(void * obj) {
  if (obj) COM((IUnknown *)obj, Release);
}

typedef void (STDMETHODCALLTYPE * d3d_get_cpu_desc_t)(ID3D12DescriptorHeap *, D3D12_CPU_DESCRIPTOR_HANDLE *);
static D3D12_CPU_DESCRIPTOR_HANDLE d3d_get_cpu_desc(ID3D12DescriptorHeap * heap) {
  D3D12_CPU_DESCRIPTOR_HANDLE h;
  ((d3d_get_cpu_desc_t)heap->lpVtbl->GetCPUDescriptorHandleForHeapStart)(heap, &h);
  return h;
}

typedef void (STDMETHODCALLTYPE * d3d_get_gpu_desc_t)(ID3D12DescriptorHeap *, D3D12_GPU_DESCRIPTOR_HANDLE *);
static D3D12_GPU_DESCRIPTOR_HANDLE d3d_get_gpu_desc(ID3D12DescriptorHeap * heap) {
  D3D12_GPU_DESCRIPTOR_HANDLE h;
  ((d3d_get_gpu_desc_t)heap->lpVtbl->GetGPUDescriptorHandleForHeapStart)(heap, &h);
  return h;
}

static int d3d_debug() {
#ifdef DEBUG_INTERFACE
  ID3D12Debug * debug;
  if (SUCCEEDED(D3D12GetDebugInterface(&IID_ID3D12Debug, (void **)&debug))) {
    COM(debug, EnableDebugLayer);
    return DXGI_CREATE_FACTORY_DEBUG;
  }
#endif
  return 0;
}

static int d3d_output_errors() {
#ifdef DEBUG_INTERFACE
  ID3D12InfoQueue * infoq;
  COM_CHK(d3d_device, QueryInterface, &IID_ID3D12InfoQueue, (void **)&infoq);
  int n = COM(infoq, GetNumStoredMessages);
  for (int i = 0; i < n; i++) {
    size_t sz = 0;
    COM(infoq, GetMessage, i, NULL, &sz);

    D3D12_MESSAGE * msg = malloc(sz); // Trusting MS sends the right size
    COM_CHK(infoq, GetMessage, i, msg, &sz);
    OutputDebugString(msg->pDescription);
    OutputDebugString("\n");
    free(msg);

    MessageBox(NULL, msg->pDescription, "Unexpected Direct3D error", MB_ICONERROR);
  }
#endif
  return 1;
}

static inline int d3d_enum_adapter_by_gpu(IDXGIFactory6 * f6, unsigned i) {
  return COM_OK(f6, EnumAdapterByGpuPreference, i, DXGI_GPU_PREFERENCE_UNSPECIFIED, &IID_IDXGIAdapter1, (void **)&d3d_adapter);
}
static inline int d3d_enum_adapter(unsigned i) {
  return COM_OK(d3d_factory, EnumAdapters1, i, &d3d_adapter);
}
static inline int d3d_adapter_is_software(void) {
  DXGI_ADAPTER_DESC1 desc;
  COM(d3d_adapter, GetDesc1, &desc);
  return desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE;
}
static inline int d3d_create_device(void) {
  return FAILED(D3D12CreateDevice((IUnknown *)d3d_adapter, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&d3d_device));
}
static int d3d_init_adapter(void) {
  IDXGIFactory6 * factory6;
  if (COM_OK(d3d_factory, QueryInterface, &IID_IDXGIFactory6, (void **)&factory6)) {
    for (unsigned i = 0; d3d_enum_adapter_by_gpu(factory6, i); i++) {
      if (d3d_adapter_is_software()) continue;
      if (0 == d3d_create_device()) return 0;
    }
  }

  for (unsigned i = 0; d3d_enum_adapter(i); i++) {
    if (d3d_adapter_is_software()) continue;
    if (0 == d3d_create_device()) return 0;
  }

  COM_CHK(d3d_factory, EnumWarpAdapter, &IID_IDXGIAdapter1, (void **)&d3d_adapter);
  return d3d_create_device();
}

static int d3d_init_queue(void) {
  D3D12_COMMAND_QUEUE_DESC desc = {0};
  COM_CHK(d3d_device, CreateCommandQueue, &desc, &IID_ID3D12CommandQueue, (void **)&d3d_queue);
  return 0;
}

static int d3d_init_swapchain(HWND hwnd, unsigned sw, unsigned sh) {
  IDXGISwapChain1 * swc;
  DXGI_SWAP_CHAIN_DESC1 desc = {
    .Width       = sw,
    .Height      = sh,
    .Format      = DXGI_FORMAT_R8G8B8A8_UNORM,
    .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
    .BufferCount = BUFFER_COUNT,
    .SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD,

    .SampleDesc = (DXGI_SAMPLE_DESC) {
      .Count = 1,
    },
  };
  COM_CHK(d3d_factory, CreateSwapChainForHwnd, (IUnknown *)d3d_queue, hwnd, &desc, NULL, NULL, &swc);
  COM_CHK(swc, QueryInterface, &IID_IDXGISwapChain3, (void **)&d3d_swc);
  return 0;
}

static int d3d_init_rtv_heap(void) {
  D3D12_DESCRIPTOR_HEAP_DESC desc = {
    .Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
    .NumDescriptors = BUFFER_COUNT,
  };
  COM_CHK(d3d_device, CreateDescriptorHeap, &desc, &IID_ID3D12DescriptorHeap, (void **)&d3d_rtv_heap);
  return 0;
}

static D3D12_CPU_DESCRIPTOR_HANDLE d3d_get_rtv_cpu_desc(int i) {
  D3D12_CPU_DESCRIPTOR_HANDLE h = d3d_get_cpu_desc(d3d_rtv_heap);
  h.ptr += i * COM(d3d_device, GetDescriptorHandleIncrementSize, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  return h;
}

static int d3d_init_rtv(void) {
  for (int i = 0; i < BUFFER_COUNT; i++) {
    COM_CHK(d3d_swc, GetBuffer, i, &IID_ID3D12Resource, (void **)&d3d_rt[i]);
    COM(d3d_device, CreateRenderTargetView, d3d_rt[i], NULL, d3d_get_rtv_cpu_desc(i));
  }
  return 0;
}

static int d3d_init_cmdlist() {
  COM_CHK(d3d_device, CreateCommandList, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d_cmd_alloc, NULL, &IID_ID3D12GraphicsCommandList, (void **)&d3d_cmd_list);
  COM_CHK(d3d_cmd_list, Close);
  return 0;
}

static void d3d_report_err(ID3DBlob * err) {
  if (!err) return;
  const char * txt = COM(err, GetBufferPointer);
  MessageBox(NULL, txt, "Direct3D error", MB_ICONERROR);
}

static ID3D12RootSignature * new_root_signature(unsigned bufs, unsigned txts) {
  D3D12_ROOT_PARAMETER params[32] = {0};
  D3D12_ROOT_PARAMETER * p = params;
  for (int i = 0; i < bufs; i++) {
    *p++ = (D3D12_ROOT_PARAMETER) {
      .ParameterType    = D3D12_ROOT_PARAMETER_TYPE_SRV,
      .Descriptor       = (D3D12_ROOT_DESCRIPTOR) {
        .ShaderRegister = i,
      },
    };
  }
  if (txts) {
    *p++ = (D3D12_ROOT_PARAMETER) {
      .ParameterType          = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
      .DescriptorTable        = {
        .NumDescriptorRanges  = 1,
        .pDescriptorRanges    = (D3D12_DESCRIPTOR_RANGE[]) {{
          .RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
          .NumDescriptors     = txts,
          .BaseShaderRegister = bufs,
        }},
      },
    };
    *p++ = (D3D12_ROOT_PARAMETER) {
      .ParameterType          = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
      .DescriptorTable        = {
        .NumDescriptorRanges  = 1,
        .pDescriptorRanges    = (D3D12_DESCRIPTOR_RANGE[]) {{
          .RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
          .NumDescriptors     = txts,
          .BaseShaderRegister = bufs,
        }},
      },
    };
  }

  ID3DBlob * blob;
  ID3DBlob * err;
  D3D12_ROOT_SIGNATURE_DESC desc = {
    .NumParameters      = p - params,
    .pParameters        = params,
  };
  if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &blob, &err))) return (d3d_report_err(err), NULL);
  if (err) return (d3d_report_err(err), NULL);

  void * root_sign;
  const void * data = COM(blob, GetBufferPointer);
  size_t        len = COM(blob, GetBufferSize);
  if (!COM_OK(d3d_device, CreateRootSignature, 0, data, len, &IID_ID3D12RootSignature, (void **)&root_sign)) return NULL;

  d3d_release(blob);
  d3d_release(err);
  return root_sign;
}

static void * d3d_shader(const char * base, const char * ext, unsigned * sz) {
  HRSRC r = FindResource(NULL, base, ext);
  HGLOBAL g = LoadResource(NULL, r);
  void * ptr = LockResource(g);
  *sz = SizeofResource(NULL, r);
  return ptr;
}

typedef struct d3d_pipeline_s {
  ID3D12RootSignature * root_sign;
  ID3D12PipelineState * pipeline;
} d3d_pipeline_t;
static void * new_pipeline(void * ptr, const char * shader, unsigned bufs, unsigned txts) {
  unsigned vss;
  void * vs = d3d_shader(shader, "vert", &vss);
  if (!vs) return NULL;

  unsigned pss;
  void * ps = d3d_shader(shader, "frag", &pss);
  if (!ps) return NULL;

  ID3D12RootSignature * root_sign = new_root_signature(bufs, txts);
  if (!root_sign) return NULL;

  D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {
    .pRootSignature        = root_sign,
    .VS                    = { vs, vss },
    .PS                    = { ps, pss },
    .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
    .SampleMask            = UINT_MAX,
    .NumRenderTargets      = 1,

    .RasterizerState = (D3D12_RASTERIZER_DESC) {
      .FillMode = D3D12_FILL_MODE_SOLID,
      .CullMode = D3D12_CULL_MODE_NONE,
    },
    .SampleDesc = (DXGI_SAMPLE_DESC) {
      .Count = 1,
    },
  };
  desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.BlendState.RenderTarget[0].BlendEnable           = TRUE;
  desc.BlendState.RenderTarget[0].SrcBlend              = D3D12_BLEND_SRC_ALPHA;
  desc.BlendState.RenderTarget[0].DestBlend             = D3D12_BLEND_INV_SRC_ALPHA;
  desc.BlendState.RenderTarget[0].BlendOp               = D3D12_BLEND_OP_ADD;
  desc.BlendState.RenderTarget[0].SrcBlendAlpha         = D3D12_BLEND_ONE;
  desc.BlendState.RenderTarget[0].DestBlendAlpha        = D3D12_BLEND_INV_SRC_ALPHA;
  desc.BlendState.RenderTarget[0].BlendOpAlpha          = D3D12_BLEND_OP_ADD;
  desc.BlendState.RenderTarget[0].LogicOp               = D3D12_LOGIC_OP_NOOP;
  desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  void * pso;
  if (!COM_OK(d3d_device, CreateGraphicsPipelineState, &desc, &IID_ID3D12PipelineState, &pso)) return (d3d_output_errors(), NULL);

  d3d_pipeline_t * res = malloc(sizeof(d3d_pipeline_t));
  res->root_sign = root_sign;
  res->pipeline  = pso;
  return res;
}

static void * new_buffer(void * ptr, int size) {
  D3D12_HEAP_PROPERTIES heap = {
    .Type = D3D12_HEAP_TYPE_UPLOAD,
  };
  D3D12_RESOURCE_DESC desc = {
    .Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER,
    .Width            = size,
    .Height           = 1,
    .DepthOrArraySize = 1,
    .MipLevels        = 1,
    .Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
    .SampleDesc       = (DXGI_SAMPLE_DESC) {
      .Count          = 1,
    },
  };
  void * res;
  if (COM_OK(d3d_device, CreateCommittedResource,
      &heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, 
      &IID_ID3D12Resource, &res)) {
    return res;
  }

  return NULL;
}

typedef struct d3d_txt_s {
  ID3D12DescriptorHeap * heap;
  ID3D12Resource       * texture;
  ID3D12Resource       * upload;
  unsigned pitch;
} d3d_txt_t;
static void * new_texture(void * ptr, int w, int h) {
  d3d_txt_t * res = malloc(sizeof(d3d_txt_t));

  D3D12_DESCRIPTOR_HEAP_DESC td_desc = {
    .NumDescriptors = 1,
    .Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
  };
  if (!COM_OK(d3d_device, CreateDescriptorHeap, &td_desc, &IID_ID3D12DescriptorHeap, (void **)&res->heap)) return NULL; 

  D3D12_HEAP_PROPERTIES heap = {
    .Type = D3D12_HEAP_TYPE_DEFAULT,
  };
  D3D12_RESOURCE_DESC res_desc = {
    .Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
    .Format           = DXGI_FORMAT_R8_UNORM,
    .Width            = w,
    .Height           = h,
    .DepthOrArraySize = 1,
    .MipLevels        = 1,
    .SampleDesc       = (DXGI_SAMPLE_DESC) {
      .Count          = 1,
    },
  };
  if (!COM_OK(d3d_device, CreateCommittedResource,
      &heap, D3D12_HEAP_FLAG_NONE, &res_desc, D3D12_RESOURCE_STATE_COPY_DEST, NULL, 
      &IID_ID3D12Resource, (void **)&res->texture)) return NULL;

  D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
    .Format                  = DXGI_FORMAT_R8_UNORM,
    .ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D,
    .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
    .Texture2D               = {
      .MipLevels             = 1,
    },
  };
  COM(d3d_device, CreateShaderResourceView, res->texture, &srv_desc, d3d_get_cpu_desc(res->heap));

  D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
  uint64_t sz;
  COM(d3d_device, GetCopyableFootprints, &res_desc, 0, 1, 0, &layout, NULL, NULL, &sz);
  res->pitch = layout.Footprint.RowPitch;

  heap = (D3D12_HEAP_PROPERTIES) {
    .Type = D3D12_HEAP_TYPE_UPLOAD,
  };
  res_desc = (D3D12_RESOURCE_DESC) {
    .Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER,
    .Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
    .Width            = sz,
    .Height           = 1,
    .DepthOrArraySize = 1,
    .MipLevels        = 1,
    .SampleDesc       = (DXGI_SAMPLE_DESC) {
      .Count          = 1,
    },
  };
  if (!COM_OK(d3d_device, CreateCommittedResource,
      &heap, D3D12_HEAP_FLAG_NONE, &res_desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, 
      &IID_ID3D12Resource, (void **)&res->upload)) return NULL;

  return res;
}

typedef struct d3d_smp_s {
  ID3D12DescriptorHeap * smp;
} d3d_smp_t;
static void * new_sampler(void * ptr, int linear) {
  D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {
    .Type           = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
    .NumDescriptors = 1,
    .Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
  };
  void * smp;
  if (!COM_OK(d3d_device, CreateDescriptorHeap, &heap_desc, &IID_ID3D12DescriptorHeap, &smp)) return NULL;

  D3D12_SAMPLER_DESC smp_desc = {
    .Filter   = linear ? D3D12_FILTER_MIN_MAG_MIP_LINEAR : D3D12_FILTER_MIN_MAG_MIP_POINT,
    .AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER,
    .AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER,
    .AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER,
  };
  COM(d3d_device, CreateSampler, &smp_desc, d3d_get_cpu_desc(smp));

  return smp;
}

static const void * load_resource(const char * name, const char * ext, unsigned * sz) {
  return NULL;
}

int d3d_init(HWND hwnd, unsigned w, unsigned h) {
  if (FAILED(CreateDXGIFactory2(d3d_debug(), &IID_IDXGIFactory4, (void **)&d3d_factory))) return 1;

  if (d3d_init_adapter()) return 1;
  if (d3d_init_queue())   return 1;

  if (d3d_init_swapchain(hwnd, w, h)) return 1;

  //COM_CHK(d3d_factory, MakeWindowAssociation, hwnd, DXGI_MWA_NO_ALT_ENTER);

  if (d3d_init_rtv_heap()) return 1;
  if (d3d_init_rtv())      return 1;

  COM_CHK(d3d_device, CreateCommandAllocator, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void **)&d3d_cmd_alloc);

  if (d3d_init_cmdlist())        return 1;

  COM_CHK(d3d_device, CreateFence, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&d3d_fence);
  d3d_fence_value = 1;
  d3d_fence_event = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (!d3d_fence_event) return 1;

  return 0;
}

static int d3d_wait() {
  uint64_t v = d3d_fence_value;
  COM_CHK(d3d_queue, Signal, d3d_fence, v);
  d3d_fence_value++;

  if (COM(d3d_fence, GetCompletedValue) < v) {
    COM(d3d_fence, SetEventOnCompletion, v, d3d_fence_event);
    WaitForSingleObject(d3d_fence_event, INFINITE);
  }

  d3d_frame_idx = COM(d3d_swc, GetCurrentBackBufferIndex);
  return 0;
}

static void d3d_deinit_rtv() {
  for (int i = 0; i < BUFFER_COUNT; i++) d3d_release(d3d_rt[i]);
  d3d_release(d3d_rtv_heap);
}
void d3d_deinit(void) {
  d3d_wait();

  d3d_deinit_rtv();

  d3d_release(d3d_fence);

  d3d_release(d3d_cmd_list);
  d3d_release(d3d_cmd_alloc);
  d3d_release(d3d_swc);
  d3d_release(d3d_queue);
  d3d_release(d3d_device);
  d3d_release(d3d_adapter);
  d3d_release(d3d_factory);

  CloseHandle(d3d_fence_event);
}

static void d3d_cmd_transition_barrier(ID3D12Resource * res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
  D3D12_RESOURCE_BARRIER b = {
    .Transition    = {
      .pResource   = res,
      .StateBefore = before,
      .StateAfter  = after,
    }
  };
  COM(d3d_cmd_list, ResourceBarrier, 1, &b);
}

static void load_buffer(g3d_buffer_t * buf, const void * data, unsigned sz) {
  void * ptr;
  if (!COM_OK((ID3D12Resource *)buf, Map, 0, NULL, &ptr)) return;
  memcpy(ptr, data, sz);
  COM((ID3D12Resource *)buf, Unmap, 0, NULL);
}

static void load_texture(g3d_texture_t * t, const void * data, unsigned w, unsigned h) {
  d3d_txt_t * txt = (d3d_txt_t *)t;

  void * ptr;
  if (!COM_OK(txt->upload, Map, 0, NULL, &ptr)) return;
  for (int y = 0; y < h; y++) {
    memcpy((char *)ptr + y * txt->pitch, (char *)data + y * w, w);
  }
  COM(txt->upload, Unmap, 0, NULL);

  D3D12_TEXTURE_COPY_LOCATION dst = {
    .pResource = txt->texture,
  };
  D3D12_TEXTURE_COPY_LOCATION src = {
    .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
    .pResource = txt->upload,
    .PlacedFootprint = {
      .Footprint = {
        .Format   = DXGI_FORMAT_R8_UNORM,
        .Width    = w,
        .Height   = h,
        .Depth    = 1,
        // Row Pitch must be a multiple of 256
        // (D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) or
        // UnrestrictedBufferTextureCopyPitchSupported must be enabled.
        .RowPitch = txt->pitch,
      },
    },
  };

  d3d_cmd_transition_barrier(txt->texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
  COM(d3d_cmd_list, CopyTextureRegion, &dst, 0, 0, 0, &src, NULL);
  d3d_cmd_transition_barrier(txt->texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

static void load_texture_file(g3d_texture_t * t, const char * file, unsigned w, unsigned h) {
  HRSRC r = FindResource(NULL, file, "img");
  HGLOBAL g = LoadResource(NULL, r);
  void * data = LockResource(g);
  load_texture(t, data, w, h);
}

static void render(const g3d_frame_render_t * t) {
  d3d_pipeline_t * ppl = t->pipeline;
  
  COM(d3d_cmd_list, SetPipelineState, ppl->pipeline);
  COM(d3d_cmd_list, SetGraphicsRootSignature, ppl->root_sign);

  int b;
  for (b = 0; t->buffers[b]; b++) {
    ID3D12Resource * buf = t->buffers[b];
    COM(d3d_cmd_list, SetGraphicsRootShaderResourceView, b, COM(buf, GetGPUVirtualAddress));
  }

  ID3D12DescriptorHeap * heaps[128];

  int tc;
  for (tc = 0; t->textures[tc] && tc < 64; tc++) {
    d3d_txt_t * txt = (d3d_txt_t *)t->textures[tc];
    heaps[tc] = txt->heap;
  }
  int sc;
  for (sc = 0; t->samplers[sc] && sc < 64; sc++) {
    heaps[tc + sc] = t->samplers[sc];
  }
  if (tc != sc) return;

  COM(d3d_cmd_list, SetDescriptorHeaps, tc + sc, heaps);

  for (int i = 0; t->textures[i] && t->samplers[i]; i++) {
    // TODO: copy if dirty
    // TODO: bind sampler
    d3d_txt_t * txt = (d3d_txt_t *)t->textures[i];
    COM(d3d_cmd_list, SetGraphicsRootDescriptorTable, b + i, d3d_get_gpu_desc(txt->heap));
    COM(d3d_cmd_list, SetGraphicsRootDescriptorTable, b + tc + i, d3d_get_gpu_desc(t->samplers[i]));
  }

  COM(d3d_cmd_list, DrawInstanced, 4, t->instances, 0, 0);
}
int d3d_frame(void) {
  d3d_wait();

  COM_CHK(d3d_cmd_alloc, Reset);
  COM_CHK(d3d_cmd_list, Reset, d3d_cmd_alloc, NULL);

  D3D12_VIEWPORT vp = { 0, 0, d3d_swc_w, d3d_swc_h };
  COM(d3d_cmd_list, RSSetViewports, 1, &vp);
  D3D12_RECT     sc = { 0, 0, d3d_swc_w, d3d_swc_h };
  COM(d3d_cmd_list, RSSetScissorRects, 1, &sc);

  d3d_cmd_transition_barrier(d3d_rt[d3d_frame_idx], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

  D3D12_CPU_DESCRIPTOR_HANDLE rtv = d3d_get_rtv_cpu_desc(d3d_frame_idx);
  COM(d3d_cmd_list, OMSetRenderTargets, 1, &rtv, FALSE, NULL);

  float colour[] = { 0.1, 0.2, 0.3, 1.0 };
  COM(d3d_cmd_list, ClearRenderTargetView, rtv, colour, 0, NULL);
  COM(d3d_cmd_list, IASetPrimitiveTopology, D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

  g3d_frame_t api = {
    .ptr               = NULL,
    .load_buffer       = load_buffer,
    .load_texture      = load_texture,
    .load_texture_file = load_texture_file,
    .render            = render,
  };
  g3d_frame(&api);

  d3d_cmd_transition_barrier(d3d_rt[d3d_frame_idx], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

  COM_CHK(d3d_cmd_list, Close);

  ID3D12CommandList * cmd_list = (ID3D12CommandList *)d3d_cmd_list;
  COM(d3d_queue, ExecuteCommandLists, 1, &cmd_list);

  COM_CHK(d3d_swc, Present, 1, 0);

  return 0;
}

static int d3d_resize(unsigned sw, unsigned sh) {
  if (d3d_swc) {
    d3d_wait();

    d3d_deinit_rtv();

    COM_CHK(d3d_swc, ResizeBuffers, BUFFER_COUNT, sw, sh, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SWAP_EFFECT_FLIP_DISCARD);

    if (d3d_init_rtv_heap()) return 1;
    if (d3d_init_rtv())      return 1;
  }

  d3d_swc_w = sw;
  d3d_swc_h = sh;

  g3d_resize(sw, sh);
  return 0;
}

static int g_last_drag_x = 1e8;
static int g_last_drag_y = 1e8;
static LRESULT window_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
  switch (msg) {
    int state = 0;

    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;

    case WM_MOUSEMOVE:
      if (w_param & (MK_MBUTTON | MK_RBUTTON)) {
        // TODO detect mouse-out events
        int x = LOWORD(l_param);
        int y = HIWORD(l_param);
        if (g_last_drag_x != 1e8) g3d_scroll(g_last_drag_x - x, g_last_drag_y - y);
        g_last_drag_x = x;
        g_last_drag_y = y;
      }

      g3d_mouse_move(LOWORD(l_param), HIWORD(l_param));
      return 0;
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
      g_last_drag_x = LOWORD(l_param);
      g_last_drag_y = HIWORD(l_param);
      return 0;

    case WM_LBUTTONDOWN:
      g3d_mouse_down(LOWORD(l_param), HIWORD(l_param));
      return 0;
    case WM_LBUTTONUP:
      g3d_mouse_up(LOWORD(l_param), HIWORD(l_param));

    case WM_MOUSEWHEEL:
      g3d_zoom((float)GET_WHEEL_DELTA_WPARAM(w_param) / (float)WHEEL_DELTA);
      return 0;

    case WM_KEYDOWN:
      if (HIWORD(l_param) & KF_REPEAT) return 0;
      state = 1;

    case WM_KEYUP:
      switch (LOWORD(w_param)) {
        case VK_LEFT:   g3d_key(g3d_key_left,   state); break;
        case VK_RIGHT:  g3d_key(g3d_key_right,  state); break;
        case VK_UP:     g3d_key(g3d_key_up,     state); break;
        case VK_DOWN:   g3d_key(g3d_key_down,   state); break;
        case VK_SPACE:  g3d_key(g3d_key_action, state); break;
        case VK_ESCAPE: g3d_key(g3d_key_cancel, state); break;
      }
      return 0;

    case WM_SIZE:
      if (d3d_resize(LOWORD(l_param), HIWORD(l_param))) PostQuitMessage(1);
      return 0;

    case WM_PAINT:
      if (d3d_frame()) PostQuitMessage(1);
      return 0;
  }
  return DefWindowProc(hwnd, msg, w_param, l_param);
}

int WinMain(HINSTANCE h_instance, HINSTANCE h_prev, LPSTR cmd_line, int cmd_show) {
  HICON h_icon = LoadIcon(h_instance, "IDI_APPICON");

  WNDCLASSEX wcex  = {
    .cbSize        = sizeof(WNDCLASSEX),
    .style         = CS_HREDRAW | CS_VREDRAW,
    .lpfnWndProc   = &window_proc,
    .hInstance     = h_instance,
    .hIcon         = h_icon,
    .hCursor       = LoadCursor(NULL, IDC_ARROW),
    .hbrBackground = (HBRUSH)(COLOR_WINDOW + 1),
    .lpszClassName = "m4c0-window",
    .hIconSm       = h_icon,
  };
  if (!RegisterClassEx(&wcex)) {
    MessageBox(NULL, "Failed to register window class", "Unhandled error", 0);
    return 1;
  }

  char title[256];
  LoadString(h_instance, 101, title, sizeof(title));

  HWND hwnd = CreateWindow(
      "m4c0-window", title,
      WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
      800, 600, 
      NULL, NULL, h_instance, NULL);
  if (!hwnd) {
    MessageBox(NULL, "Failed to create window", "Unhandled error", 0);
    return 1;
  }

  RECT rect;
  GetClientRect(hwnd, &rect);
  int sw = rect.right - rect.left;
  int sh = rect.bottom - rect.top;

  if (d3d_init(hwnd, sw, sh)) return 1;

  g3d_init_t api = {
    .ptr           = NULL,
    .new_buffer    = new_buffer,
    .new_pipeline  = new_pipeline,
    .new_sampler   = new_sampler,
    .new_texture   = new_texture,
    .load_resource = load_resource,
  };
  if (g3d_init(&api)) return 1;

  if (d3d_resize(sw, sh)) return 1;

  ShowWindow(hwnd, cmd_show);
  UpdateWindow(hwnd);

  MSG msg;
  while (GetMessage(&msg, 0, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  g3d_deinit();
  d3d_deinit();
  return msg.wParam;
}
