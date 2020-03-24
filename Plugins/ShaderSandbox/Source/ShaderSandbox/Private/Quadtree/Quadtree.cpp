#include "Quadtree.h"

namespace
{
using namespace Quadtree;

// �t���X�^���J�����O�B�r���[�t���X�^���̒���QuadNode���ꕔ�ł������Ă�����true�B�����łȂ����false�B
bool IsQuadNodeFrustumCulled(const FMatrix& ViewProjectionMatrix, const FQuadNode& Node)
{
	//FSceneView::ProjectWorldToScreen()���Q�l�ɂ��Ă���

	const FVector4& BottomRightProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(Node.BottomRight.X, Node.BottomRight.Y, 0.0f, 1.0f));
	const FVector4& BottomLeftProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(Node.BottomRight.X + Node.Length, Node.BottomRight.Y, 0.0f, 1.0f));
	const FVector4& TopRightProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(Node.BottomRight.X, Node.BottomRight.Y + Node.Length, 0.0f, 1.0f));
	const FVector4& TopLeftProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(Node.BottomRight.X + Node.Length, Node.BottomRight.Y + Node.Length, 0.0f, 1.0f));

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
float EstimateGridScreenCoverage(int32 NumRowColumn, const FVector& CameraRayIntersectionPoint, const FMatrix& ViewProjectionMatrix, const FQuadNode& Node, FIntPoint& OutNearestGrid)
{
	// �\���ʐς��ő�̃O���b�h�𒲂ׂ����̂ŁA�J�������C��Quadtreeh�Ɍ������Ă�_����ł��߂��O���b�h��I��ł���B
	// ���ۂ̓J�����ɋ߂�QuadNode�ł̓t���X�^�����ł����Ƒ傫�ȃO���b�h�����邱�Ƃ��������A�ŋߐڃO���b�h��T���v�Z�����G�ɂȂ�̂Ŋȗ������Ă���B

	// QuadNode���b�V���̃O���b�h�ւ̏c���������͓����ł���A���b�V���͐����`�ł���Ƃ����O�񂪂���
	float GridLength = Node.Length / NumRowColumn;
	FVector2D NearestGridBottomRight;

	// �J�����̐^���ɃO���b�h������΂���B�Ȃ����Clamp����
	int32 Row = FMath::Clamp<int32>((CameraRayIntersectionPoint.X - Node.BottomRight.X) / GridLength, 0, NumRowColumn - 1); // float��int32�L���X�g�Ő؂�̂�
	int32 Column = FMath::Clamp<int32>((CameraRayIntersectionPoint.Y - Node.BottomRight.Y) / GridLength, 0, NumRowColumn - 1); // float��int32�L���X�g�Ő؂�̂�
	OutNearestGrid = FIntPoint(Row, Column);
	NearestGridBottomRight.X = Node.BottomRight.X + Row * GridLength;
	NearestGridBottomRight.Y = Node.BottomRight.Y + Column * GridLength;

	// �ŋߐڃO���b�h�̃X�N���[���ł̕\���ʐϗ��v�Z
	const FVector4& NearestGridBottomRightProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(NearestGridBottomRight.X, NearestGridBottomRight.Y, 0.0f, 1.0f));
	const FVector4& NearestGridBottomLeftProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(NearestGridBottomRight.X + GridLength, NearestGridBottomRight.Y, 0.0f, 1.0f));
	const FVector4& NearestGridTopRightProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(NearestGridBottomRight.X, NearestGridBottomRight.Y + GridLength, 0.0f, 1.0f));
	const FVector4& NearestGridTopLeftProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(NearestGridBottomRight.X + GridLength, NearestGridBottomRight.Y + GridLength, 0.0f, 1.0f));

	// NDC�ւ̕ϊ���W�ŏ��Z�������̂�W��0�ɋ߂��ꍇ��KINDA_SMALL_NUMBER�Ŋ���B���̏ꍇ�͖{����茋�ʂ��������l�ɂȂ�̂ŁA���A�t�ɂȂ�₷���Ȃ�\������₷���Ȃ�B
	const FVector2D& NearestGridBottomRightNDC = FVector2D(NearestGridBottomRightProjSpace) / FMath::Max(FMath::Abs(NearestGridBottomRightProjSpace.W), KINDA_SMALL_NUMBER);
	const FVector2D& NearestGridBottomLeftNDC = FVector2D(NearestGridBottomLeftProjSpace) / FMath::Max(FMath::Abs(NearestGridBottomLeftProjSpace.W), KINDA_SMALL_NUMBER);
	const FVector2D& NearestGridTopRightNDC = FVector2D(NearestGridTopRightProjSpace) / FMath::Max(FMath::Abs(NearestGridTopRightProjSpace.W), KINDA_SMALL_NUMBER);

	// �ʐς����߂�ɂ̓w�����̌������g���̂����������ASqrt�̏�������������̂ŏ������ׂ̂��߁A�����悻���s�l�ӌ`�ƂƂ炦�Ėʐς��v�Z����B
	// ���Ƃ�2�悷��̂ŕ��ς��Ƃ�Ƃ��ɐ����̕����͖����B
	const FVector2D& HorizEdge = NearestGridBottomLeftNDC - NearestGridBottomRightNDC;
	const FVector2D& VertEdge = NearestGridTopRightNDC - NearestGridBottomRightNDC;

	// �O���b�h�̃C���f�b�N�X�������������X��Y��������UE4�͍���n�Ȃ̂ŁA�J������Z�����ɂ���Ƃ����O��Ȃ�O�ς̕����͔��]���������̂����s�l�ӌ`�̖ʐςɂȂ�
	// NDC��[-1,1]�Ȃ̂�XY�ʂ��ʐς�4�Ȃ̂ŁA�\���ʐϗ��Ƃ��Ă�1/4����Z�������̂ɂȂ�
	// �������A�J�����Ƌ߂��A���b�V���̖ڂ��e�����̂قǁA���s�l�ӌ`�ߎ��͑e���Ȃ�
	// �R�[�i�[���J�����̌��ɂ܂��ꍇ�Ȃǂ͕��������ɂ͈����Ȃ��Ȃ�̂ŁA����ł��������ň������߂ɐ�Βl�ň���
	float Ret = FMath::Abs(HorizEdge ^ VertEdge) * 0.25f;
	return Ret;
}

int32 BuildQuadtreeRecursively(int32 MaxLOD, int32 NumRowColumn, float MaxScreenCoverage, float PatchLength, const FVector& CameraRayIntersectionPoint, const FMatrix& ViewProjectionMatrix, FQuadNode& Node, TArray<FQuadNode>& OutQuadNodeList)
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
	float GridCoverage = EstimateGridScreenCoverage(NumRowColumn, CameraRayIntersectionPoint, ViewProjectionMatrix, Node, NearestGrid);

	if (GridCoverage > MaxScreenCoverage // �O���b�h���\���ʐϗ�������傫����Ύq�ɕ�������B������`�悵�Ȃ��̂̓��X�g�̎g�p����IsLeaf�Ŕ��肷��B
		&& Node.Length > PatchLength // �p�b�`�T�C�Y�ȉ��̏c�������ł���΂���ȏ㕪�����Ȃ��B��̏����������ƃJ�������߂��Ƃ�����ł��������������Ă��܂��̂�
		&& Node.LOD > 0) // LOD0�̂��̂͂���ȏ㕪�����Ȃ�
	{
		// LOD��childListIndices�͏����l�ʂ�
		FQuadNode ChildNodeBottomRight;
		ChildNodeBottomRight.BottomRight = Node.BottomRight;
		ChildNodeBottomRight.Length = Node.Length / 2.0f;
		ChildNodeBottomRight.LOD = Node.LOD - 1;
		Node.ChildNodeIndices[0] = BuildQuadtreeRecursively(MaxLOD, NumRowColumn, MaxScreenCoverage, PatchLength, CameraRayIntersectionPoint, ViewProjectionMatrix, ChildNodeBottomRight, OutQuadNodeList);

		FQuadNode ChildNodeBottomLeft;
		ChildNodeBottomLeft.BottomRight = Node.BottomRight + FVector2D(Node.Length / 2.0f, 0.0f);
		ChildNodeBottomLeft.Length = Node.Length / 2.0f;
		ChildNodeBottomLeft.LOD = Node.LOD - 1;
		Node.ChildNodeIndices[1] = BuildQuadtreeRecursively(MaxLOD, NumRowColumn, MaxScreenCoverage, PatchLength, CameraRayIntersectionPoint, ViewProjectionMatrix, ChildNodeBottomLeft, OutQuadNodeList);

		FQuadNode ChildNodeTopRight;
		ChildNodeTopRight.BottomRight = Node.BottomRight + FVector2D(0.0f, Node.Length / 2.0f);
		ChildNodeTopRight.Length = Node.Length / 2.0f;
		ChildNodeTopRight.LOD = Node.LOD - 1;
		Node.ChildNodeIndices[2] = BuildQuadtreeRecursively(MaxLOD, NumRowColumn, MaxScreenCoverage, PatchLength, CameraRayIntersectionPoint, ViewProjectionMatrix, ChildNodeTopRight, OutQuadNodeList);

		FQuadNode ChildNodeTopLeft;
		ChildNodeTopLeft.BottomRight = Node.BottomRight + FVector2D(Node.Length / 2.0f, Node.Length / 2.0f);
		ChildNodeTopLeft.Length = Node.Length / 2.0f;
		ChildNodeTopLeft.LOD = Node.LOD - 1;
		Node.ChildNodeIndices[3] = BuildQuadtreeRecursively(MaxLOD, NumRowColumn, MaxScreenCoverage, PatchLength, CameraRayIntersectionPoint, ViewProjectionMatrix, ChildNodeTopLeft, OutQuadNodeList);

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
void BuildQuadtree(int32 MaxLOD, int32 NumRowColumn, float MaxScreenCoverage, float PatchLength, const FVector& CameraRayIntersectionPoint, const FMatrix& ViewProjectionMatrix, FQuadNode& RootNode, TArray<FQuadNode>& OutAllQuadNodeList, TArray<FQuadNode>& OutRenderQuadNodeList)
{
	BuildQuadtreeRecursively(MaxLOD, NumRowColumn, MaxScreenCoverage, PatchLength, CameraRayIntersectionPoint, ViewProjectionMatrix, RootNode, OutAllQuadNodeList);

	for (const FQuadNode& Node : OutAllQuadNodeList)
	{
		if (Node.IsLeaf())
		{
			OutRenderQuadNodeList.Add(Node);
		}
	}
}
} // namespace Quadtree

