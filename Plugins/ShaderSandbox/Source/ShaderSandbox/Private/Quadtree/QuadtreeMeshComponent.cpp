#include "Quadtree/QuadtreeMeshComponent.h"
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
#include "Materials/MaterialInstanceDynamic.h"
#include "DeformMesh/DeformableVertexBuffers.h"

using namespace Quadtree;

/** almost all is copy of FCustomMeshSceneProxy. */
class FQuadtreeMeshSceneProxy final : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	FQuadtreeMeshSceneProxy(UQuadtreeMeshComponent* Component)
		: FPrimitiveSceneProxy(Component)
		, VertexFactory(GetScene().GetFeatureLevel(), "FQuadtreeMeshSceneProxy")
		, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
		, LODMIDList(Component->GetLODMIDList())
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
	}

	virtual ~FQuadtreeMeshSceneProxy()
	{
		VertexBuffers.PositionVertexBuffer.ReleaseResource();
		VertexBuffers.DeformableMeshVertexBuffer.ReleaseResource();
		VertexBuffers.ColorVertexBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		QUICK_SCOPE_CYCLE_COUNTER( STAT_QuadtreeMeshSceneProxy_GetDynamicMeshElements );

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
		//else
		//{
		//	MaterialProxy = Material->GetRenderProxy();
		//}

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
					FQuadMeshParameter MeshParams = QuadMeshParams[QuadMeshParamsIndex];

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

					//DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, DrawsVelocity(), false);
					DynamicPrimitiveUniformBuffer.Set(NewLocalToWorld, PreviousLocalToWorld, NewBounds, NewLocalBounds, true, bHasPrecomputedVolumetricLightmap, DrawsVelocity(), false);
					BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

					BatchElement.FirstIndex = MeshParams.IndexBufferOffset;
					BatchElement.NumPrimitives = MeshParams.NumIndices / 3;
					BatchElement.MinVertexIndex = 0;
					BatchElement.MaxVertexIndex = VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;
					//Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
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


private:
	UMaterialInterface* Material;
	FDeformableVertexBuffers VertexBuffers;
	FDynamicMeshIndexBuffer32 IndexBuffer;
	FLocalVertexFactory VertexFactory;

	FMaterialRelevance MaterialRelevance;
	TArray<UMaterialInstanceDynamic*> LODMIDList; // Component����UMaterialInstanceDynamic�͕ێ�����Ă�̂�GC�ŉ���͂���Ȃ�
	TArray<Quadtree::FQuadMeshParameter> QuadMeshParams;
	int32 NumGridDivision;
	float GridLength;
	int32 MaxLOD;
	int32 GridMaxPixelCoverage;
	int32 PatchLength;
};

//////////////////////////////////////////////////////////////////////////

UQuadtreeMeshComponent::UQuadtreeMeshComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

FPrimitiveSceneProxy* UQuadtreeMeshComponent::CreateSceneProxy()
{
	FPrimitiveSceneProxy* Proxy = NULL;
	if(_Vertices.Num() > 0 && _Indices.Num() > 0)
	{
		Proxy = new FQuadtreeMeshSceneProxy(this);
	}
	return Proxy;
}

void UQuadtreeMeshComponent::OnRegister()
{
	Super::OnRegister();

	CreateQuadMesh();

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
	LODMIDList.SetNumZeroed(MaxLOD + 1);
	for (int32 LOD = 0; LOD < MaxLOD + 1; LOD++)
	{
		LODMIDList[LOD] = UMaterialInstanceDynamic::Create(Material, this);
		LODMIDList[LOD]->SetVectorParameterValue(FName("Color"), FLinearColor((MaxLOD - LOD) * InvMaxLOD, 0.0f, LOD * InvMaxLOD));
	}
}

void UQuadtreeMeshComponent::CreateQuadMesh()
{
	// �O���b�h���b�V���^��VertexBuffer��TexCoordsBuffer��p�ӂ���̂�UDeformableGridMeshComponent::Ini:tGridMeshSetting()�Ɠ��������A
	// �ڂ���QuadNode��LOD�̍����l�����Đ��p�^�[���̃C���f�b�N�X�z���p�ӂ��˂΂Ȃ�Ȃ��̂œƎ��̎���������
	_NumRow = NumGridDivision;
	_NumColumn = NumGridDivision;
	_GridWidth = GridLength;
	_GridHeight = GridLength;
	_Vertices.Reset((NumGridDivision + 1) * (NumGridDivision + 1));
	_TexCoords.Reset((NumGridDivision + 1) * (NumGridDivision + 1));
	// TODO:���͌v�Z�����狁�܂�H
	//_Indices.Reset(NumRow * NumColumn * 2 * 3); // �ЂƂ̃O���b�h�ɂ�3��Triangle�A6�̒��_�C���f�b�N�X�w�肪����

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

	check((uint32)EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST == 0);
	check((uint32)EAdjacentQuadNodeLODDifference::MAX == 3);

	// �E�ׂ�LOD�������ȉ��Ȃ̂ƁA��i�K��Ȃ̂ƁA��i�K�ȏ�Ȃ̂�3�p�^�[���B���ׁA���ׁA��ׂł�3�p�^�[���ł����̑g�ݍ��킹��3*3*3*3�p�^�[���B
	QuadMeshParams.Reset(81);

	uint32 IndexOffset = 0;
	for (uint32 RightType = (uint32)EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST; RightType < (uint32)EAdjacentQuadNodeLODDifference::MAX; RightType++)
	{
		for (uint32 LeftType = (uint32)EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST; LeftType < (uint32)EAdjacentQuadNodeLODDifference::MAX; LeftType++)
		{
			for (uint32 BottomType = (uint32)EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST; BottomType < (uint32)EAdjacentQuadNodeLODDifference::MAX; BottomType++)
			{
				for (uint32 TopType = (uint32)EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST; TopType < (uint32)EAdjacentQuadNodeLODDifference::MAX; TopType++)
				{
					uint32 NumInnerMeshIndices = CreateInnerMesh();
					uint32 NumBoundaryMeshIndices = CreateBoundaryMesh((EAdjacentQuadNodeLODDifference)RightType, (EAdjacentQuadNodeLODDifference)LeftType, (EAdjacentQuadNodeLODDifference)BottomType, (EAdjacentQuadNodeLODDifference)TopType);
					// TArray�̃C���f�b�N�X�́ARightType * 3^3 + LeftType * 3^2 + BottomType * 3^1 + TopType * 3^0�ƂȂ�
					QuadMeshParams.Emplace(IndexOffset, NumInnerMeshIndices + NumBoundaryMeshIndices);
					IndexOffset += NumInnerMeshIndices + NumBoundaryMeshIndices;
				}
			}
		}
	}

	MarkRenderStateDirty();
	UpdateBounds();
}

uint32 UQuadtreeMeshComponent::CreateInnerMesh()
{
	// �����̕����͂ǂ̃��b�V���p�^�[���ł����������A���ׂē������̂��쐬����B�h���[�R�[���ŃC���f�b�N�X�o�b�t�@�̕s�A���A�N�Z�X�͂ł��Ȃ��̂�
	for (int32 Row = 1; Row < NumGridDivision - 1; Row++)
	{
		for (int32 Column = 1; Column < NumGridDivision - 1; Column++)
		{
			_Indices.Emplace(Row * (NumGridDivision + 1) + Column);
			_Indices.Emplace((Row + 1) * (NumGridDivision + 1) + Column);
			_Indices.Emplace((Row + 1) * (NumGridDivision + 1) + Column + 1);

			_Indices.Emplace(Row * (NumGridDivision + 1) + Column);
			_Indices.Emplace((Row + 1) * (NumGridDivision + 1) + Column + 1);
			_Indices.Emplace(Row * (NumGridDivision + 1) + Column + 1);
		}
	}

	return 6 * (NumGridDivision - 2) * (NumGridDivision - 2);
}

uint32 UQuadtreeMeshComponent::CreateBoundaryMesh(EAdjacentQuadNodeLODDifference RightAdjLODDiff, EAdjacentQuadNodeLODDifference LeftAdjLODDiff, EAdjacentQuadNodeLODDifference BottomAdjLODDiff, EAdjacentQuadNodeLODDifference TopAdjLODDiff)
{
	// Right
	{
		int32 Column = 0;
		for (int32 Row = 0; Row < NumGridDivision; Row++)
		{
			_Indices.Emplace(GetMeshIndex(Row, Column));
			_Indices.Emplace(GetMeshIndex(Row + 1, Column));
			_Indices.Emplace(GetMeshIndex(Row + 1, Column + 1));

			_Indices.Emplace(GetMeshIndex(Row, Column));
			_Indices.Emplace(GetMeshIndex(Row + 1, Column + 1));
			_Indices.Emplace(GetMeshIndex(Row, Column + 1));
		}
	}

	// Left
	{
		int32 Column = NumGridDivision - 1;
		for (int32 Row = 0; Row < NumGridDivision; Row++)
		{
			_Indices.Emplace(GetMeshIndex(Row, Column));
			_Indices.Emplace(GetMeshIndex(Row + 1, Column));
			_Indices.Emplace(GetMeshIndex(Row + 1, Column + 1));

			_Indices.Emplace(GetMeshIndex(Row, Column));
			_Indices.Emplace(GetMeshIndex(Row + 1, Column + 1));
			_Indices.Emplace(GetMeshIndex(Row, Column + 1));
		}
	}

	// Bottom
	{
		int32 Row = 0;
		for (int32 Column = 1; Column < NumGridDivision - 1; Column++) // Right�ALeft�Ƃ��Ԃ�Ȃ��悤�ɍ��E�̈��͔���
		{
			_Indices.Emplace(GetMeshIndex(Row, Column));
			_Indices.Emplace(GetMeshIndex(Row + 1, Column));
			_Indices.Emplace(GetMeshIndex(Row + 1, Column + 1));

			_Indices.Emplace(GetMeshIndex(Row, Column));
			_Indices.Emplace(GetMeshIndex(Row + 1, Column + 1));
			_Indices.Emplace(GetMeshIndex(Row, Column + 1));
		}
	}

	// Top
	{
		int32 Row = NumGridDivision - 1;
		for (int32 Column = 1; Column < NumGridDivision - 1; Column++) // Right�ALeft�Ƃ��Ԃ�Ȃ��悤�ɍ��E�̈��͔���
		{
			_Indices.Emplace(GetMeshIndex(Row, Column));
			_Indices.Emplace(GetMeshIndex(Row + 1, Column));
			_Indices.Emplace(GetMeshIndex(Row + 1, Column + 1));

			_Indices.Emplace(GetMeshIndex(Row, Column));
			_Indices.Emplace(GetMeshIndex(Row + 1, Column + 1));
			_Indices.Emplace(GetMeshIndex(Row, Column + 1));
		}
	}

	return 6 * NumGridDivision * 2 + 6 * (NumGridDivision - 2) * 2;
}

int32 UQuadtreeMeshComponent::GetMeshIndex(int32 Row, int32 Column)
{
	return Row * (NumGridDivision + 1) + Column;
}

FBoxSphereBounds UQuadtreeMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
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

const TArray<class UMaterialInstanceDynamic*>& UQuadtreeMeshComponent::GetLODMIDList() const
{
	return LODMIDList;
}

const TArray<Quadtree::FQuadMeshParameter>& UQuadtreeMeshComponent::GetQuadMeshParams() const
{
	return QuadMeshParams;
}

