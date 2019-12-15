#include "DeformMesh/SphereCollisionComponent.h"

void USphereCollisionComponent::SetRadius(float Radius)
{
	// UE4��/Engine/BasicShapes/Sphere��StaticMesh���g���B���a��100cm�ł���Ƃ����O���u��
	_Radius = Radius;

	SetRelativeScale3D(FVector(2.0f * Radius / 100.0f));
}
