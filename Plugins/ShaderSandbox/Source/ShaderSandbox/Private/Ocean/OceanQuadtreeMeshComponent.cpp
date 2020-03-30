#include "Ocean/OceanQuadtreeMeshComponent.h"
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
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollectionInstance.h"

using namespace Quadtree;
using namespace OceanSimulator;

/** almost all is copy of FCustomMeshSceneProxy. */
class FOceanQuadtreeMeshSceneProxy final : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	FOceanQuadtreeMeshSceneProxy(UOceanQuadtreeMeshComponent* Component)
		: FPrimitiveSceneProxy(Component)
		, VertexFactory(GetScene().GetFeatureLevel(), "FOceanQuadtreeMeshSceneProxy")
		, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
		, LODMIDList(Component->GetLODMIDList())
		, MPCInstance(Component->GetMPCInstance())
		, QuadMeshParams(Component->GetQuadMeshParams())
		, NumGridDivision(Component->NumGridDivision)
		, GridLength(Component->GridLength)
		, MaxLOD(Component->MaxLOD)
		, GridMaxPixelCoverage(Component->GridMaxPixelCoverage)
		, PatchLength(Component->PatchLength)
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

		// GetDynamicMeshElements()�̂��ƁAVerifyUsedMaterial()�ɂ���ă}�e���A�����R���|�[�l���g�ɂ��������̂��`�F�b�N�����̂�
		// SetUsedMaterialForVerification()�œo�^���������邪�A�����_�[�X���b�h�o�Ȃ���check�ɂЂ�������̂�
		bVerifyUsedMaterials = false;


		int32 SizeX, SizeY;
		Component->GetDisplacementMap()->GetSize(SizeX, SizeY);
		check(SizeX == SizeY); // �����`�ł���O��
		check(FMath::IsPowerOfTwo(SizeX)); // 2�̗ݏ�̃T�C�Y�ł���O��
		uint32 DispMapDimension = SizeX;
		
		// Phyllips Spectrum���g����������
		// Height map H(0)
		H0Data.Init(FComplex::ZeroVector, DispMapDimension * DispMapDimension);
		// FComplex::ZeroVector�Ƃ������O�������i�D�������AZero�݂����ȐV�����萔����낤�Ǝv����typedef FVector2D FComplex�ł͂ł���FVector2D���܂���FComplex�\���̂����˂΂Ȃ�Ȃ��̂ō��͑Ë�����

		Omega0Data.Init(0.0f, DispMapDimension * DispMapDimension);

		FOceanSpectrumParameters Params;
		Params.DispMapDimension = DispMapDimension;
		Params.PatchLength = PatchLength;
		Params.AmplitudeScale = Component->AmplitudeScale;
		Params.WindDirection = Component->WindDirection.GetSafeNormal();
		Params.WindSpeed = Component->WindSpeed;
		Params.WindDependency = Component->WindDependency;
		Params.ChoppyScale = Component->ChoppyScale;
		Params.AccumulatedTime = Component->GetAccumulatedTime() * Component->TimeScale;
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

	virtual ~FOceanQuadtreeMeshSceneProxy()
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
		QUICK_SCOPE_CYCLE_COUNTER( STAT_OceanQuadtreeMeshSceneProxy_GetDynamicMeshElements );

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

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];

				// RootNode�̕ӂ̒�����PatchLength��2��MaxLOD�悵���T�C�Y
				FQuadNode RootNode;
				RootNode.Length = PatchLength * (1 << MaxLOD);
				RootNode.BottomRight = GetLocalToWorld().GetOrigin() + FVector(-RootNode.Length * 0.5f, -RootNode.Length * 0.5f, 0.0f);
				RootNode.LOD = MaxLOD;

				// Area()�Ƃ����֐������邪�A�傫�Ȑ��Ŋ����Đ��x�𗎂Ƃ��Ȃ��悤��2�i�K�Ŋ���
				float MaxScreenCoverage = (float)GridMaxPixelCoverage * GridMaxPixelCoverage / View->UnscaledViewRect.Width() / View->UnscaledViewRect.Height();

				TArray<FQuadNode> QuadNodeList;
				QuadNodeList.Reserve(512); //TODO:�Ƃ肠����512�B�����Ŗ���m�ۂ͏����s�����ʂ����A���̊֐���const�Ȃ̂łƂ肠����
				TArray<FQuadNode> RenderList;
				RenderList.Reserve(512); //TODO:�Ƃ肠����512�B�����Ŗ���m�ۂ͏����s�����ʂ����A���̊֐���const�Ȃ̂łƂ肠����

				Quadtree::BuildQuadtree(MaxLOD, NumGridDivision, MaxScreenCoverage, PatchLength, View->ViewMatrices.GetViewOrigin(), View->ViewMatrices.GetProjectionScale(), View->ViewMatrices.GetViewProjectionMatrix(), RootNode, QuadNodeList, RenderList);

				for (const FQuadNode& Node : RenderList)
				{
					if(!bWireframe)
					{
						MaterialProxy = LODMIDList[Node.LOD]->GetRenderProxy();
					}

					// �אڂ���m�[�h�Ƃ�LOD�̍�����A�g�p���郁�b�V���g�|���W�[��p�ӂ������̂̂Ȃ�����I������
					const FVector2D& RightAdjPos = FVector2D(Node.BottomRight.X - PatchLength * 0.5f, Node.BottomRight.Y + Node.Length * 0.5f);
					const FVector2D& LeftAdjPos = FVector2D(Node.BottomRight.X + Node.Length + PatchLength * 0.5f, Node.BottomRight.Y + Node.Length * 0.5f);
					const FVector2D& BottomAdjPos = FVector2D(Node.BottomRight.X + Node.Length * 0.5f, Node.BottomRight.Y - PatchLength * 0.5f);
					const FVector2D& TopAdjPos = FVector2D(Node.BottomRight.X + Node.Length * 0.5f, Node.BottomRight.Y + Node.Length + PatchLength * 0.5f);

					EAdjacentQuadNodeLODDifference RightAdjLODDiff = Quadtree::QueryAdjacentNodeType(Node, RightAdjPos, RenderList);
					EAdjacentQuadNodeLODDifference LeftAdjLODDiff = Quadtree::QueryAdjacentNodeType(Node, LeftAdjPos, RenderList);
					EAdjacentQuadNodeLODDifference BottomAdjLODDiff = Quadtree::QueryAdjacentNodeType(Node, BottomAdjPos, RenderList);
					EAdjacentQuadNodeLODDifference TopAdjLODDiff = Quadtree::QueryAdjacentNodeType(Node, TopAdjPos, RenderList);

					// 3�i���ɂ����4�̗׃m�[�h�̃^�C�v�ƃC���f�b�N�X��Ή�������
					uint32 QuadMeshParamsIndex = 27 * (uint32)RightAdjLODDiff + 9 * (uint32)LeftAdjLODDiff + 3 * (uint32)BottomAdjLODDiff + (uint32)TopAdjLODDiff;
					const FQuadMeshParameter& MeshParams = QuadMeshParams[QuadMeshParamsIndex];

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

					// ���b�V���T�C�Y��QuadNode�̃T�C�Y�ɉ����ăX�P�[��������
					float MeshScale = Node.Length / (NumGridDivision * GridLength);

					FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
					// FPrimitiveSceneProxy::ApplyLateUpdateTransform()���Q�l�ɂ���
					const FMatrix& NewLocalToWorld = GetLocalToWorld().ApplyScale(MeshScale).ConcatTranslation(FVector(Node.BottomRight.X, Node.BottomRight.Y, 0.0f));
					PreviousLocalToWorld = PreviousLocalToWorld.ApplyScale(MeshScale).ConcatTranslation(FVector(Node.BottomRight.X, Node.BottomRight.Y, 0.0f));

					// �R���|�[�l���g��CalcBounds()��RootNode�̃T�C�Y�ɍ��킹�Ď�������Ă���̂ŃX�P�[��������B
					// ���ۂ͕`�悷�郁�b�V����Quadtree�����ۂ̓Ǝ��t���X�^���J�����O�őI�����Ă���̂�Bounds�͂��ׂ�RootNode�T�C�Y�ł�
					// �\��Ȃ����A�ꉞ���m�ȃT�C�Y�ɂ��Ă���
					float BoundScale = Node.Length / RootNode.Length;

					FBoxSphereBounds NewBounds = GetBounds();
					NewBounds = FBoxSphereBounds(NewBounds.Origin + NewLocalToWorld.TransformPosition(FVector(Node.BottomRight.X, Node.BottomRight.Y, 0.0f)), NewBounds.BoxExtent * BoundScale, NewBounds.SphereRadius * BoundScale);

					FBoxSphereBounds NewLocalBounds = GetLocalBounds();
					NewLocalBounds = FBoxSphereBounds(NewLocalBounds.Origin, NewLocalBounds.BoxExtent * BoundScale, NewLocalBounds.SphereRadius * BoundScale);

					DynamicPrimitiveUniformBuffer.Set(NewLocalToWorld, PreviousLocalToWorld, NewBounds, NewLocalBounds, true, bHasPrecomputedVolumetricLightmap, DrawsVelocity(), false);
					BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

					BatchElement.FirstIndex = MeshParams.IndexBufferOffset;
					BatchElement.NumPrimitives = MeshParams.NumIndices / 3;
					BatchElement.MinVertexIndex = 0;
					BatchElement.MaxVertexIndex = VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;
					Mesh.ReverseCulling = (NewLocalToWorld.Determinant() < 0.0f);
					Mesh.Type = PT_TriangleList;
					Mesh.DepthPriorityGroup = SDPG_World;
					Mesh.bCanApplyViewModeOverrides = false;
					Collector.AddMesh(ViewIndex, Mesh);
				}
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

	void EnqueSimulateOceanCommand(FRHICommandListImmediate& RHICmdList, UOceanQuadtreeMeshComponent* Component) const
	{
		FTextureRenderTargetResource* TextureRenderTargetResource = Component->GetDisplacementMap()->GetRenderTargetResource();
		if (TextureRenderTargetResource == nullptr)
		{
			return;
		}

		FOceanSpectrumParameters Params;
		Params.DispMapDimension = TextureRenderTargetResource->GetSizeX(); // TODO:�����`�O���SizeY�͌��ĂȂ�
		Params.PatchLength = PatchLength;
		Params.AmplitudeScale = Component->AmplitudeScale;
		Params.WindDirection = Component->WindDirection.GetSafeNormal();
		Params.WindSpeed = Component->WindSpeed;
		Params.WindDependency = Component->WindDependency;
		Params.ChoppyScale = Component->ChoppyScale;
		Params.AccumulatedTime = Component->GetAccumulatedTime() * Component->TimeScale;
		Params.DxyzDebugAmplitude = Component->DxyzDebugAmplitude;

		// �����_�[�X���b�h���ł��΃R�}���h�L���[�ɕʂ̃R�}���h�𔭍s�����ɑ����s���Ė��ʂ��Ȃ��̂ł����ł��
		const FVector2D& PerlinUVOffset = -Params.WindDirection * Params.AccumulatedTime * Component->PerlinUVSpeed; // ���̕����Ƌt�����ɂ��Ă���
		if (MPCInstance != nullptr)
		{
			MPCInstance->SetVectorParameterValue(FName("PerlinUVOffset"), FVector(PerlinUVOffset.X, PerlinUVOffset.Y, 0.0f));
		}

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
	FMaterialRelevance MaterialRelevance;

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

	TArray<UMaterialInstanceDynamic*> LODMIDList; // Component����UMaterialInstanceDynamic�͕ێ�����Ă�̂�GC�ŉ���͂���Ȃ�
	UMaterialParameterCollectionInstance* MPCInstance = nullptr; // Component����UMaterialInstanceDynamic�͕ێ�����Ă�̂�GC�ŉ���͂���Ȃ�
	TArray<Quadtree::FQuadMeshParameter> QuadMeshParams;
	int32 NumGridDivision;
	float GridLength;
	int32 MaxLOD;
	int32 GridMaxPixelCoverage;
	int32 PatchLength;
};

//////////////////////////////////////////////////////////////////////////

UOceanQuadtreeMeshComponent::UOceanQuadtreeMeshComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

UOceanQuadtreeMeshComponent::~UOceanQuadtreeMeshComponent()
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

FPrimitiveSceneProxy* UOceanQuadtreeMeshComponent::CreateSceneProxy()
{
	FPrimitiveSceneProxy* Proxy = NULL;
	if(_Vertices.Num() > 0 && _Indices.Num() > 0 && DisplacementMap != nullptr && GradientFoldingMap != nullptr)
	{
		Proxy = new FOceanQuadtreeMeshSceneProxy(this);
	}
	return Proxy;
}

void UOceanQuadtreeMeshComponent::OnRegister()
{
	Super::OnRegister();

	// �O���b�h���b�V���^��VertexBuffer��TexCoordsBuffer��p�ӂ���̂�UDeformableGridMeshComponent::Ini:tGridMeshSetting()�Ɠ��������A
	// �ڂ���QuadNode��LOD�̍����l�����Đ��p�^�[���̃C���f�b�N�X�z���p�ӂ��˂΂Ȃ�Ȃ��̂œƎ��̎���������
	_NumRow = NumGridDivision;
	_NumColumn = NumGridDivision;
	_GridWidth = GridLength;
	_GridHeight = GridLength;
	_Vertices.Reset((NumGridDivision + 1) * (NumGridDivision + 1));
	_TexCoords.Reset((NumGridDivision + 1) * (NumGridDivision + 1));

	// �����ł͐����`�̒��S�����_�ɂ��镽�s�ړ���LOD�ɉ������X�P�[���͂��Ȃ��B���ۂɃ��b�V����`��ɓn���Ƃ��ɕ��s�ړ��ƃX�P�[�����s���B

	for (int32 y = 0; y < NumGridDivision + 1; y++)
	{
		for (int32 x = 0; x < NumGridDivision + 1; x++)
		{
			_Vertices.Emplace(x * GridLength, y * GridLength, 0.0f, 0.0f);
		}
	}

	for (int32 y = 0; y < NumGridDivision + 1; y++)
	{
		for (int32 x = 0; x < NumGridDivision + 1; x++)
		{
			_TexCoords.Emplace((float)x / NumGridDivision, (float)y / NumGridDivision);
		}
	}

	// QuadNode�̋��E�����̘A���I�ȕω��̂��߁A�����̃O���b�h�����łȂ��ƓK�؂ȃW�I���g���ɂł��Ȃ�
	if (NumGridDivision % 2 == 1)
	{
		UE_LOG(LogTemp, Error, TEXT("NumGridDivision must be an even number."));
		return;
	}

	Quadtree::CreateQuadMeshes(NumGridDivision, _Indices, QuadMeshParams);

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

	_MPCInstance = nullptr;
	if (OceanMPC != nullptr)
	{
		_MPCInstance = GetWorld()->GetParameterCollectionInstance(OceanMPC);
		_MPCInstance->SetVectorParameterValue(FName("PerlinDisplacement"), PerlinDisplacement);
		_MPCInstance->SetVectorParameterValue(FName("PerlinGradient"), PerlinGradient);
		// UV�X�P�[���������̋t���ɂȂ�Ȃ��ƁA���[�v�\����perin�m�C�Y���g���Ă���ȏ�A���E�����ł��ꂪ�N����B�����UPROPERTY�ł͋t����FIntVector�ň����Ă���B
		// PerlinUVScale��LOD��UV�X�P�[���Ō��߂�ꂽ�p�b�`�̒��ł���ɃX�P�[����������̂ł���B
		_MPCInstance->SetVectorParameterValue(FName("PerlinUVScale"), FVector(1.0f / PerlinUVInvScale.X, 1.0f / PerlinUVInvScale.Y, 1.0f / PerlinUVInvScale.Z));
	}

	UMaterialInterface* Material = GetMaterial(0);
	if(Material == NULL)
	{
		Material = UMaterial::GetDefaultMaterial(MD_Surface);
	}

	// QuadNode�̐��́AMaxLOD-2���ŏ����x���Ȃ̂ł��ׂčŏ���QuadNode�ŕ~���l�߂��
	// 2^(MaxLOD-2)*2^(MaxLOD-2)
	// �ʏ�͂����܂ł����Ȃ��B�J�������牓���Ȃ�ɂ��2�{�ɂȂ��Ă�����
	// 2*6=12�ɂȂ邾�낤�B����͌��_�ɃJ����������Ƃ��ŁA���J�����̍�����0�ɋ߂��Ƃ��Ȃ̂ŁA
	// ����������4�{���x�̐��ƌ��ς����Ăł����͂�

	float InvMaxLOD = 1.0f / MaxLOD;
	//LODMIDList.SetNumZeroed(48);
	//for (int32 i = 0; i < 48; i++)
	_LODMIDList.SetNumZeroed(MaxLOD + 1);
	for (int32 LOD = 0; LOD < MaxLOD + 1; LOD++)
	{
		_LODMIDList[LOD] = UMaterialInstanceDynamic::Create(Material, this);
		_LODMIDList[LOD]->SetScalarParameterValue(FName("UVScale"), (float)(1 << LOD));
		_LODMIDList[LOD]->SetVectorParameterValue(FName("LODColor"), FLinearColor((MaxLOD - LOD) * InvMaxLOD, 0.0f, LOD * InvMaxLOD));
	}

	MarkRenderStateDirty();
	UpdateBounds();
}

FBoxSphereBounds UOceanQuadtreeMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	// Quadtree��RootNode�̃T�C�Y�ɂ��Ă����B�A�N�^��BP�G�f�B�^�̃r���[�|�[�g�\����t�H�[�J�X����Ȃǂł���Bound���g����̂łȂ�ׂ����m�ɂ���
	// �܂��AQuadNode�̊e���b�V����Bound���v�Z�����T�C�Y�Ƃ��Ă��g���B
	float HalfRootNodeLength = PatchLength * (1 << (MaxLOD - 1));
	const FVector& Min = LocalToWorld.TransformPosition(FVector(-HalfRootNodeLength, -HalfRootNodeLength, 0.0f));
	const FVector& Max = LocalToWorld.TransformPosition(FVector(HalfRootNodeLength, HalfRootNodeLength, 0.0f));
	FBox Box(Min, Max);

	const FBoxSphereBounds& Ret = FBoxSphereBounds(Box);
	return Ret;
}

void UOceanQuadtreeMeshComponent::SendRenderDynamicData_Concurrent()
{
	//SCOPE_CYCLE_COUNTER(STAT_OceanQuadtreeMeshCompUpdate);
	Super::SendRenderDynamicData_Concurrent();

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
					((const FOceanQuadtreeMeshSceneProxy*)SceneProxy)->EnqueSimulateOceanCommand(RHICmdList, this);
				}
			}
		);
	}
}

const TArray<class UMaterialInstanceDynamic*>& UOceanQuadtreeMeshComponent::GetLODMIDList() const
{
	return _LODMIDList;
}

UMaterialParameterCollectionInstance* UOceanQuadtreeMeshComponent::GetMPCInstance() const
{
	return _MPCInstance;
}

const TArray<Quadtree::FQuadMeshParameter>& UOceanQuadtreeMeshComponent::GetQuadMeshParams() const
{
	return QuadMeshParams;
}

