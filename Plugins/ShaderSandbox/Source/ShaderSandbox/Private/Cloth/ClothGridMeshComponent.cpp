#include "Cloth/ClothGridMeshComponent.h"
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
#include "Cloth/ClothVertexBuffers.h"
#include "Cloth/ClothGridMeshDeformer.h"
#include "Cloth/ClothManager.h"
#include "Cloth/SphereCollisionComponent.h"

//TODO:FDeformGridMeshSceneProxy�ƃ\�[�X�R�[�h�̋��ʉ����ł��Ȃ���

/** almost all is copy of FCustomMeshSceneProxy. */
class FClothGridMeshSceneProxy final : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	FClothGridMeshSceneProxy(UClothGridMeshComponent* Component)
		: FPrimitiveSceneProxy(Component)
		, VertexFactory(GetScene().GetFeatureLevel(), "FClothGridMeshSceneProxy")
		, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
	{
		TArray<FDynamicMeshVertex> Vertices;
		TArray<float> InvMasses;
		Vertices.Reset(Component->GetVertices().Num());
		InvMasses.Reset(Component->GetVertices().Num());
		IndexBuffer.Indices = Component->GetIndices();

		for (int32 VertIdx = 0; VertIdx < Component->GetVertices().Num(); VertIdx++)
		{
			// TODO:Color��Tangent�͂Ƃ肠����FDynamicMeshVertex�̃f�t�H���g�l�܂����ɂ���
			Vertices.Emplace(Component->GetVertices()[VertIdx]);
			InvMasses.Emplace(Component->GetVertices()[VertIdx].W);
		}
		VertexBuffers.InitFromClothVertexAttributes(&VertexFactory, Vertices, InvMasses, Component->GetAccelerationMoves());

		// Enqueue initialization of render resource
		BeginInitResource(&VertexBuffers.PositionVertexBuffer);
		BeginInitResource(&VertexBuffers.DeformableMeshVertexBuffer);
		BeginInitResource(&VertexBuffers.ColorVertexBuffer);
		BeginInitResource(&VertexBuffers.PrevPositionVertexBuffer);
		BeginInitResource(&VertexBuffers.AccelerationMoveVertexBuffer);
		BeginInitResource(&IndexBuffer);
		BeginInitResource(&VertexFactory);

		// Grab material
		Material = Component->GetMaterial(0);
		if(Material == NULL)
		{
			Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}
	}

	virtual ~FClothGridMeshSceneProxy()
	{
		VertexBuffers.PositionVertexBuffer.ReleaseResource();
		VertexBuffers.DeformableMeshVertexBuffer.ReleaseResource();
		VertexBuffers.ColorVertexBuffer.ReleaseResource();
		VertexBuffers.PrevPositionVertexBuffer.ReleaseResource();
		VertexBuffers.AccelerationMoveVertexBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		QUICK_SCOPE_CYCLE_COUNTER( STAT_ClothGridMeshSceneProxy_GetDynamicMeshElements );

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

	void EnqueClothGridMeshRenderCommand(UClothGridMeshComponent* Component, FClothGridMeshDeformCommand& Command)
	{
		VertexBuffers.SetAccelerationMoves(Component->GetAccelerationMoves());
		Command.VertexBuffers = &VertexBuffers;
		AClothManager::GetInstance()->EnqueueSimulateClothCommand(Command);
	}

private:

	UMaterialInterface* Material;
	FClothVertexBuffers VertexBuffers;
	FDynamicMeshIndexBuffer32 IndexBuffer;
	FLocalVertexFactory VertexFactory;

	FMaterialRelevance MaterialRelevance;
};

//////////////////////////////////////////////////////////////////////////
UClothGridMeshComponent::~UClothGridMeshComponent()
{
	AClothManager* Manager = AClothManager::GetInstance();
	if (Manager != nullptr)
	{
		Manager->UnregisterClothMesh(this);
	}
}

void UClothGridMeshComponent::InitClothSettings(int32 NumRow, int32 NumColumn, float GridWidth, float GridHeight, float Stiffness, float Damping, float LinearDrag, float FluidDensity, float LiftCoefficient, float DragCoefficient, float VertexRadius, int32 NumIteration)
{
	// �����̕ϐ���log(x)�ŕێ����Ă���̂́A�̂��̂��σt���[�����[�g�Ή���y�悷��̂ŁA�ݏ�v�Z�������
	// exp(log(x)*y)��������������x���p�t�H�[�}���X����������
	// log(1-x)�ŕێ����Ă���̂́A[0,1)�͈̔͂̕ϐ���0�t�߂̐��x���d�����Ă���̂�
	_NumRow = NumRow;
	_NumColumn = NumColumn;
	_GridWidth = GridWidth;
	_GridHeight = GridHeight;
	_Vertices.Reset((NumRow + 1) * (NumColumn + 1));
	_Indices.Reset(NumRow * NumColumn * 2 * 3); // �ЂƂ̃O���b�h�ɂ�3��Triangle�A6�̒��_�C���f�b�N�X�w�肪����
	_AccelerationMoves.Reset((NumRow + 1) * (NumColumn + 1));
	_LogStiffness = FMath::Loge(1.0f - Stiffness);
	_LogDamping = FMath::Loge(1.0f - Damping);
	_LinearLogDrag = FMath::Loge(1.0f - LinearDrag);
	_FluidDensity = FluidDensity;
	_LiftLogCoefficient = FMath::Loge(1.0f - LiftCoefficient);
	_DragLogCoefficient = FMath::Loge(1.0f - DragCoefficient);
	_VertexRadius = VertexRadius;
	_NumIteration = NumIteration;

	_PrevLocation = GetComponentLocation();
	_CurLinearVelocity = FVector::ZeroVector;
	_PrevLinearVelocity = FVector::ZeroVector;

	//TODO:�Ƃ肠����y=0�̈�s�ڂ̂�InvMass=0��
	for (int32 x = 0; x < NumColumn + 1; x++)
	{
		_Vertices.Emplace(x * GridWidth, 0.0f, 0.0f, 0.0f);
	}

	//TODO:�Ƃ肠����y>0�̓�s�ڈȍ~��InvMass=1��
	for (int32 y = 1; y < NumRow + 1; y++)
	{
		for (int32 x = 0; x < NumColumn + 1; x++)
		{
			_Vertices.Emplace(x * GridWidth, y * GridHeight, 0.0f, 1.0f);
		}
	}

	float IterDeltaTime = 1.0f / FGridClothParameters::BASE_FREQUENCY;
	float SqrIterDeltaTime = IterDeltaTime * IterDeltaTime;

	for (int32 y = 0; y < NumRow + 1; y++)
	{
		for (int32 x = 0; x < NumColumn + 1; x++)
		{
			_AccelerationMoves.Emplace(FGridClothParameters::GRAVITY * SqrIterDeltaTime);
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

	AClothManager* Manager = AClothManager::GetInstance();
	if (Manager == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("UClothGridMeshComponent::OnRegister() There is no AClothManager. So failed to register this cloth mesh."));
	}
	else
	{
		Manager->RegisterClothMesh(this);
	}
}

void UClothGridMeshComponent::IgnoreVelocityDiscontinuityNextFrame()
{
	_IgnoreVelocityDiscontinuityNextFrame = true;
}

FPrimitiveSceneProxy* UClothGridMeshComponent::CreateSceneProxy()
{
	FPrimitiveSceneProxy* Proxy = NULL;
	if(_Vertices.Num() > 0 && _Indices.Num() > 0)
	{
		Proxy = new FClothGridMeshSceneProxy(this);
	}
	return Proxy;
}

void UClothGridMeshComponent::SendRenderDynamicData_Concurrent()
{
	//SCOPE_CYCLE_COUNTER(STAT_ClothGridMeshCompUpdate);
	Super::SendRenderDynamicData_Concurrent();

	AClothManager* ClothManager = AClothManager::GetInstance();

	if (SceneProxy != nullptr && ClothManager != nullptr) // ClothManager�͑���retrun�̂��߂Ɏ擾���Ă��邾��
	{
		FClothGridMeshDeformCommand Command;
		MakeDeformCommand(Command);

		((FClothGridMeshSceneProxy*)SceneProxy)->EnqueClothGridMeshRenderCommand(this, Command);
	}
}

void UClothGridMeshComponent::MakeDeformCommand(FClothGridMeshDeformCommand& Command)
{
	AClothManager* ClothManager = AClothManager::GetInstance();

	_PrevLinearVelocity = _CurLinearVelocity;
	const FVector& CurLocation = GetComponentLocation();

	if (!_IgnoreVelocityDiscontinuityNextFrame)
	{
		_CurLinearVelocity = (CurLocation - _PrevLocation) / GetDeltaTime();
	}

	_PrevLocation = CurLocation;

	_IgnoreVelocityDiscontinuityNextFrame = false;

	float IterDeltaTime = GetDeltaTime() / _NumIteration;
	float SqrIterDeltaTime = IterDeltaTime * IterDeltaTime;
	float DampStiffnessExp = FGridClothParameters::BASE_FREQUENCY * IterDeltaTime;

	float LinearAlpha = 0.5f * (_NumIteration + 1) / _NumIteration;

	const FVector& LinearVelocityDiff = _CurLinearVelocity - _PrevLinearVelocity;
	const FVector& CurInertia = -LinearVelocityDiff * LinearAlpha / GetDeltaTime(); // �񊵐��n�̃N���X�̍��W�ł̓��[���h���W�ł̉����x�Ƃ͋t�����̉����x��������
	_PreviousInertia = -LinearVelocityDiff * (1.0f - LinearAlpha) / GetDeltaTime() * SqrIterDeltaTime;

	const FVector& Translation = _CurLinearVelocity * IterDeltaTime;
	const FVector& LinearDrag = Translation * (1.0f - FMath::Exp(_LinearLogDrag * DampStiffnessExp));

	// TODO:_AccelerationMoves��ݒ肵�Ă�̂͊֐����ɍ����ĂȂ�
	for (uint32 i = 0; i < (_NumRow + 1) * (_NumColumn + 1); i++)
	{
		_AccelerationMoves[i] = (CurInertia + FGridClothParameters::GRAVITY) * SqrIterDeltaTime - LinearDrag;
	}

	// �N���X���W�n�Ŏ󂯂镗���x�B���t���[���A�O���[�o���ȕ��͂ɂ̓����_���Ȃ�炬����Z����
	const FVector& WindVeclocity = ClothManager->WindVelocity* FMath::FRandRange(0.0f, 2.0f) - _CurLinearVelocity;

	Command.Params.NumIteration = _NumIteration;
	Command.Params.NumRow = _NumRow;
	Command.Params.NumColumn = _NumColumn;
	Command.Params.NumVertex = GetVertices().Num();
	Command.Params.GridWidth = _GridWidth;
	Command.Params.GridHeight = _GridHeight;
	Command.Params.Stiffness = (1.0f - FMath::Exp(_LogStiffness * DampStiffnessExp));
	Command.Params.Damping = (1.0f - FMath::Exp(_LogDamping * DampStiffnessExp));
	Command.Params.PreviousInertia = _PreviousInertia;
	Command.Params.WindVelocity = WindVeclocity;
	//���n�̃p�����[�^�̓V�F�[�_�̌v�Z��MKS�P�ʌn��Ȃ̂ł���ɓ����FluidDensity�͂��������������˂΂Ȃ炸���[�U�����͂��ɂ����̂ŁAMKS�P�ʌn�œ��ꂳ���Ă����Ă����ŃX�P�[������
	Command.Params.FluidDensity = _FluidDensity / (100.0f * 100.0f * 100.0f);
	Command.Params.LiftCoefficient = (1.0f - FMath::Exp(_LiftLogCoefficient * DampStiffnessExp)) / 100.0f;
	Command.Params.DragCoefficient = (1.0f - FMath::Exp(_DragLogCoefficient * DampStiffnessExp)) / 100.0f;
	Command.Params.IterDeltaTime = IterDeltaTime;
	Command.Params.VertexRadius = _VertexRadius;

	TArray<USphereCollisionComponent*> SphereCollisions = ClothManager->GetSphereCollisions();
	Command.Params.NumSphereCollision = SphereCollisions.Num();

	for (uint32 i = 0; i < Command.Params.NumSphereCollision; i++)
	{
		Command.Params.SphereCollisionParams[i] = FVector4(
			GetComponentTransform().InverseTransformPosition(SphereCollisions[i]->GetComponentLocation()),
			SphereCollisions[i]->GetRadius()
		);
	}
}
