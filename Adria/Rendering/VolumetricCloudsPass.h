#pragma once
#include <vector>
#include "Core/CoreTypes.h"
#include "RenderGraph/RenderGraphResourceId.h"


namespace adria
{
	class RenderGraph;
	class GfxDevice;
	class GfxTexture;

	class VolumetricCloudsPass
	{
		struct CloudParameters
		{
			float crispiness = 43.0f;
			float curliness = 3.6f;
			float coverage = 0.505f;
			float light_absorption = 0.003f;
			float clouds_bottom_height = 3000.0f;
			float clouds_top_height = 10000.0f;
			float density_factor = 0.015f;
			float cloud_type = 1.0f;
		};
	public:
		VolumetricCloudsPass(uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph);
		void OnResize(GfxDevice* gfx, uint32 w, uint32 h);
		void OnSceneInitialized(GfxDevice* gfx);

	private:
		uint32 width, height;
		std::vector<size_t> cloud_textures;
		std::unique_ptr<GfxTexture> prev_clouds;
		CloudParameters params{};
	};

}