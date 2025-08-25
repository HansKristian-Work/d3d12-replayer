/* Copyright (c) 2025 Hans-Kristian Arntzen for Valve Corporation
 * SPDX-License-Identifier: MIT
 */

#define NOMINMAX

#define INITGUID

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#else
#define VK_USE_PLATFORM_XCB_KHR
#define VK_USE_PLATFORM_XLIB_KHR
#define VK_USE_PLATFORM_WAYLAND_KHR
#endif
#include "vulkan/vulkan.h"

#undef Bool
#undef None

#ifdef _MSC_VER
#define __C89_NAMELESS
#define __C89_NAMELESSUNIONNAME
#endif

#include "vkd3d_windows.h"
#include "vkd3d_d3d12.h"
#include "vkd3d_d3d12sdklayers.h"
#include "vkd3d_dxgi1_5.h"
#include "vkd3d_swapchain_factory.h"

#include "cli_parser.hpp"
#include "com_ptr.hpp"
#include "logging.hpp"
#include "path_utils.hpp"
#include <string>
#include <vector>
#include <algorithm>
#include <stdint.h>

#include "SDL3/SDL.h"

using Granite::Path::relpath;

#ifdef _WIN32
#ifdef _MSC_VER
#define DLLEXPORT __declspec(dllexport) extern
#else
#define DLLEXPORT __attribute__((visibility("default")))
#endif
extern "C" {
DLLEXPORT const UINT D3D12SDKVersion = 616;
DLLEXPORT const char *D3D12SDKPath = u8".\\D3D12\\";
}
#define dlopen(path, mode) (void *)LoadLibraryA(path)
#define dlsym(module, sym) (void *)GetProcAddress((HMODULE)module, sym)
#else
#include <dlfcn.h>
#endif

#ifndef RAPIDJSON_HAS_STDSTRING
#define RAPIDJSON_HAS_STDSTRING 1
#endif

#ifndef RAPIDJSON_PARSE_DEFAULT_FLAGS
#define RAPIDJSON_PARSE_DEFAULT_FLAGS (kParseIterativeFlag)
#endif

#include <exception>
#undef RAPIDJSON_ASSERT
#define RAPIDJSON_ASSERT(x) do { \
	if (!(x)) { \
        LOGE("Rapidjson assert: %s\n", #x); std::terminate(); \
    } \
} while(0)

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/writer.h"

#define MAP(prefix, x) if (strcmp(str, #x) == 0) return prefix##_##x
#define MAP_DIMENSION(x) MAP(D3D12_RESOURCE_DIMENSION, x)
#define MAP_SRV_DIMENSION(x) MAP(D3D12_SRV_DIMENSION, x)
#define MAP_UAV_DIMENSION(x) MAP(D3D12_UAV_DIMENSION, x)
#define MAP_FORMAT(x) MAP(DXGI_FORMAT, x)
#define MAP_BUFFER_SRV_FLAGS(x) MAP(D3D12_BUFFER_SRV_FLAG, x)
#define MAP_BUFFER_UAV_FLAGS(x) MAP(D3D12_BUFFER_UAV_FLAG, x)
#define MAP_TEXTURE_ADDRESS_MODE(x) MAP(D3D12_TEXTURE_ADDRESS_MODE, x)
#define MAP_COMPARISON_FUNC(x) MAP(D3D12_COMPARISON_FUNC, x)
#define MAP_FILTER(x) MAP(D3D12_FILTER, x)

static D3D12_RESOURCE_DIMENSION convert_resource_dimension(const char *str)
{
	MAP_DIMENSION(BUFFER);
	MAP_DIMENSION(TEXTURE1D);
	MAP_DIMENSION(TEXTURE2D);
	MAP_DIMENSION(TEXTURE3D);
	LOGE("Unrecognized resource dimension \"%s\"\n", str);
	return D3D12_RESOURCE_DIMENSION_UNKNOWN;
}

static D3D12_SRV_DIMENSION convert_srv_dimension(const char *str)
{
	MAP_SRV_DIMENSION(BUFFER);
	MAP_SRV_DIMENSION(TEXTURE1D);
	MAP_SRV_DIMENSION(TEXTURE2D);
	MAP_SRV_DIMENSION(TEXTURE3D);
	MAP_SRV_DIMENSION(TEXTURE1DARRAY);
	MAP_SRV_DIMENSION(TEXTURE2DARRAY);
	MAP_SRV_DIMENSION(TEXTURE2DMS);
	MAP_SRV_DIMENSION(TEXTURE2DMSARRAY);
	MAP_SRV_DIMENSION(TEXTURECUBE);
	MAP_SRV_DIMENSION(TEXTURECUBEARRAY);
	LOGE("Unrecognized SRV dimension \"%s\"\n", str);
	return D3D12_SRV_DIMENSION_UNKNOWN;
}

static D3D12_UAV_DIMENSION convert_uav_dimension(const char *str)
{
	MAP_UAV_DIMENSION(BUFFER);
	MAP_UAV_DIMENSION(TEXTURE1D);
	MAP_UAV_DIMENSION(TEXTURE2D);
	MAP_UAV_DIMENSION(TEXTURE3D);
	MAP_UAV_DIMENSION(TEXTURE1DARRAY);
	MAP_UAV_DIMENSION(TEXTURE2DARRAY);
	MAP_UAV_DIMENSION(TEXTURE2DMS);
	MAP_UAV_DIMENSION(TEXTURE2DMSARRAY);
	LOGE("Unrecognized UAV dimension \"%s\"\n", str);
	return D3D12_UAV_DIMENSION_UNKNOWN;
}

static DXGI_FORMAT convert_dxgi_format(const char *str)
{
	MAP_FORMAT(UNKNOWN);
	MAP_FORMAT(R32G32B32A32_TYPELESS);
	MAP_FORMAT(R32G32B32A32_FLOAT);
	MAP_FORMAT(R32G32B32A32_UINT);
	MAP_FORMAT(R32G32B32A32_SINT);
	MAP_FORMAT(R32G32B32_TYPELESS);
	MAP_FORMAT(R32G32B32_FLOAT);
	MAP_FORMAT(R32G32B32_UINT);
	MAP_FORMAT(R32G32B32_SINT);
	MAP_FORMAT(R16G16B16A16_TYPELESS);
	MAP_FORMAT(R16G16B16A16_FLOAT);
	MAP_FORMAT(R16G16B16A16_UNORM);
	MAP_FORMAT(R16G16B16A16_UINT);
	MAP_FORMAT(R16G16B16A16_SNORM);
	MAP_FORMAT(R16G16B16A16_SINT);
	MAP_FORMAT(R32G32_TYPELESS);
	MAP_FORMAT(R32G32_FLOAT);
	MAP_FORMAT(R32G32_UINT);
	MAP_FORMAT(R32G32_SINT);
	MAP_FORMAT(R32G8X24_TYPELESS);
	MAP_FORMAT(D32_FLOAT_S8X24_UINT);
	MAP_FORMAT(R32_FLOAT_X8X24_TYPELESS);
	MAP_FORMAT(X32_TYPELESS_G8X24_UINT);
	MAP_FORMAT(R10G10B10A2_TYPELESS);
	MAP_FORMAT(R10G10B10A2_UNORM);
	MAP_FORMAT(R10G10B10A2_UINT);
	MAP_FORMAT(R11G11B10_FLOAT);
	MAP_FORMAT(R8G8B8A8_TYPELESS);
	MAP_FORMAT(R8G8B8A8_UNORM);
	MAP_FORMAT(R8G8B8A8_UNORM_SRGB);
	MAP_FORMAT(R8G8B8A8_UINT);
	MAP_FORMAT(R8G8B8A8_SNORM);
	MAP_FORMAT(R8G8B8A8_SINT);
	MAP_FORMAT(R16G16_TYPELESS);
	MAP_FORMAT(R16G16_FLOAT);
	MAP_FORMAT(R16G16_UNORM);
	MAP_FORMAT(R16G16_UINT);
	MAP_FORMAT(R16G16_SNORM);
	MAP_FORMAT(R16G16_SINT);
	MAP_FORMAT(R32_TYPELESS);
	MAP_FORMAT(D32_FLOAT);
	MAP_FORMAT(R32_FLOAT);
	MAP_FORMAT(R32_UINT);
	MAP_FORMAT(R32_SINT);
	MAP_FORMAT(R24G8_TYPELESS);
	MAP_FORMAT(D24_UNORM_S8_UINT);
	MAP_FORMAT(R24_UNORM_X8_TYPELESS);
	MAP_FORMAT(X24_TYPELESS_G8_UINT);
	MAP_FORMAT(R8G8_TYPELESS);
	MAP_FORMAT(R8G8_UNORM);
	MAP_FORMAT(R8G8_UINT);
	MAP_FORMAT(R8G8_SNORM);
	MAP_FORMAT(R8G8_SINT);
	MAP_FORMAT(R16_TYPELESS);
	MAP_FORMAT(R16_FLOAT);
	MAP_FORMAT(D16_UNORM);
	MAP_FORMAT(R16_UNORM);
	MAP_FORMAT(R16_UINT);
	MAP_FORMAT(R16_SNORM);
	MAP_FORMAT(R16_SINT);
	MAP_FORMAT(R8_TYPELESS);
	MAP_FORMAT(R8_UNORM);
	MAP_FORMAT(R8_UINT);
	MAP_FORMAT(R8_SNORM);
	MAP_FORMAT(R8_SINT);
	MAP_FORMAT(A8_UNORM);
	MAP_FORMAT(R1_UNORM);
	MAP_FORMAT(R9G9B9E5_SHAREDEXP);
	MAP_FORMAT(R8G8_B8G8_UNORM);
	MAP_FORMAT(G8R8_G8B8_UNORM);
	MAP_FORMAT(BC1_TYPELESS);
	MAP_FORMAT(BC1_UNORM);
	MAP_FORMAT(BC1_UNORM_SRGB);
	MAP_FORMAT(BC2_TYPELESS);
	MAP_FORMAT(BC2_UNORM);
	MAP_FORMAT(BC2_UNORM_SRGB);
	MAP_FORMAT(BC3_TYPELESS);
	MAP_FORMAT(BC3_UNORM);
	MAP_FORMAT(BC3_UNORM_SRGB);
	MAP_FORMAT(BC4_TYPELESS);
	MAP_FORMAT(BC4_UNORM);
	MAP_FORMAT(BC4_SNORM);
	MAP_FORMAT(BC5_TYPELESS);
	MAP_FORMAT(BC5_UNORM);
	MAP_FORMAT(BC5_SNORM);
	MAP_FORMAT(B5G6R5_UNORM);
	MAP_FORMAT(B5G5R5A1_UNORM);
	MAP_FORMAT(B8G8R8A8_UNORM);
	MAP_FORMAT(B8G8R8X8_UNORM);
	MAP_FORMAT(R10G10B10_XR_BIAS_A2_UNORM);
	MAP_FORMAT(B8G8R8A8_TYPELESS);
	MAP_FORMAT(B8G8R8A8_UNORM_SRGB);
	MAP_FORMAT(B8G8R8X8_TYPELESS);
	MAP_FORMAT(B8G8R8X8_UNORM_SRGB);
	MAP_FORMAT(BC6H_TYPELESS);
	MAP_FORMAT(BC6H_UF16);
	MAP_FORMAT(BC6H_SF16);
	MAP_FORMAT(BC7_TYPELESS);
	MAP_FORMAT(BC7_UNORM);
	MAP_FORMAT(BC7_UNORM_SRGB);
	MAP_FORMAT(AYUV);
	MAP_FORMAT(Y410);
	MAP_FORMAT(Y416);
	MAP_FORMAT(NV12);
	MAP_FORMAT(P010);
	MAP_FORMAT(P016);
	MAP_FORMAT(420_OPAQUE);
	MAP_FORMAT(YUY2);
	MAP_FORMAT(Y210);
	MAP_FORMAT(Y216);
	MAP_FORMAT(NV11);
	MAP_FORMAT(AI44);
	MAP_FORMAT(IA44);
	MAP_FORMAT(P8);
	MAP_FORMAT(A8P8);
	MAP_FORMAT(B4G4R4A4_UNORM);
	MAP_FORMAT(P208);
	MAP_FORMAT(V208);
	MAP_FORMAT(V408);
	MAP_FORMAT(A4B4G4R4_UNORM);
	LOGE("Unrecognized format \"%s\"\n", str);
	return DXGI_FORMAT_UNKNOWN;
}

static void get_block_dimensions(DXGI_FORMAT format, uint32_t &width, uint32_t &height)
{
	switch (format)
	{
	case DXGI_FORMAT_BC1_TYPELESS:
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
	case DXGI_FORMAT_BC2_TYPELESS:
	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
	case DXGI_FORMAT_BC3_TYPELESS:
	case DXGI_FORMAT_BC3_UNORM:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
	case DXGI_FORMAT_BC4_TYPELESS:
	case DXGI_FORMAT_BC4_UNORM:
	case DXGI_FORMAT_BC4_SNORM:
	case DXGI_FORMAT_BC5_TYPELESS:
	case DXGI_FORMAT_BC5_UNORM:
	case DXGI_FORMAT_BC5_SNORM:
	case DXGI_FORMAT_BC6H_SF16:
	case DXGI_FORMAT_BC6H_UF16:
	case DXGI_FORMAT_BC6H_TYPELESS:
	case DXGI_FORMAT_BC7_TYPELESS:
	case DXGI_FORMAT_BC7_UNORM:
	case DXGI_FORMAT_BC7_UNORM_SRGB:
		width = 4;
		height = 4;
		break;

	default:
		width = 1;
		height = 1;
		break;
	}
}

static D3D12_BUFFER_SRV_FLAGS convert_buffer_srv_flags(const char *str)
{
	MAP_BUFFER_SRV_FLAGS(NONE);
	MAP_BUFFER_SRV_FLAGS(RAW);
	LOGE("Unrecognized SRV flags \"%s\"\n", str);
	return D3D12_BUFFER_SRV_FLAG_NONE;
}

static D3D12_BUFFER_UAV_FLAGS convert_buffer_uav_flags(const char *str)
{
	MAP_BUFFER_UAV_FLAGS(NONE);
	MAP_BUFFER_UAV_FLAGS(RAW);
	LOGE("Unrecognized UAV flags \"%s\"\n", str);
	return D3D12_BUFFER_UAV_FLAG_NONE;
}

static D3D12_TEXTURE_ADDRESS_MODE convert_texture_address_mode(const char *str)
{
	MAP_TEXTURE_ADDRESS_MODE(CLAMP);
	MAP_TEXTURE_ADDRESS_MODE(WRAP);
	MAP_TEXTURE_ADDRESS_MODE(MIRROR);
	MAP_TEXTURE_ADDRESS_MODE(MIRROR_ONCE);
	MAP_TEXTURE_ADDRESS_MODE(BORDER);
	LOGE("Unrecognized address mode \"%s\"\n", str);
	return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
}

static D3D12_COMPARISON_FUNC convert_comparison_func(const char *str)
{
	MAP_COMPARISON_FUNC(LESS);
	MAP_COMPARISON_FUNC(LESS_EQUAL);
	MAP_COMPARISON_FUNC(EQUAL);
	MAP_COMPARISON_FUNC(NOT_EQUAL);
	MAP_COMPARISON_FUNC(ALWAYS);
	MAP_COMPARISON_FUNC(NEVER);
	MAP_COMPARISON_FUNC(GREATER);
	MAP_COMPARISON_FUNC(GREATER_EQUAL);
	LOGE("Unrecognized comparison func \"%s\"\n", str);
	return D3D12_COMPARISON_FUNC_ALWAYS;
}

static D3D12_FILTER convert_filter(const char *str)
{
	MAP_FILTER(MIN_MAG_MIP_POINT);
	MAP_FILTER(MIN_MAG_POINT_MIP_LINEAR);
	MAP_FILTER(MIN_POINT_MAG_LINEAR_MIP_POINT);
	MAP_FILTER(MIN_POINT_MAG_MIP_LINEAR);
	MAP_FILTER(MIN_LINEAR_MAG_MIP_POINT);
	MAP_FILTER(MIN_LINEAR_MAG_POINT_MIP_LINEAR);
	MAP_FILTER(MIN_MAG_LINEAR_MIP_POINT);
	MAP_FILTER(MIN_MAG_MIP_LINEAR);
	MAP_FILTER(MIN_MAG_ANISOTROPIC_MIP_POINT);
	MAP_FILTER(ANISOTROPIC);
	MAP_FILTER(COMPARISON_MIN_MAG_MIP_POINT);
	MAP_FILTER(COMPARISON_MIN_MAG_POINT_MIP_LINEAR);
	MAP_FILTER(COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT);
	MAP_FILTER(COMPARISON_MIN_POINT_MAG_MIP_LINEAR);
	MAP_FILTER(COMPARISON_MIN_LINEAR_MAG_MIP_POINT);
	MAP_FILTER(COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR);
	MAP_FILTER(COMPARISON_MIN_MAG_LINEAR_MIP_POINT);
	MAP_FILTER(COMPARISON_MIN_MAG_MIP_LINEAR);
	MAP_FILTER(COMPARISON_MIN_MAG_ANISOTROPIC_MIP_POINT);
	MAP_FILTER(COMPARISON_ANISOTROPIC);
	MAP_FILTER(MINIMUM_MIN_MAG_MIP_POINT);
	MAP_FILTER(MINIMUM_MIN_MAG_POINT_MIP_LINEAR);
	MAP_FILTER(MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT);
	MAP_FILTER(MINIMUM_MIN_POINT_MAG_MIP_LINEAR);
	MAP_FILTER(MINIMUM_MIN_LINEAR_MAG_MIP_POINT);
	MAP_FILTER(MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR);
	MAP_FILTER(MINIMUM_MIN_MAG_LINEAR_MIP_POINT);
	MAP_FILTER(MINIMUM_MIN_MAG_MIP_LINEAR);
	MAP_FILTER(MINIMUM_MIN_MAG_ANISOTROPIC_MIP_POINT);
	MAP_FILTER(MINIMUM_ANISOTROPIC);
	MAP_FILTER(MAXIMUM_MIN_MAG_MIP_POINT);
	MAP_FILTER(MAXIMUM_MIN_MAG_POINT_MIP_LINEAR);
	MAP_FILTER(MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT);
	MAP_FILTER(MAXIMUM_MIN_POINT_MAG_MIP_LINEAR);
	MAP_FILTER(MAXIMUM_MIN_LINEAR_MAG_MIP_POINT);
	MAP_FILTER(MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR);
	MAP_FILTER(MAXIMUM_MIN_MAG_LINEAR_MIP_POINT);
	MAP_FILTER(MAXIMUM_MIN_MAG_MIP_LINEAR);
	MAP_FILTER(MAXIMUM_MIN_MAG_ANISOTROPIC_MIP_POINT);
	MAP_FILTER(MAXIMUM_ANISOTROPIC);

	LOGE("Unrecognized filter \"%s\"\n", str);
	return D3D12_FILTER_MIN_MAG_MIP_POINT;
}

static std::vector<uint8_t> slice_pixels(const uint8_t *data, size_t size, uint32_t output_size, uint32_t input_size)
{
	std::vector<uint8_t> pixels;
	pixels.reserve((size / input_size) * output_size);

	while (size >= input_size)
	{
		pixels.insert(pixels.end(), data, data + output_size);
		size -= input_size;
		data += input_size;
	}

	return pixels;
}

template <typename T = uint8_t>
static std::vector<T> load_binary_file(const std::string &path)
{
	FILE *f = fopen(path.c_str(), "rb");
	if (!f)
	{
		LOGE("Failed to open binary file: %s\n", path.c_str());
		return {};
	}

	fseek(f, 0, SEEK_END);
	size_t len = ftell(f);
	rewind(f);

	std::vector<T> t;
	t.resize(len / sizeof(T));
	bool success = fread(t.data(), sizeof(T), len / sizeof(T), f) == len / sizeof(T);
	fclose(f);

	if (!success)
	{
		LOGE("Failed to read elements from %s.\n", path.c_str());
		return {};
	}

	return t;
}

struct PipelineState
{
	ComPtr<ID3D12PipelineState> pso;
	ComPtr<ID3D12RootSignature> root_signature;
};

struct Resource
{
	ComPtr<ID3D12Resource> gpu_resource;
	ComPtr<ID3D12Resource> staging_resource;
	ComPtr<ID3D12Resource> gpu_staging_resource;
	D3D12_RESOURCE_STATES current_state = D3D12_RESOURCE_STATE_COPY_DEST;
	D3D12_RESOURCE_STATES execution_state = D3D12_RESOURCE_STATE_COMMON;
	bool dirty = false;
	bool dirty_gpu_staging = true;
};

struct Device
{
	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12CommandQueue> queue;
	ComPtr<ID3D12Fence> fence;
	ComPtr<ID3D12GraphicsCommandList> list;

	struct
	{
		ComPtr<ID3D12CommandAllocator> allocator;
		ComPtr<ID3D12QueryHeap> timestamps;
		ComPtr<ID3D12Resource> timestamp_readback;
		uint64_t fence_value_for_iteration = 0;
		uint32_t pending_timestamps = 0;
	} frame_contexts[2] = {};

	uint32_t frame_index = 0;
	uint64_t latest_fence_value = 0;

	uint64_t total_ticks = 0;
	uint64_t total_dispatches = 0;

	Resource create_resource_from_desc(const std::string &base_path, const rapidjson::Value &value);

	void wait_idle();
	void teardown_swapchain();

	PipelineState create_compute_shader(const std::string &cs_path, const std::string &rs_path);
	PipelineState cs;

	struct NamedResource
	{
		std::string name;
		Resource resource;
	};
	std::vector<NamedResource> resources;

	bool load_resources(const std::string &base_path, const rapidjson::Value &value);

	ComPtr<ID3D12DescriptorHeap> resource_heap;
	ComPtr<ID3D12DescriptorHeap> sampler_heap;
	bool allocate_descriptor_heaps(const rapidjson::Value &doc);

	bool create_descriptors(const rapidjson::Value &doc);
	bool create_srv_descriptors(const rapidjson::Value &srvs);
	bool create_uav_descriptors(const rapidjson::Value &uavs);
	bool create_cbv_descriptors(const rapidjson::Value &cbvs);
	bool create_sampler_descriptors(const rapidjson::Value &samplers);

	Resource *find_resource(const char *name);

	bool execute_iteration(const rapidjson::Value &doc, uint32_t dispatches_per_list);
	void execute_sync_dirty();
	void execute_sync_dirty_gpu_staging();
	bool execute_dispatch(const rapidjson::Value &doc, uint32_t iteration);

#ifdef _WIN32
	ComPtr<IDXGIFactory2> factory;
#endif
	ComPtr<IDXGISwapChain3> swapchain;
	ComPtr<IDXGIVkSwapChain> vk_swapchain;
	ComPtr<ID3D12Resource> backbuffers[2];
	ComPtr<ID3D12DescriptorHeap> rtv;

	bool init_swapchain(SDL_Window *window);
};

PipelineState Device::create_compute_shader(const std::string &path, const std::string &rs_path)
{
	auto cs_data = load_binary_file<>(path);
	auto rs_data = load_binary_file<>(rs_path);
	if (cs_data.empty() || rs_data.empty())
		return {};

	PipelineState pipe;

	if (FAILED(device->CreateRootSignature(
			0, rs_data.data(), rs_data.size(),
			IID_ID3D12RootSignature, pipe.root_signature.ppv())))
	{
		LOGE("Failed to create root signature.\n");
		return {};
	}

	D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
	desc.pRootSignature = pipe.root_signature.get();
	desc.CS.pShaderBytecode = cs_data.data();
	desc.CS.BytecodeLength = cs_data.size();

	if (FAILED(device->CreateComputePipelineState(&desc, IID_ID3D12PipelineState, pipe.pso.ppv())))
	{
		LOGE("Failed to create PSO.\n");
		return {};
	}

	return pipe;
}

static DXGI_FORMAT convert_format_to_non_dsv_format(DXGI_FORMAT fmt)
{
	// FIXME: Also find some way to support stencil?

	switch (fmt)
	{
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_R24G8_TYPELESS:
		return DXGI_FORMAT_R32_FLOAT;

	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
		return DXGI_FORMAT_R8_UINT;

	case DXGI_FORMAT_D16_UNORM:
		return DXGI_FORMAT_R16_FLOAT;

	default:
		return fmt;
	}
}

Resource Device::create_resource_from_desc(const std::string &base_path, const rapidjson::Value &value)
{
	D3D12_HEAP_PROPERTIES heap_props = {};
	D3D12_RESOURCE_DESC desc = {};
	Resource res;

	heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;
	desc.Width = 1;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Flags = D3D12_RESOURCE_FLAG_NONE;
	desc.SampleDesc.Count = 1;
	desc.MipLevels = 1;

	if (value.HasMember("Dimension"))
		desc.Dimension = convert_resource_dimension(value["Dimension"].GetString());
	if (value.HasMember("Format"))
		desc.Format = convert_dxgi_format(value["Format"].GetString());
	if (value.HasMember("Width"))
		desc.Width = value["Width"].GetUint64();
	if (value.HasMember("Height"))
		desc.Height = value["Height"].GetUint();
	if (value.HasMember("DepthOrArraySize"))
		desc.DepthOrArraySize = value["DepthOrArraySize"].GetUint();
	if (value.HasMember("SampleCount"))
		desc.SampleDesc.Count = value["SampleCount"].GetUint();
	if (value.HasMember("MipLevels"))
		desc.MipLevels = value["MipLevels"].GetUint();
	if (value.HasMember("Flags"))
		desc.Flags = D3D12_RESOURCE_FLAGS(value["Flags"].GetUint());

	if (value.HasMember("FlagUAV"))
		desc.Flags |= value["FlagUAV"].GetUint() ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
	if (value.HasMember("FlagRTV"))
		desc.Flags |= value["FlagRTV"].GetUint() ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : D3D12_RESOURCE_FLAG_NONE;
	if (value.HasMember("FlagDSV"))
		desc.Flags |= value["FlagDSV"].GetUint() ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : D3D12_RESOURCE_FLAG_NONE;

	desc.Layout = desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER ?
	              D3D12_TEXTURE_LAYOUT_ROW_MAJOR : D3D12_TEXTURE_LAYOUT_UNKNOWN;

	if (desc.SampleDesc.Count > 1)
	{
		LOGW("MSAA not supported yet.\n");
		return {};
	}

	if (FAILED(device->CreateCommittedResource(
			&heap_props, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
			IID_ID3D12Resource, res.gpu_resource.ppv())))
	{
		LOGE("Failed to create resource.\n");
		return {};
	}

	heap_props.Type = D3D12_HEAP_TYPE_CUSTOM;
	heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
	heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	desc.Flags &= ~(D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS |
	                D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET |
	                D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

	desc.Format = convert_format_to_non_dsv_format(desc.Format);

	if (FAILED(device->CreateCommittedResource(
			&heap_props, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_COMMON, nullptr,
			IID_ID3D12Resource, res.staging_resource.ppv())))
	{
		LOGE("Failed to create resource.\n");
		return {};
	}

	heap_props = {};
	heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;
	if (FAILED(device->CreateCommittedResource(
		&heap_props, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
		IID_ID3D12Resource, res.gpu_staging_resource.ppv())))
	{
		LOGE("Failed to create resource.\n");
		return {};
	}

	if (value.HasMember("data"))
	{
		uint32_t subresources_to_map = 1;

		if (desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
		{
			subresources_to_map = desc.MipLevels;
			if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D)
				subresources_to_map *= desc.DepthOrArraySize;

			for (uint32_t i = 0; i < subresources_to_map; i++)
				if (FAILED(res.staging_resource->Map(i, nullptr, nullptr)))
					return {};
		}

		for (uint32_t i = 0; i < desc.MipLevels; i++)
		{
			auto path = relpath(base_path, value["data"][i].GetString());
			auto data = load_binary_file<>(path);
			size_t src_offset = 0;

			if (data.empty())
			{
				LOGE("Failed to load init buffer \"%s\".\n", path.c_str());
				return {};
			}

			if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
			{
				if (data.size() != desc.Width)
				{
					LOGE("Mismatch between desc Width and buffer. %zu != %zu\n",
						 data.size(), size_t(desc.Width));
					return {};
				}

				void *ptr = nullptr;
				if (FAILED(res.staging_resource->Map(0, nullptr, &ptr)))
				{
					LOGE("Failed to map staging resource.\n");
					return {};
				}

				memcpy(ptr, data.data(), data.size());
				res.staging_resource->Unmap(0, nullptr);
				res.dirty = true;
			}
			else
			{
				if (!value.HasMember("PixelSize"))
				{
					LOGE("Need to set PixelSize (at least for now).\n");
					return {};
				}

				uint32_t pixel_size = value["PixelSize"].GetUint();
				uint32_t block_width, block_height;

				get_block_dimensions(desc.Format, block_width, block_height);

				// Mostly used for depth stencil to extract only depth aspect.
				if (value.HasMember("PixelSlice"))
				{
					uint32_t slice_bytes = value["PixelSlice"].GetUint();
					data = slice_pixels(data.data(), data.size(), slice_bytes, pixel_size);
					pixel_size = slice_bytes;
				}

				// TODO: Depth-stencil planes change this too.
				D3D12_BOX dst_box = {};
				dst_box.right = std::max<uint32_t>(desc.Width >> i, 1u);
				dst_box.bottom = std::max<uint32_t>(desc.Height >> i, 1u);

				uint32_t num_blocks_x = (dst_box.right + block_width - 1) / block_width;
				uint32_t num_blocks_y = (dst_box.bottom + block_height - 1) / block_height;

				uint32_t src_row_pitch = num_blocks_x * pixel_size;
				uint32_t src_slice_pitch = num_blocks_x * num_blocks_y * pixel_size;

				if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
				{
					dst_box.back = std::max<uint32_t>(desc.DepthOrArraySize >> i, 1u);
					res.staging_resource->WriteToSubresource(
							i, &dst_box, data.data() + src_offset, src_row_pitch, src_slice_pitch);

					if (src_offset + src_slice_pitch * dst_box.back > data.size())
					{
						LOGE("Attempting to load texture out of bounds.\n");
						return {};
					}
				}
				else
				{
					if (src_offset + src_slice_pitch * desc.DepthOrArraySize > data.size())
					{
						LOGE("Attempting to load texture out of bounds.\n");
						return {};
					}

					for (uint32_t j = 0; j < desc.DepthOrArraySize; j++)
					{
						dst_box.back = 1;
						res.staging_resource->WriteToSubresource(
								i + j * desc.MipLevels, &dst_box, data.data() + src_offset,
								src_row_pitch, src_slice_pitch);
						src_offset += src_slice_pitch * dst_box.back;
					}
				}

				res.dirty = true;
			}
		}

		if (desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
			for (uint32_t i = 0; i < subresources_to_map; i++)
				res.staging_resource->Unmap(i, nullptr);
	}

	return res;
}

bool Device::load_resources(const std::string &base_path, const rapidjson::Value &value)
{
	for (auto itr = value.Begin(); itr != value.End(); ++itr)
	{
		auto &obj = *itr;

		if (!obj.HasMember("name"))
		{
			LOGE("Must specify name.\n");
			return false;
		}

		Resource res = create_resource_from_desc(base_path, obj);
		if (!res.gpu_resource)
			return false;

		resources.push_back({ obj["name"].GetString(), std::move(res) });
	}

	return true;
}

Resource *Device::find_resource(const char *name)
{
	auto itr = std::find_if(resources.begin(), resources.end(), [name](const NamedResource &res) {
		return res.name == name;
	});

	if (itr == resources.end())
	{
		LOGE("Could not find resource named \"%s\".\n", name);
		return nullptr;
	}

	return &itr->resource;
}

bool Device::create_cbv_descriptors(const rapidjson::Value &cbvs)
{
	auto desc_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (auto itr = cbvs.Begin(); itr != cbvs.End(); ++itr)
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {};
		auto &cbv = *itr;

		if (!cbv.HasMember("Resource"))
		{
			LOGE("Missing Resource\n");
			return false;
		}

		auto *resource = find_resource(cbv["Resource"].GetString());
		if (!resource)
			return false;

		if (resource->execution_state != D3D12_RESOURCE_STATE_COMMON &&
		    resource->execution_state != D3D12_RESOURCE_STATE_GENERIC_READ)
		{
			LOGE("Mismatch in resource state required.\n");
			return false;
		}

		resource->execution_state = D3D12_RESOURCE_STATE_GENERIC_READ;

		cbv_desc.BufferLocation = resource->gpu_resource->GetGPUVirtualAddress();

		if (cbv.HasMember("BufferLocation"))
			cbv_desc.BufferLocation += cbv["BufferLocation"].GetUint64();

		if (!cbv.HasMember("SizeInBytes"))
		{
			LOGE("Missing SizeInBytes.\n");
			return false;
		}

		cbv_desc.SizeInBytes = cbv["SizeInBytes"].GetUint();

		if (!cbv.HasMember("HeapOffset"))
		{
			LOGE("Missing HeapOffset\n");
			return false;
		}

		auto handle = resource_heap->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += cbv["HeapOffset"].GetUint64() * desc_size;
		device->CreateConstantBufferView(&cbv_desc, handle);
	}

	return true;
}

bool Device::create_srv_descriptors(const rapidjson::Value &srvs)
{
	auto desc_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (auto itr = srvs.Begin(); itr != srvs.End(); ++itr)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		auto &srv = *itr;

		if (!srv.HasMember("Resource"))
		{
			LOGE("Missing Resource\n");
			return false;
		}

		auto *resource = find_resource(srv["Resource"].GetString());
		if (!resource)
			return false;

		srv_desc.Format = resource->gpu_resource->GetDesc().Format;

		if (srv.HasMember("ViewDimension"))
			srv_desc.ViewDimension = convert_srv_dimension(srv["ViewDimension"].GetString());
		if (srv.HasMember("Format"))
			srv_desc.Format = convert_dxgi_format(srv["Format"].GetString());
		if (srv.HasMember("Shader4ComponentMapping"))
			srv_desc.Shader4ComponentMapping = srv["Shader4ComponentMapping"].GetUint();

		switch (srv_desc.ViewDimension)
		{
		case D3D12_SRV_DIMENSION_BUFFER:
			if (srv.HasMember("FirstElement"))
				srv_desc.Buffer.FirstElement = srv["FirstElement"].GetUint64();
			if (srv.HasMember("NumElements"))
				srv_desc.Buffer.NumElements = srv["NumElements"].GetUint();
			if (srv.HasMember("StructureByteStride"))
				srv_desc.Buffer.StructureByteStride = srv["StructureByteStride"].GetUint();
			if (srv.HasMember("Flags"))
				srv_desc.Buffer.Flags = convert_buffer_srv_flags(srv["Flags"].GetString());
			break;

		case D3D12_SRV_DIMENSION_TEXTURE1D:
			srv_desc.Texture1D.MipLevels = ~0u;
			if (srv.HasMember("MipLevels"))
				srv_desc.Texture1D.MipLevels = srv["MipLevels"].GetUint();
			if (srv.HasMember("MostDetailedMip"))
				srv_desc.Texture1D.MostDetailedMip = srv["MostDetailedMip"].GetUint();
			if (srv.HasMember("ResourceMinLODClamp"))
				srv_desc.Texture1D.ResourceMinLODClamp = srv["ResourceMinLODClamp"].GetFloat();
			break;

		case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
			srv_desc.Texture1DArray.MipLevels = ~0u;
			srv_desc.Texture1DArray.ArraySize = ~0u;
			if (srv.HasMember("MipLevels"))
				srv_desc.Texture1DArray.MipLevels = srv["MipLevels"].GetUint();
			if (srv.HasMember("MostDetailedMip"))
				srv_desc.Texture1DArray.MostDetailedMip = srv["MostDetailedMip"].GetUint();
			if (srv.HasMember("ResourceMinLODClamp"))
				srv_desc.Texture1DArray.ResourceMinLODClamp = srv["ResourceMinLODClamp"].GetFloat();
			if (srv.HasMember("ArraySize"))
				srv_desc.Texture1DArray.ArraySize = srv["ArraySize"].GetUint();
			if (srv.HasMember("FirstArraySlice"))
				srv_desc.Texture1DArray.FirstArraySlice = srv["FirstArraySlice"].GetUint();
			break;

		case D3D12_SRV_DIMENSION_TEXTURE2D:
			srv_desc.Texture2D.MipLevels = ~0u;
			if (srv.HasMember("MipLevels"))
				srv_desc.Texture2D.MipLevels = srv["MipLevels"].GetUint();
			if (srv.HasMember("MostDetailedMip"))
				srv_desc.Texture2D.MostDetailedMip = srv["MostDetailedMip"].GetUint();
			if (srv.HasMember("ResourceMinLODClamp"))
				srv_desc.Texture2D.ResourceMinLODClamp = srv["ResourceMinLODClamp"].GetFloat();
			if (srv.HasMember("PlaneSlice"))
				srv_desc.Texture2D.PlaneSlice = srv["PlaneSlice"].GetUint();
			break;

		case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
			srv_desc.Texture2DArray.MipLevels = ~0u;
			srv_desc.Texture2DArray.ArraySize = ~0u;
			if (srv.HasMember("MipLevels"))
				srv_desc.Texture2DArray.MipLevels = srv["MipLevels"].GetUint();
			if (srv.HasMember("MostDetailedMip"))
				srv_desc.Texture2DArray.MostDetailedMip = srv["MostDetailedMip"].GetUint();
			if (srv.HasMember("ResourceMinLODClamp"))
				srv_desc.Texture2DArray.ResourceMinLODClamp = srv["ResourceMinLODClamp"].GetFloat();
			if (srv.HasMember("ArraySize"))
				srv_desc.Texture2DArray.ArraySize = srv["ArraySize"].GetUint();
			if (srv.HasMember("FirstArraySlice"))
				srv_desc.Texture2DArray.FirstArraySlice = srv["FirstArraySlice"].GetUint();
			if (srv.HasMember("PlaneSlice"))
				srv_desc.Texture2DArray.PlaneSlice = srv["PlaneSlice"].GetUint();
			break;

		case D3D12_SRV_DIMENSION_TEXTURECUBE:
			srv_desc.TextureCube.MipLevels = ~0u;
			if (srv.HasMember("MipLevels"))
				srv_desc.TextureCube.MipLevels = srv["MipLevels"].GetUint();
			if (srv.HasMember("MostDetailedMip"))
				srv_desc.TextureCube.MostDetailedMip = srv["MostDetailedMip"].GetUint();
			if (srv.HasMember("ResourceMinLODClamp"))
				srv_desc.TextureCube.ResourceMinLODClamp = srv["ResourceMinLODClamp"].GetFloat();
			break;

		case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
			srv_desc.TextureCubeArray.MipLevels = ~0u;
			srv_desc.TextureCubeArray.NumCubes = ~0u;
			if (srv.HasMember("MipLevels"))
				srv_desc.TextureCubeArray.MipLevels = srv["MipLevels"].GetUint();
			if (srv.HasMember("MostDetailedMip"))
				srv_desc.TextureCubeArray.MostDetailedMip = srv["MostDetailedMip"].GetUint();
			if (srv.HasMember("ResourceMinLODClamp"))
				srv_desc.TextureCubeArray.ResourceMinLODClamp = srv["ResourceMinLODClamp"].GetFloat();
			if (srv.HasMember("NumCubes"))
				srv_desc.TextureCubeArray.NumCubes = srv["NumCubes"].GetUint();
			if (srv.HasMember("First2DArrayFace"))
				srv_desc.TextureCubeArray.First2DArrayFace = srv["First2DArrayFace"].GetUint();
			break;

		case D3D12_SRV_DIMENSION_TEXTURE3D:
			srv_desc.Texture3D.MipLevels = ~0u;
			if (srv.HasMember("MipLevels"))
				srv_desc.Texture3D.MipLevels = srv["MipLevels"].GetUint();
			if (srv.HasMember("MostDetailedMip"))
				srv_desc.Texture3D.MostDetailedMip = srv["MostDetailedMip"].GetUint();
			if (srv.HasMember("ResourceMinLODClamp"))
				srv_desc.Texture3D.ResourceMinLODClamp = srv["ResourceMinLODClamp"].GetFloat();
			break;

		case D3D12_SRV_DIMENSION_TEXTURE2DMS:
			break;

		case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
			srv_desc.Texture2DMSArray.ArraySize = ~0u;
			if (srv.HasMember("ArraySize"))
				srv_desc.Texture2DMSArray.ArraySize = srv["ArraySize"].GetUint();
			if (srv.HasMember("FirstArraySlice"))
				srv_desc.Texture2DMSArray.FirstArraySlice = srv["FirstArraySlice"].GetUint();
			break;

		default:
			LOGE("SRV dimension not set properly.\n");
			return false;
		}

		if (!srv.HasMember("HeapOffset"))
		{
			LOGE("Missing HeapOffset\n");
			return false;
		}

		auto handle = resource_heap->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += srv["HeapOffset"].GetUint64() * desc_size;

		if (resource->execution_state != D3D12_RESOURCE_STATE_COMMON &&
		    resource->execution_state != D3D12_RESOURCE_STATE_GENERIC_READ)
		{
			LOGE("Mismatch in resource state required.\n");
			return false;
		}

		resource->execution_state = D3D12_RESOURCE_STATE_GENERIC_READ;

		device->CreateShaderResourceView(resource->gpu_resource.get(), &srv_desc, handle);
	}

	return true;
}

bool Device::create_uav_descriptors(const rapidjson::Value &uavs)
{
	auto desc_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (auto itr = uavs.Begin(); itr != uavs.End(); ++itr)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};

		auto &uav = *itr;

		if (!uav.HasMember("Resource"))
		{
			LOGE("Missing Resource\n");
			return false;
		}

		Resource *resource = find_resource(uav["Resource"].GetString());
		Resource *counter_resource = nullptr;
		if (!resource)
			return false;

		uav_desc.Format = resource->gpu_resource->GetDesc().Format;

		if (uav.HasMember("ViewDimension"))
			uav_desc.ViewDimension = convert_uav_dimension(uav["ViewDimension"].GetString());
		if (uav.HasMember("Format"))
			uav_desc.Format = convert_dxgi_format(uav["Format"].GetString());

		switch (uav_desc.ViewDimension)
		{
		case D3D12_UAV_DIMENSION_BUFFER:
			if (uav.HasMember("FirstElement"))
				uav_desc.Buffer.FirstElement = uav["FirstElement"].GetUint64();
			if (uav.HasMember("NumElements"))
				uav_desc.Buffer.NumElements = uav["NumElements"].GetUint();
			if (uav.HasMember("StructureByteStride"))
				uav_desc.Buffer.StructureByteStride = uav["StructureByteStride"].GetUint();
			if (uav.HasMember("Flags"))
				uav_desc.Buffer.Flags = convert_buffer_uav_flags(uav["Flags"].GetString());
			if (uav.HasMember("CounterOffsetInBytes"))
				uav_desc.Buffer.CounterOffsetInBytes = uav["CounterOffsetInBytes"].GetUint64();
			break;

		case D3D12_UAV_DIMENSION_TEXTURE1D:
			if (uav.HasMember("MipSlice"))
				uav_desc.Texture1D.MipSlice = uav["MipSlice"].GetUint();
			break;

		case D3D12_UAV_DIMENSION_TEXTURE1DARRAY:
			uav_desc.Texture1DArray.ArraySize = ~0u;
			if (uav.HasMember("MipSlice"))
				uav_desc.Texture1DArray.MipSlice = uav["MipSlice"].GetUint();
			if (uav.HasMember("ArraySize"))
				uav_desc.Texture1DArray.ArraySize = uav["ArraySize"].GetUint();
			if (uav.HasMember("FirstArraySlice"))
				uav_desc.Texture1DArray.FirstArraySlice = uav["FirstArraySlice"].GetUint();
			break;

		case D3D12_UAV_DIMENSION_TEXTURE2D:
			if (uav.HasMember("MipSlice"))
				uav_desc.Texture2D.MipSlice = uav["MipSlice"].GetUint();
			if (uav.HasMember("PlaneSlice"))
				uav_desc.Texture2D.PlaneSlice = uav["PlaneSlice"].GetUint();
			break;

		case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
			uav_desc.Texture2DArray.ArraySize = ~0u;
			if (uav.HasMember("MipSlice"))
				uav_desc.Texture2DArray.MipSlice = uav["MipSlice"].GetUint();
			if (uav.HasMember("ArraySize"))
				uav_desc.Texture2DArray.ArraySize = uav["ArraySize"].GetUint();
			if (uav.HasMember("FirstArraySlice"))
				uav_desc.Texture2DArray.FirstArraySlice = uav["FirstArraySlice"].GetUint();
			if (uav.HasMember("PlaneSlice"))
				uav_desc.Texture2DArray.PlaneSlice = uav["PlaneSlice"].GetUint();
			break;

		case D3D12_UAV_DIMENSION_TEXTURE3D:
			uav_desc.Texture3D.WSize = ~0u;
			if (uav.HasMember("MipSlice"))
				uav_desc.Texture3D.MipSlice = uav["MipSlice"].GetUint();
			if (uav.HasMember("WSize"))
				uav_desc.Texture3D.WSize = uav["WSize"].GetUint();
			if (uav.HasMember("FirstWSlice"))
				uav_desc.Texture3D.FirstWSlice = uav["FirstWSlice"].GetUint();
			break;

		case D3D12_UAV_DIMENSION_TEXTURE2DMS:
			break;

		case D3D12_UAV_DIMENSION_TEXTURE2DMSARRAY:
			uav_desc.Texture2DMSArray.ArraySize = ~0u;
			if (uav.HasMember("ArraySize"))
				uav_desc.Texture2DMSArray.ArraySize = uav["ArraySize"].GetUint();
			if (uav.HasMember("FirstArraySlice"))
				uav_desc.Texture2DMSArray.FirstArraySlice = uav["FirstArraySlice"].GetUint();
			break;

		default:
			LOGE("UAV dimension not set properly.\n");
			return false;
		}

		if (!uav.HasMember("HeapOffset"))
		{
			LOGE("Missing HeapOffset\n");
			return false;
		}


		auto handle = resource_heap->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += uav["HeapOffset"].GetUint64() * desc_size;

		if (uav.HasMember("CounterResource"))
		{
			counter_resource = find_resource(uav["CounterResource"].GetString());
			if (!counter_resource)
				return false;
		}

		if (resource->execution_state != D3D12_RESOURCE_STATE_COMMON &&
		    resource->execution_state != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		{
			LOGE("Mismatch in resource state required.\n");
			return false;
		}

		resource->execution_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

		if (counter_resource)
		{
			if (counter_resource->execution_state != D3D12_RESOURCE_STATE_COMMON &&
			    counter_resource->execution_state != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				LOGE("Mismatch in resource state required.\n");
				return false;
			}

			counter_resource->execution_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}

		device->CreateUnorderedAccessView(
				resource->gpu_resource.get(),
				counter_resource ? counter_resource->gpu_resource.get() : nullptr,
				&uav_desc, handle);
	}

	return true;
}

bool Device::create_sampler_descriptors(const rapidjson::Value &samplers)
{
	auto desc_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

	for (auto itr = samplers.Begin(); itr != samplers.End(); ++itr)
	{
		D3D12_SAMPLER_DESC sampler_desc = {};

		auto &sampler = *itr;

		if (sampler.HasMember("BorderColor"))
			for (int i = 0; i < 4; i++)
				sampler_desc.BorderColor[i] = sampler["BorderColor"][i].GetFloat();

		sampler_desc.Filter = convert_filter(sampler["Filter"].GetString());

		if (sampler.HasMember("ComparisonFunc"))
			sampler_desc.ComparisonFunc = convert_comparison_func(sampler["ComparisonFunc"].GetString());

		if (sampler.HasMember("AddressU"))
			sampler_desc.AddressU = convert_texture_address_mode(sampler["AddressU"].GetString());
		if (sampler.HasMember("AddressV"))
			sampler_desc.AddressV = convert_texture_address_mode(sampler["AddressV"].GetString());
		if (sampler.HasMember("AddressW"))
			sampler_desc.AddressW = convert_texture_address_mode(sampler["AddressW"].GetString());
		if (sampler.HasMember("MaxAnisotropy"))
			sampler_desc.MaxAnisotropy = sampler["MaxAnisotropy"].GetUint();
		if (sampler.HasMember("MinLOD"))
			sampler_desc.MinLOD = sampler["MinLOD"].GetFloat();
		if (sampler.HasMember("MaxLOD"))
			sampler_desc.MaxLOD = sampler["MaxLOD"].GetFloat();
		if (sampler.HasMember("MipLODBias"))
			sampler_desc.MipLODBias = sampler["MipLODBias"].GetFloat();

		if (!sampler.HasMember("HeapOffset"))
		{
			LOGE("Missing HeapOffset\n");
			return false;
		}

		auto handle = resource_heap->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += sampler["HeapOffset"].GetUint64() * desc_size;

		device->CreateSampler(&sampler_desc, handle);
	}

	return true;
}

bool Device::create_descriptors(const rapidjson::Value &doc)
{
	if (doc.HasMember("SRV") && !create_srv_descriptors(doc["SRV"]))
		return false;
	if (doc.HasMember("UAV") && !create_uav_descriptors(doc["UAV"]))
		return false;
	if (doc.HasMember("CBV") && !create_cbv_descriptors(doc["CBV"]))
		return false;
	if (doc.HasMember("Sampler") && !create_sampler_descriptors(doc["Sampler"]))
		return false;

	return true;
}

bool Device::allocate_descriptor_heaps(const rapidjson::Value &doc)
{
	uint32_t num_resources = 1;
	uint32_t num_samplers = 1;

	static const char *resource_types[] = { "SRV", "CBV", "UAV", "Sampler" };

	for (auto *type : resource_types)
	{
		if (doc.HasMember(type))
		{
			auto &resource_list = doc[type];
			for (auto itr = resource_list.Begin(); itr != resource_list.End(); ++itr)
			{
				auto &resource = *itr;
				if (!resource.HasMember("HeapOffset"))
				{
					LOGE("Need HeapOffset\n");
					return false;
				}

				if (strcmp(type, "Sampler") == 0)
					num_samplers = std::max<uint32_t>(num_samplers, resource["HeapOffset"].GetUint() + 1);
				else
					num_resources = std::max<uint32_t>(num_resources, resource["HeapOffset"].GetUint() + 1);
			}
		}
	}

	D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
	heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heap_desc.NumDescriptors = num_resources;

	if (FAILED(device->CreateDescriptorHeap(&heap_desc, IID_ID3D12DescriptorHeap, resource_heap.ppv())))
		return false;

	heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	heap_desc.NumDescriptors = num_samplers;

	if (FAILED(device->CreateDescriptorHeap(&heap_desc, IID_ID3D12DescriptorHeap, sampler_heap.ppv())))
		return false;

	return true;
}

void Device::wait_idle()
{
	queue->Signal(fence.get(), ++latest_fence_value);
	fence->SetEventOnCompletion(latest_fence_value, nullptr);
}

void Device::teardown_swapchain()
{
	for (auto &img : backbuffers)
		img = {};
	swapchain = {};
	vk_swapchain = {};
}

void Device::execute_sync_dirty_gpu_staging()
{
	std::vector<D3D12_RESOURCE_BARRIER> barriers;

	for (auto &resource : resources)
	{
		if (!resource.resource.dirty_gpu_staging)
			continue;

		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = resource.resource.gpu_staging_resource.get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
		barrier.Transition.Subresource = UINT32_MAX;
		barriers.push_back(barrier);

		list->CopyResource(
			resource.resource.gpu_staging_resource.get(),
			resource.resource.staging_resource.get());

		resource.resource.dirty_gpu_staging = false;
	}

	if (!barriers.empty())
		list->ResourceBarrier(barriers.size(), barriers.data());
}

void Device::execute_sync_dirty()
{
	std::vector<D3D12_RESOURCE_BARRIER> barriers;

	for (auto &resource : resources)
	{
		if (!resource.resource.dirty)
			continue;

		if (resource.resource.current_state != D3D12_RESOURCE_STATE_COPY_DEST)
		{
			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Transition.pResource = resource.resource.gpu_resource.get();
			barrier.Transition.StateBefore = resource.resource.current_state;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
			barrier.Transition.Subresource = UINT32_MAX;
			resource.resource.current_state = D3D12_RESOURCE_STATE_COPY_DEST;
			barriers.push_back(barrier);
		}
	}

	if (!barriers.empty())
		list->ResourceBarrier(barriers.size(), barriers.data());

	barriers.clear();

	for (auto &resource : resources)
	{
		if (!resource.resource.dirty)
			continue;

		if (resource.resource.current_state != resource.resource.execution_state)
		{
			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Transition.pResource = resource.resource.gpu_resource.get();
			barrier.Transition.StateBefore = resource.resource.current_state;
			barrier.Transition.StateAfter = resource.resource.execution_state;
			barrier.Transition.Subresource = UINT32_MAX;
			resource.resource.current_state = resource.resource.execution_state;
			barriers.push_back(barrier);
		}

		if (resource.resource.gpu_resource->GetDesc().Format == resource.resource.gpu_staging_resource->GetDesc().Format)
		{
			list->CopyResource(
					resource.resource.gpu_resource.get(),
					resource.resource.gpu_staging_resource.get());
		}
		else
		{
			// Handle special color <-> depth copies.
			auto desc = resource.resource.gpu_resource->GetDesc();
			uint32_t subresources = desc.MipLevels;
			if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D)
				subresources *= desc.DepthOrArraySize;
			D3D12_TEXTURE_COPY_LOCATION dst, src;
			dst.pResource = resource.resource.gpu_resource.get();
			src.pResource = resource.resource.gpu_staging_resource.get();
			dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

			for (uint32_t i = 0; i < subresources; i++)
			{
				dst.SubresourceIndex = i;
				src.SubresourceIndex = i;
				list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
			}
		}

		resource.resource.dirty = false;
	}

	if (!barriers.empty())
		list->ResourceBarrier(barriers.size(), barriers.data());
}

bool Device::execute_dispatch(const rapidjson::Value &doc, uint32_t iteration)
{
	auto &ctx = frame_contexts[frame_index];

	if (!doc.HasMember("Dispatch"))
	{
		LOGE("Missing dispatch field.\n");
		return false;
	}

	if (!doc.HasMember("RootParameters"))
	{
		LOGE("Missing RootParameters field.\n");
		return false;
	}

	list->SetComputeRootSignature(cs.root_signature.get());
	list->SetPipelineState(cs.pso.get());

	auto &params = doc["RootParameters"];
	for (auto itr = params.Begin(); itr != params.End(); ++itr)
	{
		auto &param = *itr;
		if (!param.HasMember("type") || !param.HasMember("index") || !param.HasMember("offset"))
		{
			LOGE("Missing type, index, offset fields.\n");
			return false;
		}

		const char *type = param["type"].GetString();
		uint64_t offset = param["offset"].GetUint64();
		uint32_t index = param["index"].GetUint();

		if (strcmp(type, "ResourceTable") == 0)
		{
			D3D12_GPU_DESCRIPTOR_HANDLE desc = resource_heap->GetGPUDescriptorHandleForHeapStart();
			desc.ptr += offset * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			list->SetComputeRootDescriptorTable(index, desc);
		}
		else if (strcmp(type, "SamplerTable") == 0)
		{
			D3D12_GPU_DESCRIPTOR_HANDLE desc = sampler_heap->GetGPUDescriptorHandleForHeapStart();
			desc.ptr += offset * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
			list->SetComputeRootDescriptorTable(index, desc);
		}
		else
		{
			if (!param.HasMember("Resource"))
			{
				LOGE("Missing Resource for root parameter.\n");
				return false;
			}

			auto *resource = find_resource(param["Resource"].GetString());
			if (!resource)
				return false;

			D3D12_GPU_VIRTUAL_ADDRESS va = resource->gpu_resource->GetGPUVirtualAddress();
			if (!va)
				return false;

			va += param["offset"].GetUint64();

			if (strcmp(type, "SRV") == 0)
				list->SetComputeRootShaderResourceView(index, va);
			else if (strcmp(type, "UAV") == 0)
				list->SetComputeRootUnorderedAccessView(index, va);
			else if (strcmp(type, "CBV") == 0)
				list->SetComputeRootConstantBufferView(index, va);
			else
			{
				LOGE("Invalid root parameter type \"%s\"\n", type);
				return false;
			}
		}
	}

	list->EndQuery(ctx.timestamps.get(), D3D12_QUERY_TYPE_TIMESTAMP, 2 * iteration + 0);
	list->Dispatch(doc["Dispatch"][0].GetUint(), doc["Dispatch"][1].GetUint(), doc["Dispatch"][2].GetUint());
	list->EndQuery(ctx.timestamps.get(), D3D12_QUERY_TYPE_TIMESTAMP, 2 * iteration + 1);

	// UAVs can be modified, so refresh them every iteration.
	for (auto &resource : resources)
		if (resource.resource.execution_state == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			resource.resource.dirty = true;

	D3D12_RESOURCE_BARRIER uav_barrier = {};
	uav_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	list->ResourceBarrier(1, &uav_barrier);

	return true;
}

bool Device::execute_iteration(const rapidjson::Value &doc, uint32_t dispatches_per_list)
{
	auto &ctx = frame_contexts[frame_index];
	if (ctx.fence_value_for_iteration != 0)
	{
		if (FAILED(fence->SetEventOnCompletion(ctx.fence_value_for_iteration, nullptr)))
			return false;

		// No need for the CPU copy now.
		for (auto &resource : resources)
			resource.resource.staging_resource = {};
	}

	if (ctx.pending_timestamps)
	{
		const uint64_t *tses = nullptr;
		if (SUCCEEDED(ctx.timestamp_readback->Map(0, nullptr, (void **)&tses)))
		{
			for (uint32_t i = 0; i < dispatches_per_list; i++)
			{
				total_ticks += tses[2 * i + 1] - tses[2 * i + 0];
				total_dispatches++;
			}
			ctx.timestamp_readback->Unmap(0, nullptr);
		}
	}

	ctx.pending_timestamps = dispatches_per_list;

	if (!ctx.timestamps || ctx.timestamp_readback->GetDesc().Width < dispatches_per_list * sizeof(uint64_t) * 2)
	{
		D3D12_QUERY_HEAP_DESC query_heap = {};
		query_heap.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		query_heap.Count = dispatches_per_list * 2;
		if (FAILED(device->CreateQueryHeap(&query_heap, IID_ID3D12QueryHeap, ctx.timestamps.ppv())))
			return false;

		D3D12_HEAP_PROPERTIES heap_props = {};
		heap_props.Type = D3D12_HEAP_TYPE_READBACK;

		D3D12_RESOURCE_DESC res = {};
		res.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		res.Width = dispatches_per_list * sizeof(uint64_t) * 2;
		res.Height = 1;
		res.DepthOrArraySize = 1;
		res.MipLevels = 1;
		res.SampleDesc.Count = 1;
		res.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		if (FAILED(device->CreateCommittedResource(
				&heap_props, D3D12_HEAP_FLAG_NONE,
				&res, D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr, IID_ID3D12Resource, ctx.timestamp_readback.ppv())))
		{
			return false;
		}
	}

	if (FAILED(list->Reset(ctx.allocator.get(), nullptr)))
		return false;

	// Split submissions to allow better preemption while grinding the GPU.
	ID3D12DescriptorHeap *heaps[] = { resource_heap.get(), sampler_heap.get() };
	list->SetDescriptorHeaps(2, heaps);

	for (uint32_t i = 0; i < dispatches_per_list; i++)
	{
		execute_sync_dirty_gpu_staging();
		execute_sync_dirty();
		if (!execute_dispatch(doc, i))
			return false;
	}

	list->ResolveQueryData(ctx.timestamps.get(), D3D12_QUERY_TYPE_TIMESTAMP,
	                       0, dispatches_per_list * 2,
	                       ctx.timestamp_readback.get(), 0);

	if (rtv)
	{
		// Just clear something so it looks like it's working.
		D3D12_CPU_DESCRIPTOR_HANDLE h = rtv->GetCPUDescriptorHandleForHeapStart();
		size_t index = 0;

		if (vk_swapchain)
			index = vk_swapchain->GetImageIndex();
		else if (swapchain)
			index = swapchain->GetCurrentBackBufferIndex();

		h.ptr += index * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = backbuffers[index].get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

		const FLOAT color[] = { 0.1f, 0.2f, 0.3f };

		list->ResourceBarrier(1, &barrier);
		list->ClearRenderTargetView(h, color, 0, nullptr);

		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		list->ResourceBarrier(1, &barrier);
	}

	if (FAILED(list->Close()))
		return false;

	ID3D12CommandList *lists[] = { list.get() };
	queue->ExecuteCommandLists(1, lists);

	if (vk_swapchain)
	{
		if (FAILED(vk_swapchain->Present(0, 0, nullptr)))
			return false;
	}
	else if (swapchain)
	{
		if (FAILED(swapchain->Present(0, 0)))
			return false;
	}

	queue->Signal(fence.get(), ++latest_fence_value);
	ctx.fence_value_for_iteration = latest_fence_value;
	return true;
}

static Device create_device(const std::string &path, bool validate)
{
	void *module = dlopen(path.c_str(), RTLD_NOW);
	if (!module)
	{
		LOGE("Failed to load vkd3d-proton.\n");
		return {};
	}

	if (validate)
	{
		auto get_debug_iface = (PFN_D3D12_GET_DEBUG_INTERFACE) dlsym(module, "D3D12GetDebugInterface");
		ComPtr<ID3D12Debug> debug;
		if (SUCCEEDED(get_debug_iface(IID_ID3D12Debug, debug.ppv())))
		{
			LOGI("Enabling validation.\n");
			debug->EnableDebugLayer();
		}
	}

	auto create = (PFN_D3D12_CREATE_DEVICE)dlsym(module, "D3D12CreateDevice");
	if (!create)
	{
		LOGE("Failed to query symbol.\n");
		return {};
	}

	Device device;
	if (FAILED(create(nullptr, D3D_FEATURE_LEVEL_12_0, IID_ID3D12Device, device.device.ppv())))
	{
		fprintf(stderr, "Failed to create device.\n");
		return {};
	}

	auto *dev = device.device.get();

	if (FAILED(dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_ID3D12Fence, device.fence.ppv())))
		return {};

	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	if (FAILED(dev->CreateCommandQueue(&desc, IID_ID3D12CommandQueue, device.queue.ppv())))
		return {};

	for (auto &ctx : device.frame_contexts)
		if (FAILED(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_ID3D12CommandAllocator, ctx.allocator.ppv())))
			return {};

	if (FAILED(dev->CreateCommandList(
			0, D3D12_COMMAND_LIST_TYPE_DIRECT, device.frame_contexts[0].allocator.get(),
			nullptr, IID_ID3D12CommandList, device.list.ppv())))
	{
		return {};
	}

	device.list->Close();

	return device;
}

bool Device::init_swapchain(SDL_Window *window)
{
	struct SurfaceFactory : IDXGIVkSurfaceFactory
	{
		// Unused
		HRESULT QueryInterface(const IID &, void **) override { return E_NOINTERFACE; }
		ULONG AddRef() override { return 0; }
		ULONG Release() override { return 0; }

		VkResult CreateSurface(VkInstance instance, VkPhysicalDevice, VkSurfaceKHR *pSurface) override
		{
#ifdef _WIN32
			void *module = dlopen("vulkan-1.dll", RTLD_NOW);
#else
			void *module = dlopen("libvulkan.so.1", RTLD_NOW);
#endif

			if (!module)
				return VK_ERROR_INCOMPATIBLE_DRIVER;

			auto gipa = (PFN_vkGetInstanceProcAddr)dlsym(module, "vkGetInstanceProcAddr");
			if (!gipa)
				return VK_ERROR_INCOMPATIBLE_DRIVER;

			SDL_PropertiesID props = SDL_GetWindowProperties(window);
			SDL_LockProperties(props);

			VkResult vr = VK_ERROR_INCOMPATIBLE_DRIVER;

#ifdef _WIN32
			auto win32_create = (PFN_vkCreateWin32SurfaceKHR)gipa(instance, "vkCreateWin32SurfaceKHR");
			auto hwnd = static_cast<HWND>(SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
			auto hinstance = static_cast<HINSTANCE>(SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, nullptr));

			if (hwnd && win32_create)
			{
				VkWin32SurfaceCreateInfoKHR win32_info = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
				win32_info.hwnd = hwnd;
				win32_info.hinstance = hinstance;
				vr = win32_create(instance, &win32_info, nullptr, pSurface);
			}
#else
			auto x11_win = static_cast<Window>(SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
			auto x11_dpy = static_cast<Display *>(SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr));
			auto wl_dpy = static_cast<wl_display *>(SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr));
			auto wl_surf = static_cast<wl_surface *>(SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr));

			auto x11_create = (PFN_vkCreateXlibSurfaceKHR)gipa(instance, "vkCreateXlibSurfaceKHR");
			auto wl_create = (PFN_vkCreateWaylandSurfaceKHR)gipa(instance, "vkCreateWaylandSurfaceKHR");

			if (x11_create && x11_dpy)
			{
				VkXlibSurfaceCreateInfoKHR xlib_info = { VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR };
				xlib_info.dpy = x11_dpy;
				xlib_info.window = x11_win;
				vr = x11_create(instance, &xlib_info, nullptr, pSurface);
			}
			else if (wl_create && wl_dpy && wl_surf)
			{
				VkWaylandSurfaceCreateInfoKHR wl_info = { VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR };
				wl_info.display = wl_dpy;
				wl_info.surface = wl_surf;
				vr = wl_create(instance, &wl_info, nullptr, pSurface);
			}
#endif

			SDL_UnlockProperties(props);
			return vr;
		}

		ID3D12Device *device = nullptr;
		SDL_Window *window = nullptr;
	} surface_factory;

	surface_factory.device = device.get();
	surface_factory.window = window;

	DXGI_SWAP_CHAIN_DESC1 desc = {};
	desc.Width = 512;
	desc.Height = 512;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.BufferCount = 2;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.SampleDesc.Count = 1;
	desc.BufferCount = 2;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.SampleDesc.Count = 1;
	desc.Scaling = DXGI_SCALING_STRETCH;
	desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

	ComPtr<IDXGIVkSwapChainFactory> swapchain_factory;
	if (SUCCEEDED(queue->QueryInterface(IID_IDXGIVkSwapChainFactory, swapchain_factory.ppv())))
	{
		if (FAILED(swapchain_factory->CreateSwapChain(&surface_factory, &desc, reinterpret_cast<IDXGIVkSwapChain **>(vk_swapchain.ppv()))))
			return false;
		for (int i = 0; i < 2; i++)
			if (FAILED(vk_swapchain->GetImage(i, IID_ID3D12Resource, backbuffers[i].ppv())))
				return false;
	}

	if (vk_swapchain)
		return true;

#ifdef _WIN32
	SDL_PropertiesID props = SDL_GetWindowProperties(window);
	SDL_LockProperties(props);
	HWND hwnd = static_cast<HWND>(SDL_GetPointerProperty(props, "SDL.window.win32.hwnd", nullptr));
	SDL_UnlockProperties(props);

	void *dxgi = dlopen("dxgi.dll", RTLD_NOW);
	if (!dxgi)
		return false;

	auto create_factory = (decltype(&CreateDXGIFactory1))dlsym(dxgi, "CreateDXGIFactory1");
	if (!create_factory)
		return false;

	if (FAILED(create_factory(IID_IDXGIFactory2, factory.ppv())))
		return false;

	if (FAILED(factory->CreateSwapChainForHwnd(queue.get(), hwnd, &desc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1 **>(swapchain.ppv()))))
		return false;

	for (int i = 0; i < 2; i++)
		if (FAILED(swapchain->GetBuffer(i, IID_ID3D12Resource, backbuffers[i].ppv())))
			return false;
#endif

	return true;
}

static void print_help()
{
	LOGI("d3d12-replayer [--d3d12 <path>] [--json <path to JSON>] [--validate] [--iterations <count>] [--dispatches <count>]\n");
}

int main(int argc, char **argv)
{
	unsigned dispatches_per_iteration = 1;
	std::string d3d12, json;
	bool validate = false;
	unsigned iterations = 0;
	Util::CLICallbacks cbs;

	if (!SDL_Init(SDL_INIT_VIDEO))
		return EXIT_FAILURE;

	cbs.add("--d3d12", [&](Util::CLIParser &parser) { d3d12 = parser.next_string(); });
	cbs.add("--json", [&](Util::CLIParser &parser) { json = parser.next_string(); });
	cbs.add("--validate", [&](Util::CLIParser &) { validate = true; });
	cbs.add("--iterations", [&](Util::CLIParser &parser) { iterations = parser.next_uint(); });
	cbs.add("--dispatches", [&](Util::CLIParser &parser) { dispatches_per_iteration = parser.next_uint(); });
	cbs.add("--help", [&](Util::CLIParser &parser) { parser.end(); });

	Util::CLIParser parser(std::move(cbs), argc - 1, argv + 1);
	if (!parser.parse())
	{
		LOGE("Failed to parse.\n");
		print_help();
		return EXIT_FAILURE;
	}

	if (d3d12.empty())
		d3d12 = "d3d12.dll";

	auto device = create_device(d3d12, validate);
	if (!device.device)
	{
		LOGE("Failed to create device.\n");
		return EXIT_FAILURE;
	}

	SDL_Window *window = nullptr;
	if (iterations == 0)
		window = SDL_CreateWindow("d3d12-replayer", 512, 512, 0);

	if (window)
	{
		if (!device.init_swapchain(window))
			return EXIT_FAILURE;

		D3D12_DESCRIPTOR_HEAP_DESC rtv_desc = {};
		rtv_desc.NumDescriptors = 2;
		rtv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		if (FAILED(device.device->CreateDescriptorHeap(&rtv_desc, IID_ID3D12DescriptorHeap, device.rtv.ppv())))
			return EXIT_FAILURE;

		for (int i = 0; i < 2; i++)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE h = device.rtv->GetCPUDescriptorHandleForHeapStart();
			h.ptr += size_t(i) * device.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			device.device->CreateRenderTargetView(device.backbuffers[i].get(), nullptr, h);
		}
	}

	rapidjson::Document doc;
	auto json_data = load_binary_file<char>(json);
	if (json_data.empty())
		return EXIT_FAILURE;

	doc.Parse(json_data.data(), json_data.size());
	if (doc.HasParseError())
	{
		LOGE("Parse error: %d\n", doc.GetParseError());
		return EXIT_FAILURE;
	}

	if (!doc.HasMember("CS") || !doc.HasMember("RootSignature"))
	{
		LOGE("Must define \"CS\" and \"RootSignature\".\n");
		return EXIT_FAILURE;
	}

	device.cs = device.create_compute_shader(
			relpath(json, doc["CS"].GetString()),
			relpath(json, doc["RootSignature"].GetString()));

	if (!device.cs.pso)
	{
		LOGE("Failed to create CS.\n");
		return EXIT_FAILURE;
	}

	if (!doc.HasMember("Resources"))
	{
		LOGE("Must specify resources.\n");
		return EXIT_FAILURE;
	}

	if (!device.load_resources(json, doc["Resources"]))
		return EXIT_FAILURE;

	if (!device.allocate_descriptor_heaps(doc))
	{
		LOGE("Failed to allocate descriptor heaps.\n");
		return EXIT_FAILURE;
	}

	if (!device.create_descriptors(doc))
	{
		LOGE("Failed to create descriptors.\n");
		return EXIT_FAILURE;
	}

	if (window)
	{
		bool alive = true;
		while (alive)
		{
			SDL_Event e;
			while (SDL_PollEvent(&e))
				if (e.type == SDL_EVENT_QUIT)
					alive = false;

			if (!device.execute_iteration(doc, dispatches_per_iteration))
			{
				LOGE("Failed to execute iteration.\n");
				return EXIT_FAILURE;
			}
		}
	}
	else
	{
		for (uint32_t iter = 0; iter < iterations; iter++)
		{
			if (!device.execute_iteration(doc, dispatches_per_iteration))
			{
				LOGE("Failed to execute iteration.\n");
				return EXIT_FAILURE;
			}
		}
	}

	UINT64 freq = 0;
	if (SUCCEEDED(device.queue->GetTimestampFrequency(&freq)))
	{
		LOGI("Total ticks: %llu, total timestamps: %llu\n",
		     static_cast<unsigned long long>(device.total_ticks),
		     static_cast<unsigned long long>(device.total_dispatches));
		LOGI("Total time per dispatch: %.3f us\n",
		     1e6 * double(device.total_ticks) / (double(device.total_dispatches) * double(freq)));
	}

	device.wait_idle();
	device.teardown_swapchain();
	SDL_DestroyWindow(window);
	SDL_Quit();
}
