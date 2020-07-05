#include "Quadtree/Quadtree.h"

namespace
{
using namespace Quadtree;

// �t���X�^���J�����O�B�r���[�t���X�^���̒���QuadNode���ꕔ�ł������Ă�����true�B�����łȂ����false�B
bool IsQuadNodeFrustumCulled(const FMatrix& ViewProjectionMatrix, const FQuadNode& Node)
{
	//FSceneView::ProjectWorldToScreen()���Q�l�ɂ��Ă���

	const FVector4& BottomRightProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(Node.BottomRight.X, Node.BottomRight.Y, Node.BottomRight.Z, 1.0f));
	const FVector4& BottomLeftProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(Node.BottomRight.X + Node.Length, Node.BottomRight.Y, Node.BottomRight.Z, 1.0f));
	const FVector4& TopRightProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(Node.BottomRight.X, Node.BottomRight.Y + Node.Length, Node.BottomRight.Z, 1.0f));
	const FVector4& TopLeftProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(Node.BottomRight.X + Node.Length, Node.BottomRight.Y + Node.Length, Node.BottomRight.Z, 1.0f));

	// NDC�ւ̕ϊ���W�ŏ��Z�������̂�W��0�ɋ߂��ꍇ��KINDA_SMALL_NUMBER�Ŋ���B���̏ꍇ�͖{����茋�ʂ��������l�ɂȂ�̂ő��J�����O����Ȃ�
	const FVector4& BottomRightNDC = BottomRightProjSpace / FMath::Max(FMath::Abs(BottomRightProjSpace.W), KINDA_SMALL_NUMBER);
	const FVector4& BottomLeftNDC = BottomLeftProjSpace / FMath::Max(FMath::Abs(BottomLeftProjSpace.W), KINDA_SMALL_NUMBER);
	const FVector4& TopRightNDC = TopRightProjSpace / FMath::Max(FMath::Abs(TopRightProjSpace.W), KINDA_SMALL_NUMBER);
	const FVector4& TopLeftNDC = TopLeftProjSpace / FMath::Max(FMath::Abs(TopLeftProjSpace.W), KINDA_SMALL_NUMBER);

	// �t���X�^���O�ɂ���Ƃ��́A4�R�[�i�[�����ׂāANDC�L���[�u��6�ʂ̂��������ꂩ�̖ʂ̊O�ɂ���Ƃ��ɂȂ��Ă���
	if (BottomRightNDC.X < -1.0f && BottomLeftNDC.X < -1.0f && TopRightNDC.X < -1.0f && TopLeftNDC.X < -1.0f)
	{
		return true;
	}
	if (BottomRightNDC.X > 1.0f && BottomLeftNDC.X > 1.0f && TopRightNDC.X > 1.0f && TopLeftNDC.X > 1.0f)
	{
		return true;
	}
	if (BottomRightNDC.Y < -1.0f && BottomLeftNDC.Y < -1.0f && TopRightNDC.Y < -1.0f && TopLeftNDC.Y < -1.0f)
	{
		return true;
	}
	if (BottomRightNDC.Y > 1.0f && BottomLeftNDC.Y > 1.0f && TopRightNDC.Y > 1.0f && TopLeftNDC.Y > 1.0f)
	{
		return true;
	}
	// Z��UE4��NDC�ł�[0,1]�ł���B
	if (BottomRightNDC.Z < 0.0f && BottomLeftNDC.Z < 0.0f && TopRightNDC.Z < 0.0f && TopLeftNDC.Z < 0.0f)
	{
		return true;
	}
	if (BottomRightNDC.Z > 1.0f && BottomLeftNDC.Z > 1.0f && TopRightNDC.Z > 1.0f && TopLeftNDC.Z > 1.0f)
	{
		return true;
	}

	return false;
}

// QuadNode�̃��b�V���̃O���b�h�̒��ł����Ƃ��J�����ɋ߂����̂̃X�N���[���\���ʐϗ����擾����
float EstimateGridScreenCoverage(int32 NumRowColumn, const FVector& CameraPosition, const FVector2D& ProjectionScale, const FMatrix& ViewProjectionMatrix, const FQuadNode& Node, FIntPoint& OutNearestGrid)
{
	// �\���ʐς��ő�̃O���b�h�𒲂ׂ����̂ŁA�J�����ɍł��߂��O���b�h�𒲂ׂ�B
	// �O���b�h�̒��ɂ͕\������Ă��Ȃ����̂����肤�邪�A�����ɓn�����QuadNode�̓t���X�^���J�����O�͂���ĂȂ��O��Ȃ̂�
	// �J�����ɍł��߂��O���b�h�͕\������Ă���͂��B

	// QuadNode���b�V���̃O���b�h�ւ̏c���������͓����ł���A���b�V���͐����`�ł���Ƃ����O�񂪂���
	float GridLength = Node.Length / NumRowColumn;
	FVector NearestGridBottomRight;

	// �J�����̐^���ɃO���b�h������΂���B�Ȃ����Clamp����
	int32 Row = FMath::Clamp<int32>((CameraPosition.X - Node.BottomRight.X) / GridLength, 0, NumRowColumn - 1); // float��int32�L���X�g�Ő؂�̂�
	int32 Column = FMath::Clamp<int32>((CameraPosition.Y - Node.BottomRight.Y) / GridLength, 0, NumRowColumn - 1); // float��int32�L���X�g�Ő؂�̂�
	OutNearestGrid = FIntPoint(Row, Column);
	NearestGridBottomRight.X = Node.BottomRight.X + Row * GridLength;
	NearestGridBottomRight.Y = Node.BottomRight.Y + Column * GridLength;
	NearestGridBottomRight.Z = Node.BottomRight.Z;

	// �O���b�h�ɃJ�����͐��΂��Ă��Ȃ����A���΂��Ă�Ƒz�肵���Ƃ��̃X�N���[���\���ʐς�Ԃ��B
	// ���ۂ̐��΂��ĂȂ��X�N���[���\���ʐς��g���ƁA���Ȃ��`���䂪�ނ��߂ɁA�䂪�݂��������Ƃ����Ƃ�����Ƃ����J�����̓����Ŗʐς��傫���ς����肵�Ȃ��Ȃ�B
	// NDC��[-1,1]�Ȃ̂�XY�ʂ��ʐς�4�Ȃ̂ŁA�\���ʐϗ��Ƃ��Ă�1/4����Z�������̂ɂȂ�
	float CameraDistanceSquare = (CameraPosition - NearestGridBottomRight).SizeSquared();
	float Ret = GridLength * ProjectionScale.X * GridLength * ProjectionScale.Y * 0.25f / CameraDistanceSquare;
	return Ret;
}

int32 BuildQuadtreeRecursively(int32 MaxLOD, int32 NumRowColumn, float MaxScreenCoverage, float PatchLength, const FVector& CameraPosition, const FVector2D& ProjectionScale, const FMatrix& ViewProjectionMatrix, FQuadNode& Node, TArray<FQuadNode>& OutQuadNodeList)
{
	bool bCulled = IsQuadNodeFrustumCulled(ViewProjectionMatrix, Node);
	if (bCulled)
	{
		return INDEX_NONE;
	}

	// QuadNode�ɋ��e����X�N���[���\���ʐϗ��̏���B
	//float MaxScreenCoverage = MaxPixelCoverage / View.SceneViewInitOptions.GetConstrainedViewRect().Size().X / View.SceneViewInitOptions.GetConstrainedViewRect().Size().Y;

	// QuadNode�̑S�O���b�h�̃X�N���[���\���ʐϗ��̒��ōő�̂��́B
	FIntPoint NearestGrid;
	float GridCoverage = EstimateGridScreenCoverage(NumRowColumn, CameraPosition, ProjectionScale, ViewProjectionMatrix, Node, NearestGrid);

	if (GridCoverage > MaxScreenCoverage // �O���b�h���\���ʐϗ�������傫����Ύq�ɕ�������B������`�悵�Ȃ��̂̓��X�g�̎g�p����IsLeaf�Ŕ��肷��B
		&& Node.Length > PatchLength // �p�b�`�T�C�Y�ȉ��̏c�������ł���΂���ȏ㕪�����Ȃ��B��̏����������ƃJ�������߂��Ƃ�����ł��������������Ă��܂��̂�
		&& Node.LOD > 0) // LOD0�̂��̂͂���ȏ㕪�����Ȃ�
	{
		// LOD��childListIndices�͏����l�ʂ�
		FQuadNode ChildNodeBottomRight;
		ChildNodeBottomRight.BottomRight = Node.BottomRight;
		ChildNodeBottomRight.Length = Node.Length * 0.5f;
		ChildNodeBottomRight.LOD = Node.LOD - 1;
		Node.ChildNodeIndices[0] = BuildQuadtreeRecursively(MaxLOD, NumRowColumn, MaxScreenCoverage, PatchLength, CameraPosition, ProjectionScale, ViewProjectionMatrix, ChildNodeBottomRight, OutQuadNodeList);

		FQuadNode ChildNodeBottomLeft;
		ChildNodeBottomLeft.BottomRight = Node.BottomRight + FVector(Node.Length * 0.5f, 0.0f, 0.0f);
		ChildNodeBottomLeft.Length = Node.Length * 0.5f;
		ChildNodeBottomLeft.LOD = Node.LOD - 1;
		Node.ChildNodeIndices[1] = BuildQuadtreeRecursively(MaxLOD, NumRowColumn, MaxScreenCoverage, PatchLength, CameraPosition, ProjectionScale, ViewProjectionMatrix, ChildNodeBottomLeft, OutQuadNodeList);

		FQuadNode ChildNodeTopRight;
		ChildNodeTopRight.BottomRight = Node.BottomRight + FVector(0.0f, Node.Length * 0.5f, 0.0f);
		ChildNodeTopRight.Length = Node.Length * 0.5f;
		ChildNodeTopRight.LOD = Node.LOD - 1;
		Node.ChildNodeIndices[2] = BuildQuadtreeRecursively(MaxLOD, NumRowColumn, MaxScreenCoverage, PatchLength, CameraPosition, ProjectionScale, ViewProjectionMatrix, ChildNodeTopRight, OutQuadNodeList);

		FQuadNode ChildNodeTopLeft;
		ChildNodeTopLeft.BottomRight = Node.BottomRight + FVector(Node.Length * 0.5f, Node.Length * 0.5f, 0.0f);
		ChildNodeTopLeft.Length = Node.Length * 0.5f;
		ChildNodeTopLeft.LOD = Node.LOD - 1;
		Node.ChildNodeIndices[3] = BuildQuadtreeRecursively(MaxLOD, NumRowColumn, MaxScreenCoverage, PatchLength, CameraPosition, ProjectionScale, ViewProjectionMatrix, ChildNodeTopLeft, OutQuadNodeList);

		// ���ׂĂ̎q�m�[�h���t���X�^���J�����O�Ώۂ�������A�������s���Ȃ͂��ŁA�J�����O�v�Z�ŘR�ꂽ�Ƃ݂�ׂ��Ȃ̂ŃJ�����O����
		if (Node.IsLeaf())
		{
			return INDEX_NONE;
		}
	}

	int32 Index = OutQuadNodeList.Add(Node);
	return Index;
}

int32 SearchLeafContainsPosition2D(const TArray<FQuadNode>& RenderQuadNodeList, const FVector2D& Position2D)
{
	// AllQuadNodeList����؂����ǂ��ĒT���Ă��������A���W�b�N�����G�ɂȂ邵�A�����I�ɂ���Ȃɏ������ɑ債�����͂łȂ��̂�RenderQuadNodeList���烋�[�v�ŒT��
	for (int32 i = 0; i < RenderQuadNodeList.Num(); i++)
	{
		if (RenderQuadNodeList[i].ContainsPosition2D(Position2D))
		{
			return i;
		}
	}

	return INDEX_NONE;
}


int32 GetGridMeshIndex(int32 Row, int32 Column, int32 NumColumn)
{
	return Row * (NumColumn + 1) + Column;
}

uint32 CreateInnerMesh(int32 NumRowColumn, TArray<uint32>& OutIndices)
{
	check(NumRowColumn % 2 == 0);

	// �����̕����͂ǂ̃��b�V���p�^�[���ł����������A���ׂē������̂��쐬����B�h���[�R�[���ŃC���f�b�N�X�o�b�t�@�̕s�A���A�N�Z�X�͂ł��Ȃ��̂�
	for (int32 Row = 1; Row < NumRowColumn - 1; Row++)
	{
		for (int32 Column = 1; Column < NumRowColumn - 1; Column++)
		{
			// 4���̃O���b�h���g���C�A���O��2�ɕ�������Ίp���́A���b�V���S�̂̑Ίp���̕����ɂȂ��Ă�������A
			// ����4���̕����ň���̕ӂ�LOD�����Ȃ�����̕ӂ�LOD��������ꍇ�ɁA���E�̃W�I���g�������̂������₷���B
			// ����ăC���f�b�N�X�������̃O���b�h�Ɗ�̃O���b�h�őΊp�����t�ɂ���B
			// TRIANGLE_STRIP�Ɠ����B
			// NumRowColumn�������ł��邱�Ƃ�O��ɂ��Ă���

			if ((Row + Column) % 2 == 0)
			{
				OutIndices.Emplace(GetGridMeshIndex(Row, Column, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(Row + 1, Column + 1, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(Row, Column + 1, NumRowColumn));

				OutIndices.Emplace(GetGridMeshIndex(Row, Column, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(Row + 1, Column, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(Row + 1, Column + 1, NumRowColumn));
			}
			else
			{
				OutIndices.Emplace(GetGridMeshIndex(Row, Column, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(Row + 1, Column, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(Row, Column + 1, NumRowColumn));

				OutIndices.Emplace(GetGridMeshIndex(Row + 1, Column, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(Row + 1, Column + 1, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(Row, Column + 1, NumRowColumn));
			}
		}
	}

	return 6 * (NumRowColumn - 2) * (NumRowColumn - 2);
}

uint32 CreateBoundaryMesh(EAdjacentQuadNodeLODDifference RightAdjLODDiff, EAdjacentQuadNodeLODDifference LeftAdjLODDiff, EAdjacentQuadNodeLODDifference BottomAdjLODDiff, EAdjacentQuadNodeLODDifference TopAdjLODDiff, int32 NumRowColumn, TArray<uint32>& OutIndices, TArray<Quadtree::FQuadMeshParameter>& OutQuadMeshParams)
{
	check(NumRowColumn % 2 == 0);
	uint32 NumIndices = 0;

	// ���E�����́ALOD�̍��ɍ��킹�ėׂƐڂ��镔���̃g���C�A���O���̕ӂ�2�{���邢��4�{�ɂ���
	// �ׂƐڂ���ӂ̒����g���C�A���O���P�ʂŃC���f�b�N�X��ǉ����Ă������߁ALOD�̍���1�Ȃ�O���b�h�����ǂ郋�[�v�̃X�e�b�v��2�A2�ȏ�Ȃ�4�P�ʂōs��

	// Right
	{
		if (RightAdjLODDiff == EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST)
		{
			for (int32 Row = 0; Row < NumRowColumn; Row++)
			{
				if (Row % 2 == 0)
				{
					if (Row > 0) // Bottom�Əd�����Ȃ��悤��
					{
						OutIndices.Emplace(GetGridMeshIndex(Row, 0, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(Row + 1, 1, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(Row, 1, NumRowColumn));
						NumIndices += 3;
					}

					OutIndices.Emplace(GetGridMeshIndex(Row, 0, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row + 1, 0, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row + 1, 1, NumRowColumn));
					NumIndices += 3;
				}
				else
				{
					OutIndices.Emplace(GetGridMeshIndex(Row, 0, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row + 1, 0, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row, 1, NumRowColumn));
					NumIndices += 3;

					if (Row < NumRowColumn) // Top�Əd�����Ȃ��悤��
					{
						OutIndices.Emplace(GetGridMeshIndex(Row + 1, 0, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(Row + 1, 1, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(Row, 1, NumRowColumn));
						NumIndices += 3;
					}
				}
			}
		}
		else
		{
			int32 Step = 1 << (int32)RightAdjLODDiff;

			for (int32 Row = 0; Row < NumRowColumn; Row += Step)
			{
				// �ڂ��镔���̕ӂ̒����g���C�A���O��
				OutIndices.Emplace(GetGridMeshIndex(Row, 0, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(Row + Step, 0, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(Row + (Step >> 1), 1, NumRowColumn));
				NumIndices += 3;

				// �ӂ̒����g���C�A���O���̎R�̉����𖄂߂�g���C�A���O��
				for (int32 i = 0; i < (Step >> 1); i++)
				{
					if (Row == 0 && i == 0)
					{
						// Bottom�Əd�����Ȃ��悤��4���̃g���C�A���O���̕��͍��Ȃ�
						continue;
					}

					OutIndices.Emplace(GetGridMeshIndex(Row, 0, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row + i + 1, 1, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row + i, 1, NumRowColumn));
					NumIndices += 3;
				}

				// �ӂ̒����g���C�A���O���̎R�̏㑤�𖄂߂�g���C�A���O��
				for (int32 i = (Step >> 1); i < Step; i++)
				{
					if (Row == (NumRowColumn - Step) && i == (Step - 1))
					{
						// Top�Əd�����Ȃ��悤��4���̃g���C�A���O���̕��͍��Ȃ�
						continue;
					}

					OutIndices.Emplace(GetGridMeshIndex(Row + Step, 0, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row + i + 1, 1, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row + i, 1, NumRowColumn));
					NumIndices += 3;
				}
			}
		}
	}

	// Left
	{
		if (LeftAdjLODDiff == EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST)
		{
			for (int32 Row = 0; Row < NumRowColumn; Row++)
			{
				if ((Row + NumRowColumn - 1) % 2 == 0)
				{
					OutIndices.Emplace(GetGridMeshIndex(Row, NumRowColumn - 1, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row + 1, NumRowColumn, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row, NumRowColumn, NumRowColumn));
					NumIndices += 3;

					if (Row < (NumRowColumn - 1)) // Top�Əd�����Ȃ��悤��
					{
						OutIndices.Emplace(GetGridMeshIndex(Row, NumRowColumn - 1, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(Row + 1, NumRowColumn - 1, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(Row + 1, NumRowColumn, NumRowColumn));
						NumIndices += 3;
					}
				}
				else
				{
					if (Row > 0) // Bottom�Əd�����Ȃ��悤��
					{
						OutIndices.Emplace(GetGridMeshIndex(Row, NumRowColumn - 1, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(Row + 1, NumRowColumn - 1, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(Row, NumRowColumn, NumRowColumn));
						NumIndices += 3;
					}

					OutIndices.Emplace(GetGridMeshIndex(Row + 1, NumRowColumn - 1, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row + 1, NumRowColumn, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row, NumRowColumn, NumRowColumn));
					NumIndices += 3;
				}
			}
		}
		else
		{
			int32 Step = 1 << (int32)LeftAdjLODDiff;

			for (int32 Row = 0; Row < NumRowColumn; Row += Step)
			{
				// �ڂ��镔���̕ӂ̒����g���C�A���O��
				OutIndices.Emplace(GetGridMeshIndex(Row, NumRowColumn, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(Row + (Step >> 1), NumRowColumn - 1, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(Row + Step, NumRowColumn, NumRowColumn));
				NumIndices += 3;

				// �ӂ̒����g���C�A���O���̎R�̉����𖄂߂�g���C�A���O��
				for (int32 i = 0; i < (Step >> 1); i++)
				{
					if (Row == 0 && i == 0)
					{
						// Bottom�Əd�����Ȃ��悤��4���̃g���C�A���O���̕��͍��Ȃ�
						continue;
					}

					OutIndices.Emplace(GetGridMeshIndex(Row, NumRowColumn, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row + i, NumRowColumn - 1, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row + i + 1, NumRowColumn - 1, NumRowColumn));
					NumIndices += 3;
				}

				// �ӂ̒����g���C�A���O���̎R�̏㑤�𖄂߂�g���C�A���O��
				for (int32 i = (Step >> 1); i < Step; i++)
				{
					if (Row == (NumRowColumn - Step) && i == (Step - 1))
					{
						// Top�Əd�����Ȃ��悤��4���̃g���C�A���O���̕��͍��Ȃ�
						continue;
					}

					OutIndices.Emplace(GetGridMeshIndex(Row + Step, NumRowColumn, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row + i, NumRowColumn - 1, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row + i + 1, NumRowColumn - 1, NumRowColumn));
					NumIndices += 3;
				}
			}
		}
	}

	// Bottom
	{
		if (BottomAdjLODDiff == EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST)
		{
			for (int32 Column = 0; Column < NumRowColumn; Column++)
			{
				if (Column % 2 == 0)
				{
					OutIndices.Emplace(GetGridMeshIndex(0, Column, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(1, Column + 1, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(0, Column + 1, NumRowColumn));
					NumIndices += 3;

					if (Column > 0) // Right�Əd�����Ȃ��悤��
					{
						OutIndices.Emplace(GetGridMeshIndex(0, Column, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(1, Column, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(1, Column + 1, NumRowColumn));
						NumIndices += 3;
					}
				}
				else
				{
					OutIndices.Emplace(GetGridMeshIndex(0, Column, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(1, Column, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(0, Column + 1, NumRowColumn));
					NumIndices += 3;

					if (Column < NumRowColumn - 1) // Left�Əd�����Ȃ��悤��
					{
						OutIndices.Emplace(GetGridMeshIndex(1, Column, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(1, Column + 1, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(0, Column + 1, NumRowColumn));
						NumIndices += 3;
					}
				}
			}
		}
		else
		{
			int32 Step = 1 << (int32)BottomAdjLODDiff;

			for (int32 Column = 0; Column < NumRowColumn; Column += Step)
			{
				// �ڂ��镔���̕ӂ̒����g���C�A���O��
				OutIndices.Emplace(GetGridMeshIndex(0, Column, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(1, Column + (Step >> 1), NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(0, Column + Step, NumRowColumn));
				NumIndices += 3;

				// �ӂ̒����g���C�A���O���̎R�̉E���𖄂߂�g���C�A���O��
				for (int32 i = 0; i < (Step >> 1); i++)
				{
					if (Column == 0 && i == 0)
					{
						// Bottom�Əd�����Ȃ��悤��4���̃g���C�A���O���̕��͍��Ȃ�
						continue;
					}

					OutIndices.Emplace(GetGridMeshIndex(0, Column, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(1, Column + i, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(1, Column + i + 1, NumRowColumn));
					NumIndices += 3;
				}

				// �ӂ̒����g���C�A���O���̎R�̍����𖄂߂�g���C�A���O��
				for (int32 i = (Step >> 1); i < Step; i++)
				{
					if (Column == (NumRowColumn - Step) && i == (Step - 1))
					{
						// Top�Əd�����Ȃ��悤��4���̃g���C�A���O���̕��͍��Ȃ�
						continue;
					}

					OutIndices.Emplace(GetGridMeshIndex(0, Column + Step, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(1, Column + i, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(1, Column + i + 1, NumRowColumn));
					NumIndices += 3;
				}
			}
		}
	}

	// Top
	{
		if (TopAdjLODDiff == EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST)
		{
			for (int32 Column = 0; Column < NumRowColumn; Column++)
			{
				// 4���̃O���b�h���g���C�A���O��2�ɕ�������Ίp���́A���b�V���S�̂̑Ίp���̕����ɂȂ��Ă�������A
				// ����4���̕����ň���̕ӂ�LOD�����Ȃ�����̕ӂ�LOD��������ꍇ�ɁA���E�̃W�I���g�������̂������₷���B
				// ����ăC���f�b�N�X�������̃O���b�h�Ɗ�̃O���b�h�őΊp�����t�ɂ���B
				// TRIANGLE_STRIP�Ɠ����B
				// NumRowColumn�������ł��邱�Ƃ�O��ɂ��Ă���

				if ((NumRowColumn - 1 + Column) % 2 == 0)
				{
					if (Column < NumRowColumn - 1) // Left�Əd�����Ȃ��悤��
					{
						OutIndices.Emplace(GetGridMeshIndex(NumRowColumn - 1, Column, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(NumRowColumn, Column + 1, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(NumRowColumn - 1, Column + 1, NumRowColumn));
						NumIndices += 3;
					}

					OutIndices.Emplace(GetGridMeshIndex(NumRowColumn - 1, Column, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(NumRowColumn, Column, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(NumRowColumn, Column + 1, NumRowColumn));
					NumIndices += 3;
				}
				else
				{
					if (Column > 0) // Right�Əd�����Ȃ��悤��
					{
						OutIndices.Emplace(GetGridMeshIndex(NumRowColumn - 1, Column, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(NumRowColumn, Column, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(NumRowColumn - 1, Column + 1, NumRowColumn));
						NumIndices += 3;
					}

					OutIndices.Emplace(GetGridMeshIndex(NumRowColumn, Column, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(NumRowColumn, Column + 1, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(NumRowColumn - 1, Column + 1, NumRowColumn));
					NumIndices += 3;
				}
			}
		}
		else
		{
			int32 Step = 1 << (int32)TopAdjLODDiff;

			for (int32 Column = 0; Column < NumRowColumn; Column += Step)
			{
				// �ڂ��镔���̕ӂ̒����g���C�A���O��
				OutIndices.Emplace(GetGridMeshIndex(NumRowColumn, Column, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(NumRowColumn, Column + Step, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(NumRowColumn - 1, Column + (Step >> 1), NumRowColumn));
				NumIndices += 3;

				// �ӂ̒����g���C�A���O���̎R�̉E���𖄂߂�g���C�A���O��
				for (int32 i = 0; i < (Step >> 1); i++)
				{
					if (Column == 0 && i == 0)
					{
						// Bottom�Əd�����Ȃ��悤��4���̃g���C�A���O���̕��͍��Ȃ�
						continue;
					}

					OutIndices.Emplace(GetGridMeshIndex(NumRowColumn, Column, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(NumRowColumn - 1, Column + i + 1, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(NumRowColumn - 1, Column + i, NumRowColumn));
					NumIndices += 3;
				}

				// �ӂ̒����g���C�A���O���̎R�̍����𖄂߂�g���C�A���O��
				for (int32 i = (Step >> 1); i < Step; i++)
				{
					if (Column == (NumRowColumn - Step) && i == (Step - 1))
					{
						// Top�Əd�����Ȃ��悤��4���̃g���C�A���O���̕��͍��Ȃ�
						continue;
					}

					OutIndices.Emplace(GetGridMeshIndex(NumRowColumn, Column + Step, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(NumRowColumn - 1, Column + i + 1, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(NumRowColumn - 1, Column + i, NumRowColumn));
					NumIndices += 3;
				}
			}
		}
	}

	return NumIndices;
}
} // namespace

namespace Quadtree
{
bool FQuadNode::IsLeaf() const
{
	return (ChildNodeIndices[0] == INDEX_NONE) && (ChildNodeIndices[1] == INDEX_NONE) && (ChildNodeIndices[2] == INDEX_NONE) && (ChildNodeIndices[3] == INDEX_NONE);
}

bool FQuadNode::ContainsPosition2D(const FVector2D& Position2D) const
{
	return (BottomRight.X <= Position2D.X && Position2D.X <= (BottomRight.X + Length) && BottomRight.Y <= Position2D.Y && Position2D.Y <= (BottomRight.Y + Length));
}

void BuildQuadtree(int32 MaxLOD, int32 NumRowColumn, float MaxScreenCoverage, float PatchLength, const FVector& CameraPosition, const FVector2D& ProjectionScale, const FMatrix& ViewProjectionMatrix, FQuadNode& RootNode, TArray<FQuadNode>& OutAllQuadNodeList, TArray<FQuadNode>& OutRenderQuadNodeList)
{
	BuildQuadtreeRecursively(MaxLOD, NumRowColumn, MaxScreenCoverage, PatchLength, CameraPosition, ProjectionScale, ViewProjectionMatrix, RootNode, OutAllQuadNodeList);

	for (const FQuadNode& Node : OutAllQuadNodeList)
	{
		if (Node.IsLeaf())
		{
			OutRenderQuadNodeList.Add(Node);
		}
	}
}

EAdjacentQuadNodeLODDifference QueryAdjacentNodeType(const FQuadNode& Node, const FVector2D& AdjacentPosition, const TArray<FQuadNode>& RenderQuadNodeList)
{
	int32 AdjNodeIndex = SearchLeafContainsPosition2D(RenderQuadNodeList, AdjacentPosition);

	EAdjacentQuadNodeLODDifference Ret = EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST;
	if (AdjNodeIndex != INDEX_NONE)
	{
		int32 AdjNodeLOD = RenderQuadNodeList[AdjNodeIndex].LOD;
		if (Node.LOD + 2 <= AdjNodeLOD)
		{
			Ret = EAdjacentQuadNodeLODDifference::GREATER_BY_MORE_THAN_2;
		}
		else if (Node.LOD + 1 <= AdjNodeLOD)
		{
			Ret = EAdjacentQuadNodeLODDifference::GREATER_BY_1;
		}
	}

	return Ret;
}

void CreateQuadMeshes(int32 NumRowColumn, TArray<uint32>& OutIndices, TArray<Quadtree::FQuadMeshParameter>& OutQuadMeshParams)
{
	check((uint32)EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST == 0);
	check((uint32)EAdjacentQuadNodeLODDifference::MAX == 3);

	// �O���b�h�����ׂ�2�̃g���C�A���O���ŕ��������ꍇ�̃��b�V����81�p�^�[�����Ŋm�ۂ��Ă����B���m�ɂ͋��E���������傫�ȃg���C�A���O����
	// ��������p�^�[�����܂ނ̂ł����Ə��Ȃ����A�������G�ɂȂ�̂ő傫�߂Ɋm�ۂ��Ă���
	OutIndices.Reset(81 * 2 * 3 * NumRowColumn * NumRowColumn);

	// �E�ׂ�LOD�������ȉ��Ȃ̂ƁA��i�K��Ȃ̂ƁA��i�K�ȏ�Ȃ̂�3�p�^�[���B���ׁA���ׁA��ׂł�3�p�^�[���ł����̑g�ݍ��킹��3*3*3*3�p�^�[���B
	OutQuadMeshParams.Reset(81);

	uint32 IndexOffset = 0;
	for (uint32 RightType = (uint32)EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST; RightType < (uint32)EAdjacentQuadNodeLODDifference::MAX; RightType++)
	{
		for (uint32 LeftType = (uint32)EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST; LeftType < (uint32)EAdjacentQuadNodeLODDifference::MAX; LeftType++)
		{
			for (uint32 BottomType = (uint32)EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST; BottomType < (uint32)EAdjacentQuadNodeLODDifference::MAX; BottomType++)
			{
				for (uint32 TopType = (uint32)EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST; TopType < (uint32)EAdjacentQuadNodeLODDifference::MAX; TopType++)
				{
					uint32 NumInnerMeshIndices = CreateInnerMesh(NumRowColumn, OutIndices);
					uint32 NumBoundaryMeshIndices = CreateBoundaryMesh((EAdjacentQuadNodeLODDifference)RightType, (EAdjacentQuadNodeLODDifference)LeftType, (EAdjacentQuadNodeLODDifference)BottomType, (EAdjacentQuadNodeLODDifference)TopType, NumRowColumn, OutIndices, OutQuadMeshParams);
					// TArray�̃C���f�b�N�X�́ARightType * 3^3 + LeftType * 3^2 + BottomType * 3^1 + TopType * 3^0�ƂȂ�
					OutQuadMeshParams.Emplace(IndexOffset, NumInnerMeshIndices + NumBoundaryMeshIndices);
					IndexOffset += NumInnerMeshIndices + NumBoundaryMeshIndices;
				}
			}
		}
	}
}
} // namespace Quadtree

