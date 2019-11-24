#include "DeformMesh/DeformableGridMeshComponent.h"
#include "RenderingThread.h"
#include "RenderResource.h"
#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"
#include "VertexFactory.h"
#include "MaterialShared.h"
#include "Engine/CollisionProfile.h"
#include "Materials/Material.h"
#include "LocalVertexFactory.h"
#include "SceneManagement.h"
#include "DynamicMeshBuilder.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Common/ComputableVertexBuffers.h"
#include "SinWaveGridMeshDeformer.h"
#if 0 // TODO:���ŃC���N���[�h���Ă���RaytracingDefinitions.h��Shaders/Shared�̃w�b�_�ŃC���N���[�h�ł��Ȃ��B����ă��C�g���Ή��ł��Ȃ�
#include "RayTracingInstance.h"
#endif

/** almost all is copy of FCustomMeshSceneProxy. */
class FDeformableGridMeshSceneProxy final : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	FDeformableGridMeshSceneProxy(UDeformableGridMeshComponent* Component)
		: FPrimitiveSceneProxy(Component)
		, VertexFactory(GetScene().GetFeatureLevel(), "FDeformableGridMeshSceneProxy")
		, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
	{
		TArray<FDynamicMeshVertex> Vertices;
		Vertices.Reset(Component->GetVertices().Num());
		IndexBuffer.Indices = Component->GetIndices();

		for (int32 VertIdx = 0; VertIdx < Component->GetVertices().Num(); VertIdx++)
		{
			// TODO:Color��Tangent�͂Ƃ肠����FDynamicMeshVertex�̃f�t�H���g�l�܂����ɂ���
			Vertices.Emplace(Component->GetVertices()[VertIdx]);
		}
		VertexBuffers.InitFromDynamicVertex(&VertexFactory, Vertices);

		// Enqueue initialization of render resource
		BeginInitResource(&VertexBuffers.PositionVertexBuffer);
		BeginInitResource(&VertexBuffers.ComputableMeshVertexBuffer);
		BeginInitResource(&VertexBuffers.ColorVertexBuffer);
		BeginInitResource(&IndexBuffer);
		BeginInitResource(&VertexFactory);

		// Grab material
		Material = Component->GetMaterial(0);
		if(Material == NULL)
		{
			Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}

#if 0 // TODO:
#if RHI_RAYTRACING
		if (IsRayTracingEnabled())
		{
			ENQUEUE_RENDER_COMMAND(InitProceduralMeshRayTracingGeometry)(
				[this](FRHICommandListImmediate& RHICmdList)
			{
				FRayTracingGeometryInitializer Initializer;
				Initializer.PositionVertexBuffer = nullptr;
				Initializer.IndexBuffer = nullptr;
				Initializer.BaseVertexIndex = 0;
				Initializer.VertexBufferStride = 12;
				Initializer.VertexBufferByteOffset = 0;
				Initializer.TotalPrimitiveCount = 0;
				Initializer.VertexBufferElementType = VET_Float3;
				Initializer.GeometryType = RTGT_Triangles;
				Initializer.bFastBuild = true;
				Initializer.bAllowUpdate = false;
				VertexBuffers.RayTracingGeometry.SetInitializer(Initializer);
				VertexBuffers.RayTracingGeometry.InitResource();

				VertexBuffers.RayTracingGeometry.Initializer.PositionVertexBuffer = VertexBuffers.PositionVertexBuffer.VertexBufferRHI;
				VertexBuffers.RayTracingGeometry.Initializer.IndexBuffer = IndexBuffer.IndexBufferRHI;
				VertexBuffers.RayTracingGeometry.Initializer.TotalPrimitiveCount = IndexBuffer.Indices.Num() / 3;

				//#dxr_todo: add support for segments?
				
				VertexBuffers.RayTracingGeometry.UpdateRHI();
			});
		}
#endif
#endif
	}

	virtual ~FDeformableGridMeshSceneProxy()
	{
#if 0
#if RHI_RAYTRACING
		if (IsRayTracingEnabled())
		{
			VertexBuffers.RayTracingGeometry.ReleaseResource();
		}
#endif
#endif
		VertexBuffers.PositionVertexBuffer.ReleaseResource();
		VertexBuffers.ComputableMeshVertexBuffer.ReleaseResource();
		VertexBuffers.ColorVertexBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		QUICK_SCOPE_CYCLE_COUNTER( STAT_DeformableGridMeshSceneProxy_GetDynamicMeshElements );

		const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

		auto WireframeMaterialInstance = new FColoredMaterialRenderProxy(
			GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy() : NULL,
			FLinearColor(0, 0.5f, 1.f)
			);

		Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);

		FMaterialRenderProxy* MaterialProxy = NULL;
		if(bWireframe)
		{
			MaterialProxy = WireframeMaterialInstance;
		}
		else
		{
			MaterialProxy = Material->GetRenderProxy();
		}

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];
				// Draw the mesh.
				FMeshBatch& Mesh = Collector.AllocateMesh();
				FMeshBatchElement& BatchElement = Mesh.Elements[0];
				BatchElement.IndexBuffer = &IndexBuffer;
				Mesh.bWireframe = bWireframe;
				Mesh.VertexFactory = &VertexFactory;
				Mesh.MaterialRenderProxy = MaterialProxy;

				bool bHasPrecomputedVolumetricLightmap;
				FMatrix PreviousLocalToWorld;
				int32 SingleCaptureIndex;
				bool bOutputVelocity;
				GetScene().GetPrimitiveUniformShaderParameters_RenderThread(GetPrimitiveSceneInfo(), bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);

				FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
				DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, DrawsVelocity(), false);
				BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

				BatchElement.FirstIndex = 0;
				BatchElement.NumPrimitives = IndexBuffer.Indices.Num() / 3;
				BatchElement.MinVertexIndex = 0;
				BatchElement.MaxVertexIndex = VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;
				Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
				Mesh.Type = PT_TriangleList;
				Mesh.DepthPriorityGroup = SDPG_World;
				Mesh.bCanApplyViewModeOverrides = false;
				Collector.AddMesh(ViewIndex, Mesh);
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bDynamicRelevance = true;
		Result.bRenderInMainPass = ShouldRenderInMainPass();
		Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
		Result.bRenderCustomDepth = ShouldRenderCustomDepth();
		Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		Result.bVelocityRelevance = IsMovable() && Result.bOpaqueRelevance && Result.bRenderInMainPass;
		return Result;
	}

	virtual bool CanBeOccluded() const override
	{
		return !MaterialRelevance.bDisableDepthTest;
	}

	virtual uint32 GetMemoryFootprint( void ) const override { return( sizeof( *this ) + GetAllocatedSize() ); }

	uint32 GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

#if 0
#if RHI_RAYTRACING
	virtual bool IsRayTracingRelevant() const override { return true; }

	virtual void GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances) override
	{
		if (VertexBuffers.RayTracingGeometry.RayTracingGeometryRHI.IsValid())
		{
			check(VertexBuffers.RayTracingGeometry.Initializer.PositionVertexBuffer.IsValid());
			check(VertexBuffers.RayTracingGeometry.Initializer.IndexBuffer.IsValid());

			FRayTracingInstance RayTracingInstance;
			RayTracingInstance.Geometry = &VertexBuffers.RayTracingGeometry;
			RayTracingInstance.InstanceTransforms.Add(GetLocalToWorld());

			uint32 SectionIdx = 0;
			FMeshBatch MeshBatch;

			MeshBatch.VertexFactory = &VertexFactory;
			MeshBatch.SegmentIndex = 0;
			MeshBatch.MaterialRenderProxy = Material->GetRenderProxy();
			MeshBatch.ReverseCulling = IsLocalToWorldDeterminantNegative();
			MeshBatch.Type = PT_TriangleList;
			MeshBatch.DepthPriorityGroup = SDPG_World;
			MeshBatch.bCanApplyViewModeOverrides = false;

			FMeshBatchElement& BatchElement = MeshBatch.Elements[0];
			BatchElement.IndexBuffer = &IndexBuffer;

			bool bHasPrecomputedVolumetricLightmap;
			FMatrix PreviousLocalToWorld;
			int32 SingleCaptureIndex;
			bool bOutputVelocity;
			GetScene().GetPrimitiveUniformShaderParameters_RenderThread(GetPrimitiveSceneInfo(), bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);

			FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Context.RayTracingMeshResourceCollector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
			DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, DrawsVelocity(), bOutputVelocity);
			BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

			BatchElement.FirstIndex = 0;
			BatchElement.NumPrimitives = IndexBuffer.Indices.Num() / 3;
			BatchElement.MinVertexIndex = 0;
			BatchElement.MaxVertexIndex = VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;

			RayTracingInstance.Materials.Add(MeshBatch);

			RayTracingInstance.BuildInstanceMaskAndFlags();
			OutRayTracingInstances.Add(RayTracingInstance);
		}
	}
#endif
#endif

	void EnqueDeformableGridMeshRenderCommand(UDeformableGridMeshComponent* Component) const
	{
		FGridSinWaveParameters Params;
		Params.NumRow = Component->GetNumRow();
		Params.NumColumn = Component->GetNumColumn();
		Params.NumVertex = Component->GetVertices().Num();
		Params.GridWidth = Component->GetGridWidth();
		Params.GridHeight = Component->GetGridHeight();
		Params.WaveLengthRow = Component->GetWaveLengthRow();
		Params.WaveLengthColumn = Component->GetWaveLengthColumn();
		Params.Period = Component->GetPeriod();
		Params.Amplitude = Component->GetAmplitude();
		Params.AccumulatedTime = Component->GetAccumulatedTime();

		ENQUEUE_RENDER_COMMAND(SinWaveDeformGridMeshCommand)(
			[this, Params](FRHICommandListImmediate& RHICmdList)
			{
				SinWaveDeformGridMesh(RHICmdList, Params, VertexBuffers.PositionVertexBuffer.GetUAV(), VertexBuffers.ComputableMeshVertexBuffer.GetTangentsUAV());
			}
		);
	}

private:

	UMaterialInterface* Material;
	FComputableVertexBuffers VertexBuffers;
	FDynamicMeshIndexBuffer32 IndexBuffer;
	FLocalVertexFactory VertexFactory;

	FMaterialRelevance MaterialRelevance;
};

//////////////////////////////////////////////////////////////////////////

UDeformableGridMeshComponent::UDeformableGridMeshComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

FPrimitiveSceneProxy* UDeformableGridMeshComponent::CreateSceneProxy()
{
	FPrimitiveSceneProxy* Proxy = NULL;
	if(_Vertices.Num() > 0 && _Indices.Num() > 0)
	{
		Proxy = new FDeformableGridMeshSceneProxy(this);
	}
	return Proxy;
}

void UDeformableGridMeshComponent::InitSetting(int32 NumRow, int32 NumColumn, float GridWidth, float GridHeight, float WaveLengthRow, float WaveLengthColumn, float Period, float Amplitude)
{
	_NumRow = NumRow;
	_NumColumn = NumColumn;
	_Vertices.Reset((NumRow + 1) * (NumColumn + 1));
	_Indices.Reset(NumRow * NumColumn * 2 * 3); // �ЂƂ̃O���b�h�ɂ�3��Triangle�A6�̒��_�C���f�b�N�X�w�肪����
	_WaveLengthRow = WaveLengthRow;
	_WaveLengthColumn = WaveLengthColumn;
	_Period = Period;
	_Amplitude = Amplitude;

	for (int32 y = 0; y < NumRow + 1; y++)
	{
		for (int32 x = 0; x < NumColumn + 1; x++)
		{
			_Vertices.Emplace(x * GridWidth, y * GridHeight, 0.0f);
		}
	}

	for (int32 Row = 0; Row < NumRow; Row++)
	{
		for (int32 Column = 0; Column < NumColumn; Column++)
		{
			_Indices.Emplace(Row * (NumColumn + 1) + Column);
			_Indices.Emplace((Row + 1) * (NumColumn + 1) + Column);
			_Indices.Emplace((Row + 1) * (NumColumn + 1) + Column + 1);

			_Indices.Emplace(Row * (NumColumn + 1) + Column);
			_Indices.Emplace((Row + 1) * (NumColumn + 1) + Column + 1);
			_Indices.Emplace(Row * (NumColumn + 1) + Column + 1);
		}
	}

	MarkRenderStateDirty();
	UpdateBounds();
}

int32 UDeformableGridMeshComponent::GetNumMaterials() const
{
	return 1;
}

FBoxSphereBounds UDeformableGridMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox BoundingBox(ForceInit);

	// Bounds are tighter if the box is generated from pre-transformed vertices.
	for (const FVector& Vertex : _Vertices)
	{
		BoundingBox += LocalToWorld.TransformPosition(Vertex);
	}

	FBoxSphereBounds NewBounds;
	NewBounds.BoxExtent = BoundingBox.GetExtent();
	NewBounds.Origin = BoundingBox.GetCenter();
	NewBounds.SphereRadius = NewBounds.BoxExtent.Size();

	return NewBounds;
}

void UDeformableGridMeshComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	_AccumulatedTime += DeltaTime;

	// reset by a big number
	if (_AccumulatedTime > 10000)
	{
		_AccumulatedTime = 0.0f;
	}

	MarkRenderDynamicDataDirty();
}

void UDeformableGridMeshComponent::SendRenderDynamicData_Concurrent()
{
	//SCOPE_CYCLE_COUNTER(STAT_DeformableGridMeshCompUpdate);
	Super::SendRenderDynamicData_Concurrent();

	if (SceneProxy != nullptr)
	{
		((const FDeformableGridMeshSceneProxy*)SceneProxy)->EnqueDeformableGridMeshRenderCommand(this);
	}
}
