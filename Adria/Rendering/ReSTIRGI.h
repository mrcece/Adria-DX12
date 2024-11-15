#pragma once
#include "Graphics/GfxDescriptor.h"
#include "entt/entity/fwd.hpp"


namespace adria
{
	class GfxBuffer;
	class GfxTexture;
	class GfxDevice;
	class GfxComputePipelineState;
	class RenderGraph;

	class ReSTIRGI
	{
		enum class ResamplingMode
		{
			None = 0,
			Temporal = 1,
			Spatial = 2,
			TemporalAndSpatial = 3
		};

	public:
		ReSTIRGI(GfxDevice* gfx, Uint32 width, Uint32 height);

		void AddPasses(RenderGraph& rg);

		void OnResize(Uint32 w, Uint32 h)
		{
			width = w, height = h;
		}

	private:
		GfxDevice* gfx;
		Uint32 width, height;

		Bool enable = false;
		ResamplingMode resampling_mode = ResamplingMode::TemporalAndSpatial;

		struct TemporalReservoirBuffers
		{
			std::unique_ptr<GfxTexture> sample_radiance;
			std::unique_ptr<GfxTexture> ray_direction;
			std::unique_ptr<GfxTexture> reservoir;
		};
		TemporalReservoirBuffers temporal_reservoir_buffers[2];

		std::unique_ptr<GfxComputePipelineState> initial_sampling_pso;

	private:
		void AddInitialSamplingPass(RenderGraph& rg);
		void AddTemporalResamplingPass(RenderGraph& rg);
		void AddSpatialResamplingPass(RenderGraph& rg);

		void CreateBuffers();
	};
}