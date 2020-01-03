#include "DeformMesh/ClothManager.h"
#include "PrimitiveSceneProxy.h"
#include "DeformMesh/ClothGridMeshComponent.h"
#include "DeformMesh/SphereCollisionComponent.h"

static inline void InitOrUpdateResourceMacroClothManager(FRenderResource* Resource)
{
	if (!Resource->IsInitialized())
	{
		Resource->InitResource();
	}
	else
	{
		Resource->UpdateRHI();
	}
}

class FClothManagerSceneProxy final : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	FClothManagerSceneProxy(UClothManagerComponent* Component)
		: FPrimitiveSceneProxy(Component)
	{
	}

	virtual ~FClothManagerSceneProxy()
	{
		WorkAccelerationVertexBuffer.ReleaseResource();
		WorkPrevPositionVertexBuffer.ReleaseResource();
		WorkPositionVertexBuffer.ReleaseResource();
	}

	virtual uint32 GetMemoryFootprint( void ) const override { return( sizeof( *this ) + GetAllocatedSize() ); }

	uint32 GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

	void RegisterClothMesh(UClothGridMeshComponent* ClothMesh)
	{
		FMergeData MergeData;
		MergeData.NumVertex = ClothMesh->GetVertices().Num();

		uint32 Offset = 0;
		for (const TPair<UClothGridMeshComponent*, FMergeData>& ClothMeshData : ClothMeshDataMap)
		{
			Offset += ClothMeshData.Value.NumVertex;
		}

		MergeData.Offset = Offset;

		check(ClothMeshDataMap.Find(ClothMesh) == nullptr);
		ClothMeshDataMap.Add(ClothMesh, MergeData);

		// TODO:�N���X�̉񐔂���Init����UpdateRHI���Ă�̂͂��������Ȃ�
		// Init�͍ē��\�B����Realloc���Ă���
		WorkAccelerationVertexBuffer.Init(MergeData.Offset + MergeData.NumVertex);
		WorkPrevPositionVertexBuffer.Init(MergeData.Offset + MergeData.NumVertex);
		WorkPositionVertexBuffer.Init(MergeData.Offset + MergeData.NumVertex);

		InitOrUpdateResourceMacroClothManager(&WorkAccelerationVertexBuffer);
		InitOrUpdateResourceMacroClothManager(&WorkPrevPositionVertexBuffer);
		InitOrUpdateResourceMacroClothManager(&WorkPositionVertexBuffer);
	}

	void UnregisterClothMesh(UClothGridMeshComponent* ClothMesh)
	{
		ClothMeshDataMap.Remove(ClothMesh);
		// TODO:Remove���Ă��A���̂Ƃ���A�������o�[�e�b�N�X�o�b�t�@�͏k�߂Ȃ��B
	}

	// TODO:1���b�V���ɂ܂Ƃ߂�O�̏������ێ����邽�߉��ɍ����
	void EnqueueSimulateClothCommand(FRHICommandListImmediate& RHICmdList, const FClothGridMeshDeformCommand& Command)
	{
		NumCommand++;
		VertexDeformer.EnqueueDeformCommand(Command);

		if ((int32)NumCommand >= ClothMeshDataMap.Num())
		{
			VertexDeformer.FlushDeformCommandQueue(RHICmdList, WorkAccelerationVertexBuffer.GetUAV(), WorkPrevPositionVertexBuffer.GetUAV(), WorkPositionVertexBuffer.GetUAV());
			NumCommand = 0;
		}
	}

private:
	// Data for 
	struct FMergeData
	{
		int32 Offset;
		int32 NumVertex;
	};

	int32 NumCommand = 0;
	TMap<class UClothGridMeshComponent*, FMergeData> ClothMeshDataMap;
	FClothGridMeshDeformer VertexDeformer;
	// vertex buffer to simulate all clothes by one dispatch by merging all vertex buffers.
	// TODO:UAV�����ł悭�ASRV������̂�����
	FDeformablePositionVertexBuffer WorkAccelerationVertexBuffer;
	FDeformablePositionVertexBuffer WorkPrevPositionVertexBuffer;
	FDeformablePositionVertexBuffer WorkPositionVertexBuffer;
};

void UClothManagerComponent::RegisterClothMesh(UClothGridMeshComponent* ClothMesh)
{
	if (SceneProxy != nullptr)
	{
		FClothManagerSceneProxy* ClothManagerSceneProxy = (FClothManagerSceneProxy*)SceneProxy;
		ENQUEUE_RENDER_COMMAND(RegisterClothMesh)(
			[ClothManagerSceneProxy, ClothMesh](FRHICommandListImmediate& RHICmdList)
			{
				ClothManagerSceneProxy->RegisterClothMesh(ClothMesh);
			});
	}
}

void UClothManagerComponent::UnregisterClothMesh(UClothGridMeshComponent* ClothMesh)
{
	if (SceneProxy != nullptr)
	{
		FClothManagerSceneProxy* ClothManagerSceneProxy = (FClothManagerSceneProxy*)SceneProxy;
		ENQUEUE_RENDER_COMMAND(UnregisterClothMesh)(
			[ClothManagerSceneProxy, ClothMesh](FRHICommandListImmediate& RHICmdList)
			{
				ClothManagerSceneProxy->UnregisterClothMesh(ClothMesh);
			});
	}
}

// TODO:1���b�V���ɂ܂Ƃ߂�O�̏������ێ����邽�߉��ɍ����
void UClothManagerComponent::EnqueueSimulateClothCommand(const FClothGridMeshDeformCommand& Command)
{
	if (SceneProxy != nullptr)
	{
		FClothManagerSceneProxy* ClothManagerSceneProxy = (FClothManagerSceneProxy*)SceneProxy;
		ENQUEUE_RENDER_COMMAND(UnregisterClothMesh)(
			[ClothManagerSceneProxy, Command](FRHICommandListImmediate& RHICmdList)
			{
				ClothManagerSceneProxy->EnqueueSimulateClothCommand(RHICmdList, Command);
			});
	}
}

FPrimitiveSceneProxy* UClothManagerComponent::CreateSceneProxy()
{
	return new FClothManagerSceneProxy(this);
}

AClothManager::AClothManager()
{
	gClothManager = this;
	RootComponent = CreateDefaultSubobject<UClothManagerComponent>(TEXT("ClothManagerComponent"));
}

AClothManager::~AClothManager()
{
	gClothManager = nullptr;
}

AClothManager* AClothManager::GetInstance()
{
	return gClothManager;
}

void AClothManager::RegisterClothMesh(UClothGridMeshComponent* ClothMesh)
{
	Cast<UClothManagerComponent>(RootComponent)->RegisterClothMesh(ClothMesh);
}

void AClothManager::UnregisterClothMesh(UClothGridMeshComponent* ClothMesh)
{
	Cast<UClothManagerComponent>(RootComponent)->UnregisterClothMesh(ClothMesh);
}

const TArray<USphereCollisionComponent*>& AClothManager::GetSphereCollisions() const
{
	return SphereCollisions;
}

void AClothManager::RegisterSphereCollision(USphereCollisionComponent* SphereCollision)
{
	SphereCollisions.Add(SphereCollision);
}

void AClothManager::UnregisterSphereCollision(USphereCollisionComponent* SphereCollision)
{
	SphereCollisions.Remove(SphereCollision);
}

// TODO:1���b�V���ɂ܂Ƃ߂�O�̏������ێ����邽�߉��ɍ����
void AClothManager::EnqueueSimulateClothCommand(const FClothGridMeshDeformCommand& Command)
{
	Cast<UClothManagerComponent>(RootComponent)->EnqueueSimulateClothCommand(Command);
}

