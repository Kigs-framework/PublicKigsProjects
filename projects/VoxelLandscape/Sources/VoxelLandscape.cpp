#include <VoxelLandscape.h>
#include <FilePathManager.h>
#include <NotificationCenter.h>
#include "VOctree.h"
#include "simplexnoise.h"
#include "Camera.h"


IMPLEMENT_CLASS_INFO(VoxelLandscape);

IMPLEMENT_CONSTRUCTOR(VoxelLandscape)
{

}

void	VoxelLandscape::ProtectedInit()
{
	// Base modules have been created at this point

	// lets say that the update will sleep 1ms
	SetUpdateSleepTime(1);

	SP<FilePathManager>& pathManager = KigsCore::Singleton<FilePathManager>();
	pathManager->AddToPath(".", "xml");


	// Load AppInit, GlobalConfig then launch first sequence
	DataDrivenBaseApplication::ProtectedInit();

	DECLARE_FULL_CLASS_INFO(KigsCore::Instance(), VOctree, VOctree, VoxelLandscape);
}

void	VoxelLandscape::ProtectedUpdate()
{
	DataDrivenBaseApplication::ProtectedUpdate();
}

void	VoxelLandscape::ProtectedClose()
{
	DataDrivenBaseApplication::ProtectedClose();
}

void	VoxelLandscape::ProtectedInitSequence(const kstl::string& sequence)
{
	if (sequence == "sequencemain")
	{
		initLandscape();

#ifdef _DEBUG
		mVOctree->printAllocatedNodeCount();
#endif

		printf("Octree max depth = %d\n", mVOctree->getMaxDepth());

		v3i startingPos = getEmptyNode({ 256,256,256 });
		nodeInfo startingNode =mVOctree->getVoxelAt(startingPos);

		std::vector<CMSP> cams = CoreModifiable::GetInstancesByName("Node3D", "Camera",true);
		SP<Camera> cam = nullptr;
		if (cams.size())
		{
			cam = cams[0];
			cam->globalMove({ (float)startingPos.x,(float)startingPos.y,(float)startingPos.z });
		}

		std::vector<nodeInfo> visible=mVOctree->getVisibleCubeList(startingNode, *cam.get());

		printf("visible cube found = %d\n", visible.size());
	}
}
void	VoxelLandscape::ProtectedCloseSequence(const kstl::string& sequence)
{
	if (sequence == "sequencemain")
	{
		
	}
}

v3i		VoxelLandscape::getEmptyNode(const v3i& startingPos,int maxTry)
{
	int maxD=mVOctree->getValue<int>("MaxDepth");
	int maxCoord = (2 << maxD);
	
	int dist = 0;

	do
	{
		for (int i = 0; i < (dist/2+1); i++)
		{

			v3i testedPos = startingPos;
			testedPos.x += (rand() % (2 * dist + 1)) - dist;
			testedPos.y += (rand() % (2 * dist + 1)) - dist;
			testedPos.z += (rand() % (2 * dist + 1)) - dist;

			auto boundPos = [](int& p, int maxC) {
				if (p < 0)
				{
					p = 0;
				}
				else if (p >= maxC)
				{
					p = maxC - 1;
				}
			};

			boundPos(testedPos.x, maxCoord);
			boundPos(testedPos.y, maxCoord);
			boundPos(testedPos.z, maxCoord);
			nodeInfo tst = mVOctree->getVoxelAt(testedPos);

			if (tst.node)
			{
				if (tst.getNode<VOctreeNode>()->getContentType().getContent() == 0)
				{
					return tst.coord;
				}
			}

			maxTry--;
		}

		dist+=2;

	} while (maxTry);

	
	return { -1,-1,-1 };

}
void	VoxelLandscape::initLandscape()
{

	const int depth = 8;

	mVOctree = KigsCore::GetInstanceOf("mVOctree", "VOctree");
	mVOctree->setValue("MaxDepth", depth);
	mVOctree->Init();

	const int zonesize = 1 << depth;

	int i, j;
	
	for (j = 0; j < zonesize; j++)
	{
		printf("noise filling line %d/%d \n", j, zonesize);
		for (i = 0; i < zonesize; i++)
		{

			int noise =(int) scaled_octave_noise_3d(6.0f, 0.2f, 0.001f, 0.0f, 255.0f, (float)i, (float)j, 0.0f);

			/*for (k = 0; k < noise; k++)
			{
				v3i	coords(i * 2 + 1, j * 2 + 1, k * 2 + 1);
				mVOctree->setVoxelContent(coords, 1);
			}*/
			{
				v3i	coords(i * 2 + 1, j * 2 + 1, noise * 2 + 1);
				mVOctree->setVoxelContent(coords, 1);
			}

		}
	}
	
}