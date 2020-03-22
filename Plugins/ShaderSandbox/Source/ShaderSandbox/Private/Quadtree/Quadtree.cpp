#include "Quadtree.h"

namespace
{
// �t���X�^���J�����O�B�r���[�t���X�^���̒���QuadNode���ꕔ�ł������Ă�����true�B�����łȂ����false�B
bool IsQuadNodeFrustumCulled(const FMatrix& ViewProjectionMatrix, const Quadtree::FQuadNode& Node)
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
	if (BottomRightNDC.Z < -1.0f && BottomLeftNDC.Z < -1.0f && TopRightNDC.Z < -1.0f && TopLeftNDC.Z < -1.0f)
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
float EstimateGridScreenCoverage(int32 NumRowColumn, const FVector& CameraPosition, const FMatrix& ViewProjectionMatrix, const Quadtree::FQuadNode& Node, FIntPoint& OutNearestGrid)
{
	// �\���ʐς��ő�̃O���b�h�𒲂ׂ����̂ŁA�J�����ɍł��߂��O���b�h�𒲂ׂ�B
	// �O���b�h�̒��ɂ͕\������Ă��Ȃ����̂����肤�邪�A�����ɓn�����QuadNode�̓t���X�^���J�����O�͂���ĂȂ��O��Ȃ̂�
	// �J�����ɍł��߂��O���b�h�͕\������Ă���͂��B

	// QuadNode���b�V���̃O���b�h�ւ̏c���������͓����ł���A���b�V���͐����`�ł���Ƃ����O�񂪂���
	float GridLength = Node.Length / NumRowColumn;
	FVector2D NearestGridBottomRight;

	// �J�����̐^���ɃO���b�h������΂���B�Ȃ����Clamp����
	int32 Row = FMath::Clamp<int32>((CameraPosition.X - Node.BottomRight.X) / GridLength, 0, NumRowColumn - 1); // float��int32�L���X�g�Ő؂�̂�
	int32 Column = FMath::Clamp<int32>((CameraPosition.Y - Node.BottomRight.Y) / GridLength, 0, NumRowColumn - 1); // float��int32�L���X�g�Ő؂�̂�
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
} // namespace

namespace Quadtree
{
bool IsLeaf(const FQuadNode& Node)
{
	// ChildNodeIndices[]�łЂƂł��L���ȃC���f�b�N�X�̂��̂����邩�ǂ�����Leaf���ǂ������肵�Ă���
	for (int32 i = 0; i < 4; i++)
	{
		if (Node.ChildNodeIndices[i] != INDEX_NONE)
		{
			return false;
		}
	}

	return true;
}

int32 BuildQuadNodeRenderListRecursively(int32 MaxLOD, int32 NumRowColumn, float MaxScreenCoverage, float PatchLength, const FVector& CameraPosition, const FMatrix& ViewProjectionMatrix, FQuadNode& Node, TArray<FQuadNode>& OutRenderList)
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
	float GridCoverage = EstimateGridScreenCoverage(NumRowColumn, CameraPosition, ViewProjectionMatrix, Node, NearestGrid);

	bool bVisible = true;
	if (GridCoverage > MaxScreenCoverage // �O���b�h���\���ʐϗ�������傫����Ύq�ɕ�������B������`�悵�Ȃ��̂̓��X�g�̎g�p����IsLeaf�Ŕ��肷��B
		&& Node.Length > PatchLength) // �p�b�`�T�C�Y�ȉ��̏c�������ł���΂���ȏ㕪�����Ȃ��B��̏����������ƃJ�������߂��Ƃ�����ł��������������Ă��܂��̂�
	{
		// LOD��childListIndices�͏����l�ʂ�
		FQuadNode ChildNodeBottomRight;
		ChildNodeBottomRight.BottomRight = Node.BottomRight;
		ChildNodeBottomRight.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[0] = BuildQuadNodeRenderListRecursively(MaxLOD, NumRowColumn, MaxScreenCoverage, PatchLength, CameraPosition, ViewProjectionMatrix, ChildNodeBottomRight, OutRenderList);

		FQuadNode ChildNodeBottomLeft;
		ChildNodeBottomLeft.BottomRight = Node.BottomRight + FVector2D(Node.Length / 2.0f, 0.0f);
		ChildNodeBottomLeft.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[1] = BuildQuadNodeRenderListRecursively(MaxLOD, NumRowColumn, MaxScreenCoverage, PatchLength, CameraPosition, ViewProjectionMatrix, ChildNodeBottomLeft, OutRenderList);

		FQuadNode ChildNodeTopRight;
		ChildNodeTopRight.BottomRight = Node.BottomRight + FVector2D(0.0f, Node.Length / 2.0f);
		ChildNodeTopRight.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[2] = BuildQuadNodeRenderListRecursively(MaxLOD, NumRowColumn, MaxScreenCoverage, PatchLength, CameraPosition, ViewProjectionMatrix, ChildNodeTopRight, OutRenderList);

		FQuadNode ChildNodeTopLeft;
		ChildNodeTopLeft.BottomRight = Node.BottomRight + FVector2D(Node.Length / 2.0f, Node.Length / 2.0f);
		ChildNodeTopLeft.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[3] = BuildQuadNodeRenderListRecursively(MaxLOD, NumRowColumn, MaxScreenCoverage, PatchLength, CameraPosition, ViewProjectionMatrix, ChildNodeTopLeft, OutRenderList);

		// ���ׂĂ̎q�m�[�h���t���X�^���J�����O�Ώۂ�������A�������s���Ȃ͂��ŁA�J�����O�v�Z�ŘR�ꂽ�Ƃ݂�ׂ��Ȃ̂ŃJ�����O����
		bVisible = !IsLeaf(Node);
	}

	if (bVisible)
	{
		// TODO:�����̌v�Z���֐����������ȁB�B�B
		// ���b�V����LOD���X�N���[���\���ʐϗ����猈�߂�
		// LOD���x�����オ�邲�Ƃɏc��2�{����̂ŁA�O���b�h�̃X�N���[���\���ʐϗ�������l��1/4^x�ɂȂ��Ă����x��LOD���x���ł���
		int32 LOD = 0;
		for (; LOD < MaxLOD - 1; LOD++)  //TODO:�ő�LOD���x��8���}�W�b�N�i���o�[
		{
			if (GridCoverage > MaxScreenCoverage)
			{
				break;
			}

			GridCoverage *= 4;
		}

		// TODO:LOD�̍ő僌�x���ƁA������ЂƂ��̃��x���͎g��Ȃ��B���Ƃōl������
		Node.LOD = FMath::Max(FMath::Min(LOD, MaxLOD - 2), 0);
	}
	else
	{
		return INDEX_NONE;
	}

	int32 Index = OutRenderList.Add(Node);
	return Index;
}
} // namespace Quadtree


