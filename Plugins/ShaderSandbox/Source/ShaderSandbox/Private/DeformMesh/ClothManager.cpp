#include "DeformMesh/ClothManager.h"
#include "DeformMesh/SphereCollisionComponent.h"

AClothManager::AClothManager()
{
	gClothManager = this;
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
	ClothMeshes.Add(ClothMesh);
}

void AClothManager::UnregisterClothMesh(UClothGridMeshComponent* ClothMesh)
{
	ClothMeshes.Remove(ClothMesh);
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

void AClothManager::EnqueueSimulateClothTask(const FGridClothParameters& Task)
{
	SimulateClothTaskQueue.Add(Task);

	if (SimulateClothTaskQueue.Num() >= ClothMeshes.Num())
	{
		// ENQUEUE_RENDER_COMMAND�̒��̃����_�[�R�}���h�L���[�̏����͂����s����邩�킩��Ȃ��̂ł��̎��_�ł̃N���X�^�X�N�L���[���g���Ȃ�R�s�[���K�v
		TArray<FGridClothParameters> CopiedQueue = SimulateClothTaskQueue;
		SimulateClothTaskQueue.Reset();

		ENQUEUE_RENDER_COMMAND(SimulateGridMeshClothes)(
			[CopiedQueue](FRHICommandListImmediate& RHICmdList)
			{
				SimulateGridMeshClothes(RHICmdList, CopiedQueue);
			}
		);
	}
}

