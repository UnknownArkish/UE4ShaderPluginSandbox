#include "Quadtree.h"

namespace
{
bool IsInNDCCube(const FVector4& PositionProjectionSpace)
{
	if (PositionProjectionSpace.W > KINDA_SMALL_NUMBER)
	{
		const FVector4& PositionNDC = PositionProjectionSpace / PositionProjectionSpace.W;
		if (PositionNDC.X > -1.0f && PositionNDC.X < 1.0f
			&& PositionNDC.Y > -1.0f && PositionNDC.Y < 1.0f
			&& PositionNDC.Z > -1.0f && PositionNDC.Z < 1.0f)
		{
			return true;
		}
	}

	return false;
}
} // namespace

namespace Quadtree
{
// �t���X�^���J�����O�B�r���[�t���X�^���̒���QuadNode���ꕔ�ł������Ă�����true�B�����łȂ����false�B
bool IsQuadNodeFrustumCulled(const FMatrix& ViewProjectionMatrix, const Quadtree::FQuadNode& Node)
{
	//FSceneView::ProjectWorldToScreen()���Q�l�ɂ��Ă���

	// NDC�ւ̕ϊ���W�ŏ��Z�������̂�W��0�ɋ߂��ꍇ�̓J�����O�ΏۂƂ��Ĉ���
	const FVector4& BottomLeftProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(Node.BottomLeft.X, Node.BottomLeft.Y, 0.0f, 1.0f));
	if (BottomLeftProjSpace.W < KINDA_SMALL_NUMBER)
	{
		return true;
	}
	
	const FVector4& BottomRightProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(Node.BottomLeft.X + Node.Length, Node.BottomLeft.Y, 0.0f, 1.0f));
	if (BottomRightProjSpace.W < KINDA_SMALL_NUMBER)
	{
		return true;
	}

	const FVector4& TopLeftProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(Node.BottomLeft.X, Node.BottomLeft.Y + Node.Length, 0.0f, 1.0f));
	if (TopLeftProjSpace.W < KINDA_SMALL_NUMBER)
	{
		return true;
	}

	const FVector4& TopRightProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(Node.BottomLeft.X + Node.Length, Node.BottomLeft.Y + Node.Length, 0.0f, 1.0f));
	if (TopRightProjSpace.W < KINDA_SMALL_NUMBER)
	{
		return true;
	}

	const FVector4& BottomLeftNDC = BottomLeftProjSpace / BottomLeftProjSpace.W;
	const FVector4& BottomRightNDC = BottomRightProjSpace / BottomRightProjSpace.W;
	const FVector4& TopLeftNDC = TopLeftProjSpace / TopLeftProjSpace.W;
	const FVector4& TopRightNDC = TopRightProjSpace / TopRightProjSpace.W;

	// �t���X�^���O�ɂ���Ƃ��́A4�R�[�i�[�����ׂāANDC�L���[�u��6�ʂ̂��������ꂩ�̖ʂ̊O�ɂ���Ƃ��ɂȂ��Ă���
	if (BottomLeftNDC.X < -1.0f && BottomRightNDC.X < -1.0f && TopLeftNDC.X < -1.0f && TopRightNDC.X < -1.0f)
	{
		return true;
	}
	if (BottomLeftNDC.X > 1.0f && BottomRightNDC.X > 1.0f && TopLeftNDC.X > 1.0f && TopRightNDC.X > 1.0f)
	{
		return true;
	}
	if (BottomLeftNDC.Y < -1.0f && BottomRightNDC.Y < -1.0f && TopLeftNDC.Y < -1.0f && TopRightNDC.Y < -1.0f)
	{
		return true;
	}
	if (BottomLeftNDC.Y > 1.0f && BottomRightNDC.Y > 1.0f && TopLeftNDC.Y > 1.0f && TopRightNDC.Y > 1.0f)
	{
		return true;
	}
	if (BottomLeftNDC.Z < -1.0f && BottomRightNDC.Z < -1.0f && TopLeftNDC.Z < -1.0f && TopRightNDC.Z < -1.0f)
	{
		return true;
	}
	if (BottomLeftNDC.Z > 1.0f && BottomRightNDC.Z > 1.0f && TopLeftNDC.Z > 1.0f && TopRightNDC.Z > 1.0f)
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
	FVector2D NearestGridBottomLeft;

	// �J�����̐^���ɃO���b�h������΂���B�Ȃ����Clamp����
	int32 Row = FMath::Clamp<int32>((CameraPosition.X - Node.BottomLeft.X) / GridLength, 0, NumRowColumn - 1); // float��int32�L���X�g�Ő؂�̂�
	int32 Column = FMath::Clamp<int32>((CameraPosition.Y - Node.BottomLeft.Y) / GridLength, 0, NumRowColumn - 1); // float��int32�L���X�g�Ő؂�̂�
	OutNearestGrid = FIntPoint(Row, Column);
	NearestGridBottomLeft.X = Node.BottomLeft.X + Row * GridLength;
	NearestGridBottomLeft.Y = Node.BottomLeft.Y + Column * GridLength;

	// �ŋߐڃO���b�h�̃X�N���[���ł̕\���ʐϗ��v�Z
	const FVector4& NearestGridBottomLeftProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(NearestGridBottomLeft.X, NearestGridBottomLeft.Y, 0.0f, 1.0f));
	const FVector4& NearestGridBottomRightProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(NearestGridBottomLeft.X + GridLength, NearestGridBottomLeft.Y, 0.0f, 1.0f));
	const FVector4& NearestGridTopLeftProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(NearestGridBottomLeft.X, NearestGridBottomLeft.Y + GridLength, 0.0f, 1.0f));
	const FVector4& NearestGridTopRightProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(NearestGridBottomLeft.X + GridLength, NearestGridBottomLeft.Y + GridLength, 0.0f, 1.0f));

	const FVector2D& NearestGridBottomLeftNDC = FVector2D(NearestGridBottomLeftProjSpace) / NearestGridBottomLeftProjSpace.W;
	const FVector2D& NearestGridBottomRightNDC = FVector2D(NearestGridBottomRightProjSpace) / NearestGridBottomRightProjSpace.W;
	const FVector2D& NearestGridTopLeftNDC = FVector2D(NearestGridTopLeftProjSpace) / NearestGridTopLeftProjSpace.W;

	// �ʐς����߂�ɂ̓w�����̌������g���̂����������ASqrt�̏�������������̂ŏ������ׂ̂��߁A�����悻���s�l�ӌ`�ƂƂ炦�Ėʐς��v�Z����B
	// ���Ƃ�2�悷��̂ŕ��ς��Ƃ�Ƃ��ɐ����̕����͖����B
	const FVector2D& HorizEdge = NearestGridBottomRightNDC - NearestGridBottomLeftNDC;
	const FVector2D& VertEdge = NearestGridTopLeftNDC - NearestGridBottomLeftNDC;

	// �O���b�h�̃C���f�b�N�X�������������X��Y��������UE4�͍���n�Ȃ̂ŁA�J������Z�����ɂ���Ƃ����O��Ȃ�O�ς̕����͔��]���������̂����s�l�ӌ`�̖ʐςɂȂ�
	// NDC��[-1,1]�Ȃ̂�XY�ʂ��ʐς�4�Ȃ̂ŁA�\���ʐϗ��Ƃ��Ă�1/4����Z�������̂ɂȂ�
	float Ret = -(HorizEdge ^ VertEdge) * 0.25f;
	return Ret;
}

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

int32 BuildQuadNodeRenderListRecursively(int32 NumRowColumn, float MaxScreenCoverage, float PatchLength, const FVector& CameraPosition, const FMatrix& ViewProjectionMatrix, FQuadNode& Node, TArray<FQuadNode>& OutRenderList)
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
		FQuadNode ChildNodeBottomLeft;
		ChildNodeBottomLeft.BottomLeft = Node.BottomLeft;
		ChildNodeBottomLeft.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[0] = BuildQuadNodeRenderListRecursively(NumRowColumn, MaxScreenCoverage, PatchLength, CameraPosition, ViewProjectionMatrix, ChildNodeBottomLeft, OutRenderList);

		FQuadNode ChildNodeBottomRight;
		ChildNodeBottomRight.BottomLeft = Node.BottomLeft + FVector2D(Node.Length / 2.0f, 0.0f);
		ChildNodeBottomRight.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[1] = BuildQuadNodeRenderListRecursively(NumRowColumn, MaxScreenCoverage, PatchLength, CameraPosition, ViewProjectionMatrix, ChildNodeBottomRight, OutRenderList);

		FQuadNode ChildNodeTopLeft;
		ChildNodeTopLeft.BottomLeft = Node.BottomLeft + FVector2D(0.0f, Node.Length / 2.0f);
		ChildNodeTopLeft.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[2] = BuildQuadNodeRenderListRecursively(NumRowColumn, MaxScreenCoverage, PatchLength, CameraPosition, ViewProjectionMatrix, ChildNodeTopLeft, OutRenderList);

		FQuadNode ChildNodeTopRight;
		ChildNodeTopRight.BottomLeft = Node.BottomLeft + FVector2D(Node.Length / 2.0f, Node.Length / 2.0f);
		ChildNodeTopRight.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[3] = BuildQuadNodeRenderListRecursively(NumRowColumn, MaxScreenCoverage, PatchLength, CameraPosition, ViewProjectionMatrix, ChildNodeTopRight, OutRenderList);

		// ���ׂĂ̎q�m�[�h���t���X�^���J�����O�Ώۂ�������A�������s���Ȃ͂��ŁA�J�����O�v�Z�ŘR�ꂽ�Ƃ݂�ׂ��Ȃ̂ŃJ�����O����
		bVisible = !IsLeaf(Node);
	}

	if (bVisible)
	{
		// TODO:�����̌v�Z���֐����������ȁB�B�B
		// ���b�V����LOD���X�N���[���\���ʐϗ����猈�߂�
		// LOD���x�����オ�邲�Ƃɏc��2�{����̂ŁA�O���b�h�̃X�N���[���\���ʐϗ�������l��1/4^x�ɂȂ��Ă����x��LOD���x���ł���
		int32 LOD = 0;
		for (; LOD < 8 - 1; LOD++)  //TODO:�ő�LOD���x��8���}�W�b�N�i���o�[
		{
			if (GridCoverage > MaxScreenCoverage)
			{
				break;
			}

			GridCoverage *= 4;
		}

		// TODO:LOD�̍ő僌�x���ƁA������ЂƂ��̃��x���͎g��Ȃ��B���Ƃōl������
		Node.LOD = FMath::Min(LOD, 8 - 2);
	}
	else
	{
		return INDEX_NONE;
	}

	int32 Index = OutRenderList.Add(Node);
	return Index;
}
} // namespace Quadtree


