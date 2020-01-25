#include "Cloth/ClothParameterStructuredBuffer.h"
#include "Cloth/ClothGridMeshParameters.h"

FClothParameterStructuredBuffer::~FClothParameterStructuredBuffer()
{
	ComponentsDataSRV.SafeRelease();
	ComponentsData.SafeRelease();
}

void FClothParameterStructuredBuffer::SetData(const TArray<struct FGridClothParameters>& Data)
{
	if (ComponentsData.IsValid())
	{
		// ���ɏ������ς݂Ȃ�l�̐ݒ�
		check(ComponentsDataSRV.IsValid());

		//TODO: Reallocate�͂��ĂȂ��̂ŁA�N���X�C���X�^���X�̐�����������͂��Ȃ��Ƃ����O��B������悤�Ȃ�A�O�̂悤��BUF_Volatile�ɂ��Ė��t���[��new������������
		uint32 Size = Data.Num() * sizeof(FGridClothParameters);
		uint8* Buffer = (uint8*)RHILockStructuredBuffer(ComponentsData, 0, Size, EResourceLockMode::RLM_WriteOnly);
		FMemory::Memcpy(Buffer, Data.GetData(), Size);
		RHIUnlockStructuredBuffer(ComponentsData);
	}
	else
	{
		// �܂����������ĂȂ��Ȃ�InitResource()����InitDynamicRHI()�̌Ăяo��
		DataCache = Data;
		InitResource();
	}
}

void FClothParameterStructuredBuffer::InitDynamicRHI()
{
	FRHIResourceCreateInfo CreateInfo;
	uint32 Size = DataCache.Num() * sizeof(FGridClothParameters);

	ComponentsData = RHICreateStructuredBuffer(sizeof(FGridClothParameters), Size, EBufferUsageFlags::BUF_Dynamic | EBufferUsageFlags::BUF_ShaderResource, CreateInfo);
	ComponentsDataSRV = RHICreateShaderResourceView(ComponentsData);

	uint8* Buffer = (uint8*)RHILockStructuredBuffer(ComponentsData, 0, Size, EResourceLockMode::RLM_WriteOnly);
	FMemory::Memcpy(Buffer, DataCache.GetData(), Size);
	RHIUnlockStructuredBuffer(ComponentsData);
}

void FClothParameterStructuredBuffer::ReleaseDynamicRHI()
{
	ComponentsDataSRV.SafeRelease();
	ComponentsData.SafeRelease();
}

