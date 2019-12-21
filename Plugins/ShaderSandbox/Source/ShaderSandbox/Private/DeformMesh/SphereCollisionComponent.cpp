#include "DeformMesh/SphereCollisionComponent.h"
#include "DeformMesh/ClothManager.h"

USphereCollisionComponent::~USphereCollisionComponent()
{
	AClothManager* Manager = AClothManager::GetInstance();
	if (Manager != nullptr)
	{
		Manager->UnregisterSphereCollision(this);
	}
}

void USphereCollisionComponent::SetRadius(float Radius)
{
	// UE4��/Engine/BasicShapes/Sphere��StaticMesh���g���B���a��100cm�ł���Ƃ����O���u��
	_Radius = Radius;

	SetRelativeScale3D(FVector(2.0f * Radius / 100.0f));
}

void USphereCollisionComponent::OnRegister()
{
	Super::OnRegister();

	AClothManager* Manager = AClothManager::GetInstance();
	if (Manager == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("USphereCollisionComponent::OnRegister() There is no AClothManager. So failed to register this collision."));
	}
	else
	{
		Manager->RegisterSphereCollision(this);
	}
}
