#pragma once
#include "RendererSettings.h"
//#include "SceneViewport.h"
#include "ConstantBuffers.h"
#include "Postprocessor.h"
#include "GBufferPass.h"
#include "AmbientPass.h"
#include "SkyPass.h"
#include "LightingPass.h"
#include "TiledLightingPass.h"
#include "ShadowPass.h"
#include "ToneMapPass.h"
#include "FXAAPass.h"
#include "CopyToTexturePass.h"
#include "../Graphics/ShaderUtility.h"
#include "../Graphics/ConstantBuffer.h"
#include "../Graphics/TextureManager.h"
#include "../Graphics/GPUProfiler.h"
#include "../Utilities/CPUProfiler.h" 
#include "../RenderGraph/RenderGraphResourcePool.h"



namespace adria
{
	namespace tecs
	{
		class registry;
	}
	class Camera;
	class Buffer;
	class Texture;
	struct Light;

	class RenderGraphRenderer
	{
		enum ENullHeapSlot
		{
			NULL_HEAP_SLOT_TEXTURE2D,
			NULL_HEAP_SLOT_TEXTURECUBE,
			NULL_HEAP_SLOT_TEXTURE2DARRAY,
			NULL_HEAP_SLOT_RWTEXTURE2D,
			NULL_HEAP_SIZE
		};
		static constexpr uint32 SSAO_KERNEL_SIZE = 16;

	public:

		RenderGraphRenderer(tecs::registry& reg, GraphicsDevice* gfx, uint32 width, uint32 height);
		~RenderGraphRenderer();

		void NewFrame(Camera const* camera);
		void Update(float32 dt);
		void Render(RendererSettings const&);

		void SetProfilerSettings(ProfilerSettings const&);
		void OnResize(uint32 w, uint32 h);
		void OnSceneInitialized();

		Texture const* GetFinalTexture() const { return final_texture.get(); }
		TextureManager& GetTextureManager();
		std::vector<std::string> GetProfilerResults(bool log) const
		{
			return gpu_profiler.GetProfilerResults(gfx->GetDefaultCommandList(), log);
		}
	private:
		tecs::registry& reg;
		GraphicsDevice* gfx;
		RGResourcePool resource_pool;

		CPUProfiler cpu_profiler;
		GPUProfiler gpu_profiler;

		RendererSettings render_settings;
		ProfilerSettings profiler_settings = NO_PROFILING;

		TextureManager texture_manager;
		Camera const* camera;

		uint32 const backbuffer_count;
		uint32 backbuffer_index;
		uint32 width;
		uint32 height;

		//resources
		std::unique_ptr<Texture> final_texture;

		//Persistent constant buffers
		ConstantBuffer<FrameCBuffer> frame_cbuffer;
		ConstantBuffer<PostprocessCBuffer> postprocess_cbuffer;
		ConstantBuffer<ComputeCBuffer> compute_cbuffer;
		ConstantBuffer<WeatherCBuffer> weather_cbuffer;
		//misc
		std::unique_ptr<DescriptorHeap> null_heap;
		std::array<DirectX::XMVECTOR, SSAO_KERNEL_SIZE> ssao_kernel{};

		//passes
		GBufferPass  gbuffer_pass;
		AmbientPass  ambient_pass;
		ToneMapPass  tonemap_pass;
		FXAAPass	 fxaa_pass;
		SkyPass		 sky_pass;
		LightingPass lighting_pass;
		TiledLightingPass tiled_lighting_pass;
		CopyToTexturePass copy_to_texture_pass;
		ShadowPass    shadow_pass;
		Postprocessor postprocessor;
	private:
		void CreateNullHeap();
		void CreateSizeDependentResources();
		void UpdatePersistentConstantBuffers(float32 dt);
		void CameraFrustumCulling();

		void ResolveToBackbuffer(RenderGraph& rg);
		void ResolveToTexture(RenderGraph& rg);
	};
}