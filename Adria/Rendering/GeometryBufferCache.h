#pragma once
#include <memory>
#include "Core/CoreTypes.h"
#include "Graphics/GfxDescriptor.h"
#include "Utilities/HashMap.h"
#include "Utilities/Singleton.h"

namespace adria
{
	class GfxDevice;
	class GfxBuffer;

	inline constexpr uint64 INVALID_GEOMETRY_BUFFER_HANDLE = -1;

	class GeometryBufferHandle
	{
		friend class ArcGeometryBufferHandle;

	public:

		~GeometryBufferHandle();
		operator uint64() const { return handle; }
		GeometryBufferHandle operator ++() const { return handle + 1; }
		bool IsValid() const { return handle != INVALID_GEOMETRY_BUFFER_HANDLE; }

	private:
		GeometryBufferHandle() = default;
		GeometryBufferHandle(uint64 h) : handle(h) {}
		GeometryBufferHandle(GeometryBufferHandle const&) = default;
		GeometryBufferHandle(GeometryBufferHandle&&) = default;
		GeometryBufferHandle& operator=(GeometryBufferHandle const&) = default;
		GeometryBufferHandle& operator=(GeometryBufferHandle&&) = default;

	private:
		uint64 handle = INVALID_GEOMETRY_BUFFER_HANDLE;
	};

	class ArcGeometryBufferHandle
	{
	public:
		ArcGeometryBufferHandle() = default;
		ArcGeometryBufferHandle(uint64 h) : handle{ new GeometryBufferHandle(h) } {}
		~ArcGeometryBufferHandle() = default;

		operator GeometryBufferHandle&() const { return *handle; }
		bool IsValid() const { return handle->IsValid(); }
	private:
		std::shared_ptr<GeometryBufferHandle> handle;
	};


	class GeometryBufferCache : public Singleton<GeometryBufferCache>
	{
		friend class Singleton<GeometryBufferCache>;

	public:

		void Initialize(GfxDevice* _gfx);
		void Destroy();

		[[nodiscard]] ArcGeometryBufferHandle CreateAndInitializeGeometryBuffer(GfxBuffer* staging_buffer, uint64 total_buffer_size, uint64 src_offset);
		[[nodiscard]] GfxBuffer* GetGeometryBuffer(GeometryBufferHandle& handle) const;
		[[nodiscard]] GfxDescriptor GetGeometryBufferSRV(GeometryBufferHandle& handle) const;
		void DestroyGeometryBuffer(GeometryBufferHandle& handle);

	private:
		GfxDevice* gfx;
		uint64 current_handle = INVALID_GEOMETRY_BUFFER_HANDLE;
		HashMap<uint64, std::unique_ptr<GfxBuffer>> buffer_map;
		HashMap<uint64, GfxDescriptor> buffer_srv_map;
	};
	#define g_GeometryBufferCache GeometryBufferCache::Get()
}