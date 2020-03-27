#include "Quadtree.h"

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
} // namespace

namespace Quadtree
{
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
} // namespace Quadtree

