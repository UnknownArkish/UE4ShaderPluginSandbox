#include "Quadtree.h"
#include "SceneView.h"

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

// �t���X�^���J�����O�B�r���[�t���X�^���̒���QuadNode���ꕔ�ł������Ă�����true�B�����łȂ����false�B
bool IsQuadNodeFrustumCulled(const Quadtree::FQuadNode& Node, const FSceneView& View)
{
	//FSceneView::ProjectWorldToScreen()���Q�l�ɂ��Ă���
	const FMatrix& ViewProjMat = View.ViewMatrices.GetViewProjectionMatrix();

	// �R�[�i�[�̂��ׂĂ̓_��NDC�i���K���f�o�C�X���W�n�j�ł�[-1,1]�̃L���[�u�ɓ����ĂȂ���΃J�����O�Ώ�

	const FVector4& BottomLeftProjSpace = ViewProjMat.TransformFVector4(FVector4(Node.BottomLeft.X, Node.BottomLeft.Y, 0.0f, 1.0f));
	if (IsInNDCCube(BottomLeftProjSpace))
	{
		return false;
	}

	const FVector4& BottomRightProjSpace = ViewProjMat.TransformFVector4(FVector4(Node.BottomLeft.X + Node.Length, Node.BottomLeft.Y, 0.0f, 1.0f));
	if (IsInNDCCube(BottomRightProjSpace))
	{
		return false;
	}

	const FVector4& TopLeftProjSpace = ViewProjMat.TransformFVector4(FVector4(Node.BottomLeft.X, Node.BottomLeft.Y + Node.Length, 0.0f, 1.0f));
	if (IsInNDCCube(TopLeftProjSpace))
	{
		return false;
	}

	const FVector4& TopRightProjSpace = ViewProjMat.TransformFVector4(FVector4(Node.BottomLeft.X + Node.Length, Node.BottomLeft.Y + Node.Length, 0.0f, 1.0f));
	if (IsInNDCCube(TopRightProjSpace))
	{
		return false;
	}

	return true;
}

// QuadNode�̃��b�V���̃O���b�h�̒��ł����Ƃ��J�����ɋ߂����̂̃X�N���[���\���ʐϗ����擾����
float EstimateGridScreenCoverage(const Quadtree::FQuadNode& Node, const FSceneView& View)
{
	return 0.0f;
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

int32 BuildQuadNodeRenderListRecursively(float MaxScreenCoverage, float PatchLength, FQuadNode& Node, const ::FSceneView& View, TArray<FQuadNode>& OutRenderList)
{
	bool bCulled = IsQuadNodeFrustumCulled(Node, View);
	if (bCulled)
	{
		return INDEX_NONE;
	}

	// QuadNode�ɋ��e����X�N���[���J�o���b�W�䗦�̏���B
	//float MaxScreenCoverage = MaxPixelCoverage / View.SceneViewInitOptions.GetConstrainedViewRect().Size().X / View.SceneViewInitOptions.GetConstrainedViewRect().Size().Y;

	// QuadNode�̑S�O���b�h�̃X�N���[���J�o���b�W�䗦�̒��ōő�̂��́B
	float GridCoverage = EstimateGridScreenCoverage(Node, View);

	bool bVisible = true;
	if (GridCoverage > MaxScreenCoverage // �O���b�h���X�N���[���J�o���b�W������傫����Ύq�ɕ�������B������`�悵�Ȃ��̂̓��X�g�̎g�p����IsLeaf�Ŕ��肷��B
		&& Node.Length > PatchLength) // �p�b�`�T�C�Y�ȉ��̏c�������ł���΂���ȏ㕪�����Ȃ��B��̏����������ƃJ�������߂��Ƃ�����ł��������������Ă��܂��̂�
	{
		// LOD��childListIndices�͏����l�ʂ�
		FQuadNode ChildNodeBottomLeft;
		ChildNodeBottomLeft.BottomLeft = Node.BottomLeft;
		ChildNodeBottomLeft.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[0] = BuildQuadNodeRenderListRecursively(MaxScreenCoverage, PatchLength, ChildNodeBottomLeft, View, OutRenderList);

		FQuadNode ChildNodeBottomRight;
		ChildNodeBottomRight.BottomLeft = Node.BottomLeft + FVector2D(Node.Length / 2.0f, 0.0f);
		ChildNodeBottomRight.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[1] = BuildQuadNodeRenderListRecursively(MaxScreenCoverage, PatchLength, ChildNodeBottomRight, View, OutRenderList);

		FQuadNode ChildNodeTopLeft;
		ChildNodeTopLeft.BottomLeft = Node.BottomLeft + FVector2D(0.0f, Node.Length / 2.0f);
		ChildNodeTopLeft.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[2] = BuildQuadNodeRenderListRecursively(MaxScreenCoverage, PatchLength, ChildNodeTopLeft, View, OutRenderList);

		FQuadNode ChildNodeTopRight;
		ChildNodeTopRight.BottomLeft = Node.BottomLeft + FVector2D(Node.Length / 2.0f, Node.Length / 2.0f);
		ChildNodeTopRight.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[3] = BuildQuadNodeRenderListRecursively(MaxScreenCoverage, PatchLength, ChildNodeTopRight, View, OutRenderList);

		// ���ׂĂ̎q�m�[�h���t���X�^���J�����O�Ώۂ�������A�������s���Ȃ͂��ŁA�J�����O�v�Z�ŘR�ꂽ�Ƃ݂�ׂ��Ȃ̂ŃJ�����O����
		bVisible = !IsLeaf(Node);
	}

	if (bVisible)
	{
		// TODO:�����̌v�Z���֐����������ȁB�B�B
		// ���b�V����LOD���X�N���[���J�o���b�W���猈�߂�
		// LOD���x�����オ�邲�Ƃɏc��2�{����̂ŁA�O���b�h�J�o���b�W������l��1/4^x�ɂȂ��Ă����x��LOD���x���ł���
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


