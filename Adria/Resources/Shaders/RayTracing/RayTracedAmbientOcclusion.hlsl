#include "RayTracingUtil.hlsli"

struct RayTracedAmbientOcclusionConstants
{
	uint  depthIdx;
	uint  normalsIdx;
	uint  outputIdx;
	float aoRadius;
	float aoPower;
};

ConstantBuffer<RayTracedAmbientOcclusionConstants> RayTracedAmbientOcclusionPassCB : register(b1);


struct [raypayload] AORayData
{
	float tHit : read(caller) : write(caller, miss, anyhit);
};

static const int RAY_COUNT = 1;

[shader("raygeneration")]
void RTAO_RayGen()
{
    RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[FrameCB.accelStructIdx];
	Texture2D<float> depthTexture = ResourceDescriptorHeap[RayTracedAmbientOcclusionPassCB.depthIdx];
	Texture2D normalTexture = ResourceDescriptorHeap[RayTracedAmbientOcclusionPassCB.normalsIdx];
	RWTexture2D<float> outputTexture = ResourceDescriptorHeap[RayTracedAmbientOcclusionPassCB.outputIdx];

	uint2 launchIndex = DispatchRaysIndex().xy;
	uint2 launchDim = DispatchRaysDimensions().xy;

	float depth = depthTexture.Load(int3(launchIndex.xy, 0)).r;
	float2 texCoords = (launchIndex + 0.5f) / FrameCB.renderResolution;
	float3 worldPosition = GetWorldPosition(texCoords, depth);
	float3 viewNormal = normalTexture.Load(int3(launchIndex.xy, 0)).xyz;
	viewNormal = 2.0 * viewNormal - 1.0;
	float3 worldNormal = normalize(mul(viewNormal, (float3x3) FrameCB.inverseView));

	uint randSeed = InitRand(launchIndex.x + launchIndex.y * launchDim.x, 47, 16);

	float3 worldDir = GetCosHemisphereSample(randSeed, worldNormal);
	AORayData rayPayload = { true };
	RayDesc rayAO;
	rayAO.Origin = OffsetRay(worldPosition, worldNormal);
	rayAO.Direction = normalize(worldDir);
	rayAO.TMin = 0.02f;
	rayAO.TMax = RayTracedAmbientOcclusionPassCB.aoRadius;

	TraceRay(tlas,
		(RAY_FLAG_CULL_BACK_FACING_TRIANGLES),
		0xFF, 0, 0, 0, rayAO, rayPayload);

	outputTexture[launchIndex.xy] = rayPayload.tHit < 0.0f ? 1.0f : pow(saturate(rayPayload.tHit / RayTracedAmbientOcclusionPassCB.aoRadius), RayTracedAmbientOcclusionPassCB.aoPower);
}

[shader("miss")]
void RTAO_Miss(inout AORayData hitData)
{
	hitData.tHit = -1;
}

[shader("anyhit")]
void RTAO_AnyHit(inout AORayData hitData, in BuiltInTriangleIntersectionAttributes attribs)
{
	hitData.tHit = RayTCurrent();
}