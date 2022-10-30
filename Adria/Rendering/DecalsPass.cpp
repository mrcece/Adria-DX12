#include "DecalsPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Graphics/TextureManager.h"
#include "entt/entity/registry.hpp"

using namespace DirectX;

namespace adria
{

	DecalsPass::DecalsPass(entt::registry& reg, TextureManager& texture_manager, uint32 w, uint32 h)
	 : reg{ reg }, texture_manager{ texture_manager }, width{ w }, height{ h }
	{}

	void DecalsPass::AddPass(RenderGraph& rendergraph)
	{
		if (reg.view<Decal>().size() == 0) return;
		GlobalBlackboardData const& global_data = rendergraph.GetBlackboard().GetChecked<GlobalBlackboardData>();

		struct DecalsPassData
		{
			RGTextureReadOnlyId depth_srv;
		};
		rendergraph.AddPass<DecalsPassData>("Decals Pass",
			[=](DecalsPassData& data, RenderGraphBuilder& builder)
			{
				data.depth_srv = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_PixelShader);
				builder.WriteRenderTarget(RG_RES_NAME(GBufferAlbedo), ERGLoadStoreAccessOp::Preserve_Preserve);
				builder.WriteRenderTarget(RG_RES_NAME(GBufferNormal), ERGLoadStoreAccessOp::Preserve_Preserve);
				builder.SetViewport(width, height);
			},
			[=](DecalsPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				auto upload_buffer = gfx->GetDynamicAllocator();

				uint32 depth_idx = (uint32)descriptor_allocator->Allocate();
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(depth_idx), context.GetReadOnlyTexture(data.depth_srv),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				struct DecalsConstants
				{
					XMMATRIX model_matrix;
					XMMATRIX transposed_inverse_model;
					uint32 decal_type;
					uint32 decal_albedo_idx;
					uint32 decal_normal_idx;
					uint32 depth_idx;
				} constants = 
				{
					.depth_idx = depth_idx
				};

				cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::Common));
				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.new_frame_cbuffer_address);
				auto decal_view = reg.view<Decal>();

				auto decal_pass_lambda = [&](bool modify_normals)
				{
					cmd_list->SetPipelineState(PSOCache::Get(modify_normals ? EPipelineState::Decals_ModifyNormals : EPipelineState::Decals));
					for (auto e : decal_view)
					{
						Decal& decal = decal_view.get<Decal>(e);
						if (decal.modify_gbuffer_normals != modify_normals) continue;

						constants.model_matrix = decal.decal_model_matrix;
						constants.transposed_inverse_model = XMMatrixTranspose(XMMatrixInverse(nullptr, decal.decal_model_matrix));
						constants.decal_type = static_cast<uint32>(decal.decal_type);
						constants.decal_albedo_idx = (uint32)decal.albedo_decal_texture;
						constants.decal_normal_idx = (uint32)decal.normal_decal_texture;
						DynamicAllocation allocation = upload_buffer->Allocate(GetCBufferSize<DecalsConstants>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
						allocation.Update(constants);
						cmd_list->SetGraphicsRootConstantBufferView(2, allocation.gpu_address);
						cmd_list->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
						BindVertexBuffer(cmd_list, cube_vb.get());
						BindIndexBuffer(cmd_list, cube_ib.get());
						cmd_list->DrawIndexedInstanced(cube_ib->GetCount(), 1, 0, 0, 0);
					}
				};
				decal_pass_lambda(false);
				decal_pass_lambda(true);
			}, ERGPassType::Graphics, ERGPassFlags::None);
	}

	void DecalsPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void DecalsPass::OnSceneInitialized(GraphicsDevice* gfx)
	{
		CreateCubeBuffers(gfx);
	}

	void DecalsPass::CreateCubeBuffers(GraphicsDevice* gfx)
	{
		SimpleVertex const cube_vertices[8] =
		{
			XMFLOAT3{ -0.5f, -0.5f,  0.5f },
			XMFLOAT3{  0.5f, -0.5f,  0.5f },
			XMFLOAT3{  0.5f,  0.5f,  0.5f },
			XMFLOAT3{ -0.5f,  0.5f,  0.5f },
			XMFLOAT3{ -0.5f, -0.5f, -0.5f },
			XMFLOAT3{  0.5f, -0.5f, -0.5f },
			XMFLOAT3{  0.5f,  0.5f, -0.5f },
			XMFLOAT3{ -0.5f,  0.5f, -0.5f }
		};

		uint16_t const cube_indices[36] =
		{
			// front
			0, 1, 2,
			2, 3, 0,
			// right
			1, 5, 6,
			6, 2, 1,
			// back
			7, 6, 5,
			5, 4, 7,
			// left
			4, 0, 3,
			3, 7, 4,
			// bottom
			4, 5, 1,
			1, 0, 4,
			// top
			3, 2, 6,
			6, 7, 3
		};

		BufferDesc vb_desc{};
		vb_desc.bind_flags = EBindFlag::None;
		vb_desc.size = sizeof(cube_vertices);
		vb_desc.stride = sizeof(SimpleVertex);
		cube_vb = std::make_unique<Buffer>(gfx, vb_desc, cube_vertices);

		BufferDesc ib_desc{};
		ib_desc.bind_flags = EBindFlag::None;
		ib_desc.format = EFormat::R16_UINT;
		ib_desc.stride = sizeof(uint16);
		ib_desc.size = sizeof(cube_indices);
		cube_ib = std::make_unique<Buffer>(gfx, ib_desc, cube_indices);
	}

}

