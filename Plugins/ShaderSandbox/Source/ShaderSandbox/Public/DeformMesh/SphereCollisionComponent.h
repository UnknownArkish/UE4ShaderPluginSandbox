#pragma once
#include "Components/StaticMeshComponent.h"
#include "SphereCollisionComponent.generated.h"


// �N���X�p�̋��R���W������\�����邽�߂̃R���|�[�l���g�B�g�p�̍ۂ́AUE4��/Engine/BasicShapes/Sphere��StaticMesh�ɐݒ肷�邱�ƁB
UCLASS(hidecategories=(Object,LOD, Physics, Collision), editinlinenew, meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class SHADERSANDBOX_API USphereCollisionComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	/** Set the geometry and vertex paintings to use on this triangle mesh as cloth. */
	UFUNCTION(BlueprintCallable, Category = "Components|ClothGridMesh")
	void SetRadius(float Radius);

	float GetRadius() const { return _Radius; }

private:
	float _Radius = 100.0f;
};
