#pragma once
#include <memory>
#include "RenderGraph/RenderGraphResourceId.h"
#include "Graphics/GfxDefines.h"
#include "Graphics/GfxPSOPermutations.h"


namespace adria
{
	class RenderGraph;
	class TextureManager;
	class GfxDevice;
	class GfxTexture;
	class GfxBuffer;

	class GPUDrivenGBufferPass
	{
		static constexpr uint32 MAX_HZB_MIP_COUNT = 13;
		struct DebugStats
		{
			uint32 num_instances;
			uint32 occluded_instances;
			uint32 visible_instances;
			uint32 processed_meshlets;
			uint32 phase1_candidate_meshlets;
			uint32 phase2_candidate_meshlets;
			uint32 phase1_visible_meshlets;
			uint32 phase2_visible_meshlets;
		};


	public:
		GPUDrivenGBufferPass(entt::registry& reg, GfxDevice* gfx, uint32 width, uint32 height);

		void Render(RenderGraph& rg);

		void OnResize(uint32 w, uint32 h)
		{
			width = w, height = h;
			InitializeHZB();
		}

		void OnRainEvent(bool enabled)
		{
			rain_active = enabled;
		}

	private:
		GfxDevice* gfx;
		entt::registry& reg;
		uint32 width, height;

		std::unique_ptr<GfxTexture> HZB;
		uint32 hzb_mip_count = 0;
		uint32 hzb_width = 0;
		uint32 hzb_height = 0;

		bool occlusion_culling = true;

		std::unique_ptr<GfxBuffer> debug_buffer;
		bool display_debug_stats = false;
		DebugStats debug_stats[GFX_BACKBUFFER_COUNT];

		bool rain_active = false;
		MeshShaderPSOPermutations<2> draw_psos;
		ComputePSOPermutations<3>	 cull_meshlets_psos;
		ComputePSOPermutations<3>	 cull_instances_psos;
		ComputePSOPermutations<2>    build_meshlet_cull_args_psos;
		ComputePSOPermutations<2>    build_meshlet_draw_args_psos;
		std::unique_ptr<ComputePipelineState> clear_counters_pso;
		std::unique_ptr<ComputePipelineState> build_instance_cull_args_pso;
		std::unique_ptr<ComputePipelineState> initialize_hzb_pso;
		std::unique_ptr<ComputePipelineState> hzb_mips_pso;

	private:
		void CreatePSOs();
		void InitializeHZB();

		void AddClearCountersPass(RenderGraph& rg);
		void Add1stPhasePasses(RenderGraph& rg);
		void Add2ndPhasePasses(RenderGraph& rg);

		void AddHZBPasses(RenderGraph& rg, bool second_phase = false);
		void AddDebugPass(RenderGraph& rg);

		void CalculateHZBParameters();
		void CreateDebugBuffer();
	};

}