#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include "../Core/Definitions.h"

namespace adria
{
	struct GlobalBlackboardData
	{
		DirectX::XMVECTOR			camera_position;
		DirectX::XMMATRIX			camera_view;
		DirectX::XMMATRIX			camera_proj;
		DirectX::XMMATRIX			camera_viewproj;
		float						camera_fov;
		D3D12_GPU_VIRTUAL_ADDRESS   new_frame_cbuffer_address;
		D3D12_GPU_VIRTUAL_ADDRESS   frame_cbuffer_address;
		D3D12_CPU_DESCRIPTOR_HANDLE null_srv_texture2d;
		D3D12_CPU_DESCRIPTOR_HANDLE null_uav_texture2d;
		D3D12_CPU_DESCRIPTOR_HANDLE null_srv_texturecube;
		D3D12_CPU_DESCRIPTOR_HANDLE null_srv_texture2darray;
		D3D12_CPU_DESCRIPTOR_HANDLE white_srv_texture2d;
		D3D12_CPU_DESCRIPTOR_HANDLE lights_buffer_cpu_srv;
	};

	struct DoFBlackboardData
	{
		float dof_params_x;
		float dof_params_y;
		float dof_params_z;
		float dof_params_w;
	};
}