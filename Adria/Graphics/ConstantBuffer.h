#pragma once
#include "GraphicsCoreDX12.h"
#include "d3dx12.h"
#include "DescriptorHeap.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include <memory>

namespace adria
{
	
	template<typename BufferType>
	class ConstantBuffer
	{
		static constexpr u32 GetCBufferSize()
		{
			return (sizeof(BufferType) + (D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1)) & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);
		}

	public:
		ConstantBuffer(ID3D12Device* device, u32 cbuffer_count)
			: cbuffer_size(GetCBufferSize()), cbuffer_count(cbuffer_count)
		{
			auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			auto buffer_desc = CD3DX12_RESOURCE_DESC::Buffer((u64)cbuffer_size * cbuffer_count);

			BREAK_IF_FAILED(device->CreateCommittedResource(
				&heap_properties,
				D3D12_HEAP_FLAG_NONE,
				&buffer_desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&cb)));

			CD3DX12_RANGE read_range(0, 0);
			BREAK_IF_FAILED(cb->Map(0, &read_range, reinterpret_cast<void**>(&_mapped_data)));
		}

		ConstantBuffer(ConstantBuffer const&) = delete;
		ConstantBuffer(ConstantBuffer&& o) noexcept
			: cb(std::move(o.cb)), cbuffer_size(o.cbuffer_size), _mapped_data(o._mapped_data)
		{
			o._mapped_data = nullptr;
		}

		ConstantBuffer& operator=(ConstantBuffer const&) = delete;
		ConstantBuffer& operator=(ConstantBuffer&&) = delete;

		~ConstantBuffer()
		{
			if (cb != nullptr)
				cb->Unmap(0, nullptr);

			_mapped_data = nullptr;
		}

		void Update(BufferType const& data, u32 cbuffer_index)
		{
			memcpy(&_mapped_data[cbuffer_index * cbuffer_size], &data, sizeof(BufferType)); //maybe change to cbuffer_size
		}
		void Update(void* data, u32 data_size, u32 cbuffer_index)
		{
			memcpy(&_mapped_data[cbuffer_index * cbuffer_size], data, data_size); 
		}
		D3D12_CONSTANT_BUFFER_VIEW_DESC View(u32 cbuffer_index) const
		{
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
			cbvDesc.BufferLocation = cb->GetGPUVirtualAddress() + (u64)cbuffer_index * cbuffer_size;
			cbvDesc.SizeInBytes = cbuffer_size;
			return cbvDesc;
		}

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> cb;
		u8* _mapped_data = nullptr;
		u32 const cbuffer_size;
		u32 const cbuffer_count;
	};


}