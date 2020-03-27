#include "Ocean/OceanGridMeshComponent.h"
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
#include "DeformMesh/DeformableVertexBuffers.h"
#include "Ocean/OceanSimulator.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "ResourceArrayStructuredBuffer.h"

using namespace OceanSimulator;

/** almost all is copy of FCustomMeshSceneProxy. */
class FOceanGridMeshSceneProxy final : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	FOceanGridMeshSceneProxy(UOceanGridMeshComponent* Component)
		: FPrimitiveSceneProxy(Component)
		, VertexFactory(GetScene().GetFeatureLevel(), "FOceanGridMeshSceneProxy")
		, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
	{
		TArray<FDynamicMeshVertex> Vertices;
		Vertices.Reset(Component->GetVertices().Num());
		IndexBuffer.Indices = Component->GetIndices();

		for (int32 VertIdx = 0; VertIdx < Component->GetVertices().Num(); VertIdx++)
		{
			// TODO:Tangent�͂Ƃ肠����FDynamicMeshVertex�̃f�t�H���g�l�܂����ɂ���BColor��DynamicMeshVertex�̃f�t�H���g�l���̗p���Ă���
			Vertices.Emplace(Component->GetVertices()[VertIdx], Component->GetTexCoords()[VertIdx], FColor(255, 255, 255));
		}
		VertexBuffers.InitFromDynamicVertex(&VertexFactory, Vertices);

		// Enqueue initialization of render resource
		BeginInitResource(&VertexBuffers.PositionVertexBuffer);
		BeginInitResource(&VertexBuffers.DeformableMeshVertexBuffer);
		BeginInitResource(&VertexBuffers.ColorVertexBuffer);
		BeginInitResource(&IndexBuffer);
		BeginInitResource(&VertexFactory);

		// Grab material
		Material = Component->GetMaterial(0);
		if(Material == NULL)
		{
			Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}

		int32 SizeX, SizeY;
		Component->GetDisplacementMap()->GetSize(SizeX, SizeY);
		check(SizeX == SizeY); // �����`�ł���O��
		check(FMath::IsPowerOfTwo(SizeX)); // 2�̗ݏ�̃T�C�Y�ł���O��
		uint32 DispMapDimension = SizeX;
		
		// Phyllips Spectrum���g����������
		// Height map H(0)
		H0Data.Init(FComplex::ZeroVector, DispMapDimension * DispMapDimension);
		// FComplex::ZeroVector�Ƃ������O�������i�D�������AZero�݂����ȐV�����萔����낤�Ǝv����typedef FVector2D FComplex�ł͂ł��Ȃ��̂ō��͑Ë�����

		Omega0Data.Init(0.0f, DispMapDimension * DispMapDimension);

		FOceanSpectrumParameters Params;
		Params.DispMapDimension = DispMapDimension; // TODO:�����`�O��
		Params.PatchLength = Component->GetGridWidth() * Component->GetNumColumn(); // ���̃R���|�[�l���g���ƃO���b�h���b�V����PatchLength�ɍ��킹���X�P�[�����O�͍s���Ă��炸�A���b�V���T�C�Y��GridWidth��NumColumn���猈�߂Ă���B�����`��O��ɂ��Ă���
		Params.AmplitudeScale = Component->GetAmplitudeScale();
		Params.WindDirection = Component->GetWindDirection();
		Params.WindSpeed = Component->GetWindSpeed();
		Params.WindDependency = Component->GetWindDependency();
		Params.ChoppyScale = Component->GetChoppyScale();
		Params.AccumulatedTime = Component->GetAccumulatedTime() * Component->GetTimeScale();
		Params.DxyzDebugAmplitude = Component->DxyzDebugAmplitude;

		CreateInitialHeightMap(Params, Component->GetWorld()->GetGravityZ(), H0Data, Omega0Data);

		H0Buffer.Initialize(H0Data, sizeof(FComplex));
		Omega0Buffer.Initialize(Omega0Data, sizeof(float));

		HtZeroInitData.Init(FComplex::ZeroVector, DispMapDimension * DispMapDimension);
		HtBuffer.Initialize(HtZeroInitData, sizeof(FComplex));

		DkxZeroInitData.Init(FComplex::ZeroVector, DispMapDimension * DispMapDimension);
		DkxBuffer.Initialize(DkxZeroInitData, sizeof(FComplex));

		DkyZeroInitData.Init(FComplex::ZeroVector, DispMapDimension * DispMapDimension);
		DkyBuffer.Initialize(DkyZeroInitData, sizeof(FComplex));

		FFTWorkZeroInitData.Init(FComplex::ZeroVector, DispMapDimension * DispMapDimension);
		FFTWorkBuffer.Initialize(FFTWorkZeroInitData, sizeof(FComplex));

		DxZeroInitData.Init(0.0f, DispMapDimension * DispMapDimension);
		DxBuffer.Initialize(DxZeroInitData, sizeof(float));

		DyZeroInitData.Init(0.0f, DispMapDimension * DispMapDimension);
		DyBuffer.Initialize(DyZeroInitData, sizeof(float));

		DzZeroInitData.Init(0.0f, DispMapDimension * DispMapDimension);
		DzBuffer.Initialize(DyZeroInitData, sizeof(float));
	}

	virtual ~FOceanGridMeshSceneProxy()
	{
		VertexBuffers.PositionVertexBuffer.ReleaseResource();
		VertexBuffers.DeformableMeshVertexBuffer.ReleaseResource();
		VertexBuffers.ColorVertexBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();
		H0Buffer.ReleaseResource();
		Omega0Buffer.ReleaseResource();
		HtBuffer.ReleaseResource();
		FFTWorkBuffer.ReleaseResource();
		DkxBuffer.ReleaseResource();
		DkyBuffer.ReleaseResource();
		DxBuffer.ReleaseResource();
		DyBuffer.ReleaseResource();
		DzBuffer.ReleaseResource();
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		QUICK_SCOPE_CYCLE_COUNTER( STAT_OceanGridMeshSceneProxy_GetDynamicMeshElements );

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

	void EnqueTestSinWaveCommand(FRHICommandListImmediate& RHICmdList, UOceanGridMeshComponent* Component) const
	{
		FTextureRenderTargetResource* TextureRenderTargetResource = Component->GetDisplacementMap()->GetRenderTargetResource();
		if (TextureRenderTargetResource == nullptr)
		{
			return;
		}

		FOceanSinWaveParameters Params;
		Params.MapWidth = TextureRenderTargetResource->GetSizeX();
		Params.MapHeight = TextureRenderTargetResource->GetSizeY();
		Params.MeshWidth = Component->GetGridWidth() * Component->GetNumColumn();
		Params.MeshHeight = Component->GetGridHeight() * Component->GetNumRow();
		Params.WaveLengthRow = Component->GetWaveLengthRow();
		Params.WaveLengthColumn = Component->GetWaveLengthColumn();
		Params.Period = Component->GetPeriod();
		Params.Amplitude = Component->GetAmplitude();
		Params.AccumulatedTime = Component->GetAccumulatedTime();

		TestSinWave(RHICmdList, Params, Component->GetDisplacementMapUAV());
	}

	void EnqueSimulateOceanCommand(FRHICommandListImmediate& RHICmdList, UOceanGridMeshComponent* Component) const
	{
		FTextureRenderTargetResource* TextureRenderTargetResource = Component->GetDisplacementMap()->GetRenderTargetResource();
		if (TextureRenderTargetResource == nullptr)
		{
			return;
		}

		FOceanSpectrumParameters Params;
		Params.DispMapDimension = TextureRenderTargetResource->GetSizeX(); // TODO:�����`�O���SizeY�͌��ĂȂ�
		Params.PatchLength = Component->GetGridWidth() * Component->GetNumColumn(); // ���̃R���|�[�l���g���ƃ��b�V����PatchLength�ɍ��킹���X�P�[�����O�͍s���Ă��炸�AGridWidth�ANumColumn�Ȃǂ�
		Params.AmplitudeScale = Component->GetAmplitudeScale();
		Params.WindDirection = Component->GetWindDirection();
		Params.WindSpeed = Component->GetWindSpeed();
		Params.WindDependency = Component->GetWindDependency();
		Params.ChoppyScale = Component->GetChoppyScale();
		Params.AccumulatedTime = Component->GetAccumulatedTime() * Component->GetTimeScale();
		Params.DxyzDebugAmplitude = Component->DxyzDebugAmplitude;

		FOceanBufferViews Views;
		Views.H0SRV = H0Buffer.GetSRV();
		Views.OmegaSRV = Omega0Buffer.GetSRV();
		Views.HtSRV = HtBuffer.GetSRV();
		Views.HtUAV = HtBuffer.GetUAV();
		Views.DkxSRV = DkxBuffer.GetSRV();
		Views.DkxUAV = DkxBuffer.GetUAV();
		Views.DkySRV = DkyBuffer.GetSRV();
		Views.DkyUAV = DkyBuffer.GetUAV();
		Views.FFTWorkUAV = FFTWorkBuffer.GetUAV();
		Views.DxSRV = DxBuffer.GetSRV();
		Views.DxUAV = DxBuffer.GetUAV();
		Views.DySRV = DyBuffer.GetSRV();
		Views.DyUAV = DyBuffer.GetUAV();
		Views.DzSRV = DzBuffer.GetSRV();
		Views.DzUAV = DzBuffer.GetUAV();
		Views.H0DebugViewUAV = Component->GetH0DebugViewUAV();
		Views.HtDebugViewUAV = Component->GetHtDebugViewUAV();
		Views.DkxDebugViewUAV = Component->GetDkxDebugViewUAV();
		Views.DkyDebugViewUAV = Component->GetDkyDebugViewUAV();
		Views.DxyzDebugViewUAV = Component->GetDxyzDebugViewUAV();
		Views.DisplacementMapSRV = Component->GetDisplacementMapSRV();
		Views.DisplacementMapUAV = Component->GetDisplacementMapUAV();
		Views.GradientFoldingMapUAV = Component->GetGradientFoldingMapUAV();

		SimulateOcean(RHICmdList, Params, Views);
	}

private:

	UMaterialInterface* Material;
	FDeformableVertexBuffers VertexBuffers;
	FDynamicMeshIndexBuffer32 IndexBuffer;
	FLocalVertexFactory VertexFactory;

	TResourceArray<FComplex> H0Data;
	TResourceArray<float> Omega0Data;
	TResourceArray<FComplex> HtZeroInitData;
	TResourceArray<FComplex> DkxZeroInitData;
	TResourceArray<FComplex> DkyZeroInitData;
	TResourceArray<FComplex> FFTWorkZeroInitData;
	TResourceArray<float> DxZeroInitData;
	TResourceArray<float> DyZeroInitData;
	TResourceArray<float> DzZeroInitData;
	FResourceArrayStructuredBuffer H0Buffer;
	FResourceArrayStructuredBuffer Omega0Buffer;
	FResourceArrayStructuredBuffer HtBuffer;
	FResourceArrayStructuredBuffer DkxBuffer;
	FResourceArrayStructuredBuffer DkyBuffer;
	FResourceArrayStructuredBuffer FFTWorkBuffer;
	FResourceArrayStructuredBuffer DxBuffer;
	FResourceArrayStructuredBuffer DyBuffer;
	FResourceArrayStructuredBuffer DzBuffer;

	FMaterialRelevance MaterialRelevance;
};

//////////////////////////////////////////////////////////////////////////

UOceanGridMeshComponent::UOceanGridMeshComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

UOceanGridMeshComponent::~UOceanGridMeshComponent()
{
	if (_DisplacementMapSRV.IsValid())
	{
		_DisplacementMapSRV.SafeRelease();
	}

	if (_DisplacementMapUAV.IsValid())
	{
		_DisplacementMapUAV.SafeRelease();
	}

	if (_GradientFoldingMapUAV.IsValid())
	{
		_GradientFoldingMapUAV.SafeRelease();
	}

	if (_H0DebugViewUAV.IsValid())
	{
		_H0DebugViewUAV.SafeRelease();
	}

	if (_HtDebugViewUAV.IsValid())
	{
		_HtDebugViewUAV.SafeRelease();
	}

	if (_DkxDebugViewUAV.IsValid())
	{
		_DkxDebugViewUAV.SafeRelease();
	}

	if (_DkyDebugViewUAV.IsValid())
	{
		_DkyDebugViewUAV.SafeRelease();
	}

	if (_DxyzDebugViewUAV.IsValid())
	{
		_DxyzDebugViewUAV.SafeRelease();
	}
}

FPrimitiveSceneProxy* UOceanGridMeshComponent::CreateSceneProxy()
{
	FPrimitiveSceneProxy* Proxy = NULL;
	if(_Vertices.Num() > 0 && _Indices.Num() > 0 && DisplacementMap != nullptr && GradientFoldingMap != nullptr)
	{
		Proxy = new FOceanGridMeshSceneProxy(this);
	}
	return Proxy;
}

void UOceanGridMeshComponent::SetSinWaveSettings(float WaveLengthRow, float WaveLengthColumn, float Period, float Amplitude)
{
	_WaveLengthRow = WaveLengthRow;
	_WaveLengthColumn = WaveLengthColumn;
	_Period = Period;
	_Amplitude = Amplitude;

	if (_DisplacementMapUAV.IsValid())
	{
		_DisplacementMapUAV.SafeRelease();
	}

	if (DisplacementMap != nullptr)
	{
		_DisplacementMapUAV = RHICreateUnorderedAccessView(DisplacementMap->GameThread_GetRenderTargetResource()->TextureRHI);
	}
}

void UOceanGridMeshComponent::SetOceanSpectrumSettings(float TimeScale, float AmplitudeScale, FVector2D WindDirection, float WindSpeed, float WindDependency, float ChoppyScale)
{
	_TimeScale = TimeScale;
	_AmplitudeScale = AmplitudeScale;
	_WindDirection = WindDirection.GetSafeNormal(); // ���K�����Ă���
	_WindSpeed = WindSpeed;
	_WindDependency = WindDependency;
	_ChoppyScale = ChoppyScale;

	if (_DisplacementMapSRV.IsValid())
	{
		_DisplacementMapSRV.SafeRelease();
	}

	if (DisplacementMap != nullptr)
	{
		_DisplacementMapSRV = RHICreateShaderResourceView(DisplacementMap->GameThread_GetRenderTargetResource()->TextureRHI, 0);
	}

	if (_DisplacementMapUAV.IsValid())
	{
		_DisplacementMapUAV.SafeRelease();
	}

	if (DisplacementMap != nullptr)
	{
		_DisplacementMapUAV = RHICreateUnorderedAccessView(DisplacementMap->GameThread_GetRenderTargetResource()->TextureRHI);
	}

	if (_GradientFoldingMapUAV.IsValid())
	{
		_GradientFoldingMapUAV.SafeRelease();
	}

	if (GradientFoldingMap != nullptr)
	{
		_GradientFoldingMapUAV = RHICreateUnorderedAccessView(GradientFoldingMap->GameThread_GetRenderTargetResource()->TextureRHI);
	}

	if (_H0DebugViewUAV.IsValid())
	{
		_H0DebugViewUAV.SafeRelease();
	}

	if (H0DebugView != nullptr)
	{
		_H0DebugViewUAV = RHICreateUnorderedAccessView(H0DebugView->GameThread_GetRenderTargetResource()->TextureRHI);
	}

	if (_HtDebugViewUAV.IsValid())
	{
		_HtDebugViewUAV.SafeRelease();
	}

	if (HtDebugView != nullptr)
	{
		_HtDebugViewUAV = RHICreateUnorderedAccessView(HtDebugView->GameThread_GetRenderTargetResource()->TextureRHI);
	}

	if (_DkxDebugViewUAV.IsValid())
	{
		_DkxDebugViewUAV.SafeRelease();
	}

	if (DkxDebugView != nullptr)
	{
		_DkxDebugViewUAV = RHICreateUnorderedAccessView(DkxDebugView->GameThread_GetRenderTargetResource()->TextureRHI);
	}

	if (_DkyDebugViewUAV.IsValid())
	{
		_DkyDebugViewUAV.SafeRelease();
	}

	if (DkyDebugView != nullptr)
	{
		_DkyDebugViewUAV = RHICreateUnorderedAccessView(DkyDebugView->GameThread_GetRenderTargetResource()->TextureRHI);
	}

	if (DxyzDebugView != nullptr)
	{
		_DxyzDebugViewUAV = RHICreateUnorderedAccessView(DxyzDebugView->GameThread_GetRenderTargetResource()->TextureRHI);
	}
}

#define TEST_SIN_WAVE 0

void UOceanGridMeshComponent::SendRenderDynamicData_Concurrent()
{
	//SCOPE_CYCLE_COUNTER(STAT_OceanGridMeshCompUpdate);
	Super::SendRenderDynamicData_Concurrent();

#if TEST_SIN_WAVE > 0
	if (SceneProxy != nullptr && DisplacementMap != nullptr && _DisplacementMapUAV.IsValid())
	{
		ENQUEUE_RENDER_COMMAND(OceanDeformGridMeshCommand)(
			[this](FRHICommandListImmediate& RHICmdList)
			{
				if (SceneProxy != nullptr && DisplacementMap != nullptr && _DisplacementMapUAV.IsValid())
				{
					((const FOceanGridMeshSceneProxy*)SceneProxy)->EnqueTestSinWaveCommand(RHICmdList, this);
				}
			}
		);
	}
#else
	if (SceneProxy != nullptr
		&& DisplacementMap != nullptr
		&& _DisplacementMapSRV.IsValid()
		&& _DisplacementMapUAV.IsValid()
		&& GradientFoldingMap != nullptr
		&& _GradientFoldingMapUAV.IsValid()
		&& _H0DebugViewUAV.IsValid()
		&& _HtDebugViewUAV.IsValid()
		&& _DkxDebugViewUAV.IsValid()
		&& _DkyDebugViewUAV.IsValid()
		&& _DxyzDebugViewUAV.IsValid()
	)
	{
		ENQUEUE_RENDER_COMMAND(OceanDeformGridMeshCommand)(
			[this](FRHICommandListImmediate& RHICmdList)
			{
				if (SceneProxy != nullptr
					&& DisplacementMap != nullptr
					&& _DisplacementMapSRV.IsValid()
					&& _DisplacementMapUAV.IsValid()
					&& GradientFoldingMap != nullptr
					&& _GradientFoldingMapUAV.IsValid()
					&& _H0DebugViewUAV.IsValid()
					&& _HtDebugViewUAV.IsValid()
					&& _DkxDebugViewUAV.IsValid()
					&& _DkyDebugViewUAV.IsValid()
					&& _DxyzDebugViewUAV.IsValid()
					)
				{
					((const FOceanGridMeshSceneProxy*)SceneProxy)->EnqueSimulateOceanCommand(RHICmdList, this);
				}
			}
		);
	}
#endif
}

