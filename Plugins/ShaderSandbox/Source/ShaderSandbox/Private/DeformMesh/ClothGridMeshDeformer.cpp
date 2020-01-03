#include "ClothGridMeshDeformer.h"
#include "GlobalShader.h"
#include "RHIResources.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"

class FClothMeshCopyToWorkBufferCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClothMeshCopyToWorkBufferCS);
	SHADER_USE_PARAMETER_STRUCT(FClothMeshCopyToWorkBufferCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, VertexIndexOffset)
		SHADER_PARAMETER(uint32, NumVertex)
		SHADER_PARAMETER_SRV(Buffer<float>, AccelerationVertexBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float>, WorkAccelerationBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float>, PrevPositionVertexBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float>, WorkPrevPositionBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float>, PositionVertexBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float>, WorkPositionBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FClothMeshCopyToWorkBufferCS, "/Plugin/ShaderSandbox/Private/ClothMeshCopy.usf", "CopyToWorkBuffer", SF_Compute);

class FClothMeshCopyFromWorkBufferCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClothMeshCopyFromWorkBufferCS);
	SHADER_USE_PARAMETER_STRUCT(FClothMeshCopyFromWorkBufferCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, VertexIndexOffset)
		SHADER_PARAMETER(uint32, NumVertex)
		SHADER_PARAMETER_UAV(RWBuffer<float>, PrevPositionVertexBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float>, WorkPrevPositionBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float>, PositionVertexBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float>, WorkPositionBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FClothMeshCopyFromWorkBufferCS, "/Plugin/ShaderSandbox/Private/ClothMeshCopy.usf", "CopyFromWorkBuffer", SF_Compute);

class FClothSimulationCS : public FGlobalShader
{
public:
	// TODO:�萔�o�b�t�@���g���Ă��邤���̓T�C�Y��������肠�܂�傫���ł��Ȃ��BStructuredBuffer�ɒu�������悤
	static const uint32 MAX_CLOTH_MESH = 8;
	static const uint32 MAX_SPHERE_COLLISION_PER_MESH = 4;

	DECLARE_GLOBAL_SHADER(FClothSimulationCS);
	SHADER_USE_PARAMETER_STRUCT(FClothSimulationCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_ARRAY(uint32, NumIteration, [MAX_CLOTH_MESH])
		SHADER_PARAMETER_ARRAY(uint32, NumRow, [MAX_CLOTH_MESH])
		SHADER_PARAMETER_ARRAY(uint32, NumColumn, [MAX_CLOTH_MESH])
		SHADER_PARAMETER_ARRAY(uint32, VertexIndexOffset, [MAX_CLOTH_MESH])
		SHADER_PARAMETER_ARRAY(uint32, NumVertex, [MAX_CLOTH_MESH])
		SHADER_PARAMETER_ARRAY(float, GridWidth, [MAX_CLOTH_MESH])
		SHADER_PARAMETER_ARRAY(float, GridHeight, [MAX_CLOTH_MESH])
		SHADER_PARAMETER_ARRAY(float, SquareDeltaTime, [MAX_CLOTH_MESH])
		SHADER_PARAMETER_ARRAY(float, Stiffness, [MAX_CLOTH_MESH])
		SHADER_PARAMETER_ARRAY(float, Damping, [MAX_CLOTH_MESH])
		SHADER_PARAMETER_ARRAY(FVector, PreviousInertia, [MAX_CLOTH_MESH])
		SHADER_PARAMETER_ARRAY(FVector, WindVelocity, [MAX_CLOTH_MESH])
		SHADER_PARAMETER_ARRAY(float, FluidDensity, [MAX_CLOTH_MESH])
		SHADER_PARAMETER_ARRAY(float, DeltaTime, [MAX_CLOTH_MESH])
		SHADER_PARAMETER_ARRAY(float, VertexRadius, [MAX_CLOTH_MESH])
		SHADER_PARAMETER_ARRAY(uint32, NumSphereCollision, [MAX_CLOTH_MESH])
		SHADER_PARAMETER_ARRAY(FVector4, SphereCenterAndRadiusArray, [MAX_CLOTH_MESH * MAX_SPHERE_COLLISION_PER_MESH])
		SHADER_PARAMETER_UAV(RWBuffer<float>, WorkAccelerationVertexBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float>, WorkPrevPositionVertexBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float>, WorkPositionVertexBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FClothSimulationCS, "/Plugin/ShaderSandbox/Private/ClothSimulationGridMesh.usf", "Main", SF_Compute);

class FClothGridMeshTangentCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClothGridMeshTangentCS);
	SHADER_USE_PARAMETER_STRUCT(FClothGridMeshTangentCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, NumRow)
		SHADER_PARAMETER(uint32, NumColumn)
		SHADER_PARAMETER(uint32, NumVertex)
		SHADER_PARAMETER_UAV(RWBuffer<float>, InPositionVertexBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float4>, OutTangentVertexBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FClothGridMeshTangentCS, "/Plugin/ShaderSandbox/Private/GridMeshTangent.usf", "MainCS", SF_Compute);

void FClothGridMeshDeformer::EnqueueDeformTask(const FGridClothParameters& Param)
{
	DeformTaskQueue.Add(Param);
}

void FClothGridMeshDeformer::FlushDeformTaskQueue(FRHICommandListImmediate& RHICmdList, FRHIUnorderedAccessView* WorkAccelerationVertexBufferUAV, FRHIUnorderedAccessView* WorkPrevVertexBufferUAV, FRHIUnorderedAccessView* WorkVertexBufferUAV)
{
	FRDGBuilder GraphBuilder(RHICmdList);

	TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);

	uint32 NumClothMesh = DeformTaskQueue.Num();
	// TODO:�ǂ����Ńo���f�[�V����������
	check(NumClothMesh > 0);
	check(NumClothMesh <= FClothSimulationCS::MAX_CLOTH_MESH);

	// TODO:�֐������悤
	{
		uint32 Offset = 0;
		for (uint32 MeshIdx = 0; MeshIdx < NumClothMesh; MeshIdx++)
		{
			TShaderMapRef<FClothMeshCopyToWorkBufferCS> ClothMeshCopyToWorkBufferCS(ShaderMap);
			FClothMeshCopyToWorkBufferCS::FParameters* ClothCopyToWorkParams = GraphBuilder.AllocParameters<FClothMeshCopyToWorkBufferCS::FParameters>();

			const FGridClothParameters& GridClothParams = DeformTaskQueue[MeshIdx];
			ClothCopyToWorkParams->VertexIndexOffset = Offset;
			Offset += GridClothParams.NumVertex;
			ClothCopyToWorkParams->NumVertex = GridClothParams.NumVertex;
			ClothCopyToWorkParams->AccelerationVertexBuffer = GridClothParams.AccelerationVertexBufferSRV;
			ClothCopyToWorkParams->WorkAccelerationBuffer = WorkAccelerationVertexBufferUAV;
			ClothCopyToWorkParams->PrevPositionVertexBuffer = GridClothParams.PrevPositionVertexBufferUAV;
			ClothCopyToWorkParams->WorkPrevPositionBuffer = WorkPrevVertexBufferUAV;
			ClothCopyToWorkParams->PositionVertexBuffer = GridClothParams.PositionVertexBufferUAV;
			ClothCopyToWorkParams->WorkPositionBuffer = WorkVertexBufferUAV;

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("ClothMeshCopyToWorkBuffer"),
				*ClothMeshCopyToWorkBufferCS,
				ClothCopyToWorkParams,
				FIntVector(1, 1, 1)
			);
		}
	}

	{
		FClothSimulationCS::FParameters* ClothSimParams = GraphBuilder.AllocParameters<FClothSimulationCS::FParameters>();
		// TODO:Stiffness�ADamping�̌��ʂ�NumIteration��t���[�����[�g�Ɉˑ����Ă��܂��Ă���̂łǂ��ɂ����˂�
		uint32 Offset = 0;
		for (uint32 MeshIdx = 0; MeshIdx < NumClothMesh; MeshIdx++)
		{
			const FGridClothParameters& GridClothParams = DeformTaskQueue[MeshIdx];

			float DeltaTimePerIterate = GridClothParams.DeltaTime / GridClothParams.NumIteration;
			float SquareDeltaTime = DeltaTimePerIterate * DeltaTimePerIterate;
			const FVector& WindVelocity = GridClothParams.WindVelocity * FMath::FRandRange(0.0f, 2.0f); //���t���[���A���͂Ƀ����_���Ȃ�炬������

			ClothSimParams->NumIteration[MeshIdx] = GridClothParams.NumIteration;
			ClothSimParams->NumRow[MeshIdx] = GridClothParams.NumRow;
			ClothSimParams->NumColumn[MeshIdx] = GridClothParams.NumColumn;
			ClothSimParams->VertexIndexOffset[MeshIdx] = Offset;
			Offset += GridClothParams.NumVertex;
			ClothSimParams->NumVertex[MeshIdx] = GridClothParams.NumVertex;
			ClothSimParams->GridWidth[MeshIdx] = GridClothParams.GridWidth;
			ClothSimParams->GridHeight[MeshIdx] = GridClothParams.GridHeight;
			ClothSimParams->SquareDeltaTime[MeshIdx] = SquareDeltaTime;
			ClothSimParams->Stiffness[MeshIdx] = GridClothParams.Stiffness;
			ClothSimParams->Damping[MeshIdx] = GridClothParams.Damping;
			ClothSimParams->PreviousInertia[MeshIdx] = GridClothParams.PreviousInertia;
			ClothSimParams->WindVelocity[MeshIdx] = WindVelocity;
			ClothSimParams->FluidDensity[MeshIdx] = GridClothParams.FluidDensity / (100.0f * 100.0f * 100.0f); // �V�F�[�_�̌v�Z��MKS�P�ʌn��Ȃ̂ł���ɓ����FluidDensity�͂��������������˂΂Ȃ炸���[�U�����͂��ɂ����̂ŁAMKS�P�ʌn�œ��ꂳ���Ă����Ă����ŃX�P�[������
			ClothSimParams->DeltaTime[MeshIdx] = DeltaTimePerIterate;
			ClothSimParams->VertexRadius[MeshIdx] = GridClothParams.VertexRadius;
			ClothSimParams->NumSphereCollision[MeshIdx] = DeformTaskQueue[MeshIdx].SphereCollisionParams.Num(); //TODO:�Ƃ肠������������O��ŃN���X0�ɂ��ׂẴR���W�������ݒ肳��Ă�O��

			check(DeformTaskQueue[MeshIdx].SphereCollisionParams.Num() <= FClothSimulationCS::MAX_SPHERE_COLLISION_PER_MESH);
			for (uint32 CollisionIdx = 0; CollisionIdx < FClothSimulationCS::MAX_SPHERE_COLLISION_PER_MESH; CollisionIdx++)
			{
				if (CollisionIdx < ClothSimParams->NumSphereCollision[MeshIdx]) //TODO:�Ƃ肠������������O��ŃN���X0�ɂ��ׂẴR���W�������ݒ肳��Ă�O��
				{
					ClothSimParams->SphereCenterAndRadiusArray[MeshIdx * FClothSimulationCS::MAX_SPHERE_COLLISION_PER_MESH + CollisionIdx]
						= FVector4(GridClothParams.SphereCollisionParams[CollisionIdx].RelativeCenter, GridClothParams.SphereCollisionParams[CollisionIdx].Radius);
				}
				else
				{
					ClothSimParams->SphereCenterAndRadiusArray[MeshIdx * FClothSimulationCS::MAX_SPHERE_COLLISION_PER_MESH + CollisionIdx]
						= FVector4(0.0f, 0.0f, 0.0f, 0.0f);
				}
			}
		}

		ClothSimParams->WorkAccelerationVertexBuffer = WorkAccelerationVertexBufferUAV;
		ClothSimParams->WorkPrevPositionVertexBuffer = WorkPrevVertexBufferUAV;
		ClothSimParams->WorkPositionVertexBuffer = WorkVertexBufferUAV;

		TShaderMapRef<FClothSimulationCS> ClothSimulationCS(ShaderMap);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ClothSimulation"),
			*ClothSimulationCS,
			ClothSimParams,
			FIntVector(NumClothMesh, 1, 1)
		);
	}

	{
		uint32 Offset = 0;
		for (uint32 MeshIdx = 0; MeshIdx < NumClothMesh; MeshIdx++)
		{
			TShaderMapRef<FClothMeshCopyFromWorkBufferCS> ClothMeshCopyFromWorkBufferCS(ShaderMap);

			FClothMeshCopyFromWorkBufferCS::FParameters* ClothCopyFromWorkParams = GraphBuilder.AllocParameters<FClothMeshCopyFromWorkBufferCS::FParameters>();
			const FGridClothParameters& GridClothParams = DeformTaskQueue[MeshIdx];
			ClothCopyFromWorkParams->VertexIndexOffset = Offset;
			Offset += GridClothParams.NumVertex;
			ClothCopyFromWorkParams->NumVertex = GridClothParams.NumVertex;
			ClothCopyFromWorkParams->PrevPositionVertexBuffer = GridClothParams.PrevPositionVertexBufferUAV;
			ClothCopyFromWorkParams->WorkPrevPositionBuffer = WorkPrevVertexBufferUAV;
			ClothCopyFromWorkParams->PositionVertexBuffer = GridClothParams.PositionVertexBufferUAV;
			ClothCopyFromWorkParams->WorkPositionBuffer = WorkVertexBufferUAV;

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("ClothMeshCopyFromWorkBuffer"),
				*ClothMeshCopyFromWorkBufferCS,
				ClothCopyFromWorkParams,
				FIntVector(1, 1, 1)
			);
		}
	}

	{
		for (const FGridClothParameters& GridClothParams : DeformTaskQueue)
		{
			TShaderMapRef<FClothGridMeshTangentCS> GridMeshTangentCS(ShaderMap);

			FClothGridMeshTangentCS::FParameters* GridMeshTangentParams = GraphBuilder.AllocParameters<FClothGridMeshTangentCS::FParameters>();
			GridMeshTangentParams->NumRow = GridClothParams.NumRow;
			GridMeshTangentParams->NumColumn = GridClothParams.NumColumn;
			GridMeshTangentParams->NumVertex = GridClothParams.NumVertex;
			GridMeshTangentParams->InPositionVertexBuffer = GridClothParams.PositionVertexBufferUAV;
			GridMeshTangentParams->OutTangentVertexBuffer = GridClothParams.TangentVertexBufferUAV;

			const uint32 DispatchCount = FMath::DivideAndRoundUp(GridClothParams.NumVertex, (uint32)32);
			check(DispatchCount <= 65535);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("GridMeshTangent"),
				*GridMeshTangentCS,
				GridMeshTangentParams,
				FIntVector(DispatchCount, 1, 1)
			);
		}
	}

	GraphBuilder.Execute();

	DeformTaskQueue.Reset();
}

