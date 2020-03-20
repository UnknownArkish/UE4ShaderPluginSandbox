#include "Quadtree.h"

namespace
{
// �t���X�^���J�����O�B�J�����t���X�^���̒���QuadNode���ꕔ�ł������Ă�����true�B�����łȂ����false�B
bool IsQuadNodeInCameraFrustum(const Quadtree::FQuadNode& Node, const class FSceneView& View)
{
	return false;
}

// QuadNode�̃��b�V���̃O���b�h�̒��ł����Ƃ��J�����ɋ߂����̂̃X�N���[���\���ʐϗ����擾����
float EstimateGridScreenCoverage(const Quadtree::FQuadNode& Node, const class FSceneView& View)
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

int32 BuildQuadNodeRenderListRecursively(FQuadNode& Node, const class FSceneView& View, TArray<FQuadNode>& OutRenderList)
{
	return INDEX_NONE;
}
} // namespace Quadtree

