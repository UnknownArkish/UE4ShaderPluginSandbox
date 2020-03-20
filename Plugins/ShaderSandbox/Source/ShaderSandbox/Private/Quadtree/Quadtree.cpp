#include "Quadtree.h"
#include "SceneView.h"

namespace
{
// �t���X�^���J�����O�B�r���[�t���X�^���̒���QuadNode���ꕔ�ł������Ă�����true�B�����łȂ����false�B
bool IsQuadNodeFrustumCulled(const Quadtree::FQuadNode& Node, const FSceneView& View)
{
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

int32 BuildQuadNodeRenderListRecursively(FQuadNode& Node, const ::FSceneView& View, TArray<FQuadNode>& OutRenderList)
{
	bool bCulled = IsQuadNodeFrustumCulled(Node, View);
	if (bCulled)
	{
		return INDEX_NONE;
	}

	// TODO:�c��8�s�N�Z���̃X�N���[���J�o���b�W�䗦������B����ȏ�ł���΃O���b�h���傫������̂Ŏq�ɕ�������B�}�W�b�N�i���o�[
	// TODO:�{���͂��̔䗦�������œn���ׂ��B�ċA�̂��тɌv�Z���Ă���
	float MaxCoverage = 8 / View.SceneViewInitOptions.GetConstrainedViewRect().Size().X * 8 / View.SceneViewInitOptions.GetConstrainedViewRect().Size().Y;
	float GridCoverage = EstimateGridScreenCoverage(Node, View);

	float PatchLength = 2000; // TODO:����������������ŗ^����ׂ�
	bool bVisible = true;
	if (GridCoverage > MaxCoverage && Node.Length > PatchLength)
	{
		// LOD��childListIndices�͏����l�ʂ�
		FQuadNode ChildNodeBottomLeft;
		ChildNodeBottomLeft.BottomLeft = Node.BottomLeft;
		ChildNodeBottomLeft.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[0] = BuildQuadNodeRenderListRecursively(ChildNodeBottomLeft, View, OutRenderList);

		FQuadNode ChildNodeBottomRight;
		ChildNodeBottomRight.BottomLeft = Node.BottomLeft + FVector2D(Node.Length / 2.0f, 0.0f);
		ChildNodeBottomRight.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[1] = BuildQuadNodeRenderListRecursively(ChildNodeBottomRight, View, OutRenderList);

		FQuadNode ChildNodeTopLeft;
		ChildNodeTopLeft.BottomLeft = Node.BottomLeft + FVector2D(0.0f, Node.Length / 2.0f);
		ChildNodeTopLeft.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[2] = BuildQuadNodeRenderListRecursively(ChildNodeTopLeft, View, OutRenderList);

		FQuadNode ChildNodeTopRight;
		ChildNodeTopRight.BottomLeft = Node.BottomLeft + FVector2D(Node.Length / 2.0f, Node.Length / 2.0f);
		ChildNodeTopRight.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[3] = BuildQuadNodeRenderListRecursively(ChildNodeTopRight, View, OutRenderList);

		bVisible = !IsLeaf(Node);
	}

	if (bVisible)
	{
		// ���b�V����LOD���X�N���[���J�o���b�W���猈�߂�
		// LOD���x�����オ�邲�Ƃɏc��2�{����̂ŁA�O���b�h�J�o���b�W������l��1/4^x�ɂȂ��Ă����x��LOD���x���ł���
		int32 LOD = 0;
		for (; LOD < 8 - 1; LOD++)  //TODO:�ő�LOD���x��8���}�W�b�N�i���o�[
		{
			if (GridCoverage > MaxCoverage)
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


