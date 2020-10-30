#pragma once

#include "CoreModifiable.h"
#include "CoreModifiableAttribute.h"
#include "Camera.h"
#include "OctreeBase.h"


class VoxelOctreeContent 
{
public:
	VoxelOctreeContent(){}
	~VoxelOctreeContent() {}

	void	setContent(unsigned int c)
	{
		mContent = c;
	}

	unsigned int	getContent()
	{
		return mContent;
	}

	operator unsigned int() const
	{
		return mContent;
	}

	void	setVisibilityFlag(unsigned int f)
	{
		mVisibilityFlag = f;
	}
	unsigned int	getVisibilityFlag() const
	{
		return mVisibilityFlag;
	}

	static VoxelOctreeContent canCollapse(OctreeNode<VoxelOctreeContent>* children,bool& doCollapse);

	bool	canOnlyBeSetOnLeaf() const
	{
		return true;
	}

protected:
	unsigned int	mContent;
	unsigned int	mVisibilityFlag;

private:

};
class VOctreeNode : public OctreeNode<VoxelOctreeContent>
{
public:
	VOctreeNode() : OctreeNode<VoxelOctreeContent>()
	{

	}

	VOctreeNode(unsigned int c) : OctreeNode<VoxelOctreeContent>()
	{
		mContentType.setContent(c);
	}

	void	setContent(unsigned int c)
	{
		mContentType.setContent(c);
	}

private:

};



typedef	nodeInfo	VNodeInfo;

// Octree minimal subdivision is 2 units wide, so that the center of each cubic node can be defined using v3i 

class VOctree : public CoreModifiable
{
public:
	DECLARE_CLASS_INFO(VOctree, CoreModifiable, Core);
	DECLARE_CONSTRUCTOR(VOctree);

	
	// change content type of a voxel given it's coordinates in Octree reference
	void			setVoxelContent(const v3i& coordinate, unsigned int content);
	unsigned int	getVoxelContent(const v3i& coordinate,unsigned int maxLevel=(unsigned int)-1);

	VNodeInfo getVoxelAt(const v3i& coordinate, unsigned int maxDepth = (unsigned int)-1);
	std::vector<VNodeInfo>	getVisibleCubeList(VNodeInfo& startPos, Camera& camera);

#ifdef _DEBUG
	void	printAllocatedNodeCount()
	{
		printf("allocated nodes : %d\n", VOctreeNode::getCurrentAllocatedNodeCount());
	}
#endif

	// return max depth in octree
	unsigned int getMaxDepth();

protected:

	// get neighbour in the given direction ( as an index in mNeightboursDecalVectors)   
	VNodeInfo	getVoxelNeighbour(const VNodeInfo& node,int dir);

	// utility class to avoid passing the same parameters to the recursive method
	// and mutualise some computation 
	class recurseVoxelSideChildren
	{
	public:

		recurseVoxelSideChildren()
		{

		}

		recurseVoxelSideChildren(int dir,VOctree& octree, std::vector<VNodeInfo>* child)
		{
			reset(dir, octree,child);
		}
		~recurseVoxelSideChildren() {}

		void run(const VNodeInfo& node);

		void	reset(int dir, VOctree& octree,std::vector<VNodeInfo>* child)
		{
			mMaxDepth = octree.mMaxDepth;
			mChildList = child;
			mDir = dir;
			mMaskTest = 1 << (mDir / 2);
			mMaskResult = (dir & 1) * mMaskTest;
		}

	private:
		unsigned int mMaskTest;
		unsigned int mMaskResult;
		int			 mMaxDepth;

		int							mDir;
		std::vector<VNodeInfo>*		mChildList;
	};

	void	recurseFloodFill(const VNodeInfo& startPos, std::vector<VNodeInfo>& notEmptyList);

	// utility class to avoid passing the same parameters to the recursive method
	// and mutualise some computation 
	class recurseOrientedFloodFill
	{
	public:
		recurseOrientedFloodFill(VOctree& octree,std::vector<VNodeInfo>* notEmptyList, const v3f& viewVector, const v3f& viewPos, const float fieldOfView) :
			mOctree(octree)
			, mNotEmptyList(notEmptyList)
			, mViewVector(viewVector)
			, mViewPos(viewPos)
			, mSinFieldOfView(sinf(fieldOfView))
		{
		}
		~recurseOrientedFloodFill() {}

		void run(VNodeInfo& node);

		void	reset(int dir)
		{
			
		}
	private:
		v3f		mViewVector;
		v3f		mViewPos;
		float	mSinFieldOfView;

		VOctree& mOctree;

		std::vector<VNodeInfo>*		mNotEmptyList;
		std::vector< VNodeInfo>		mChildToTreat;
		recurseVoxelSideChildren	mSideChildrenGrabber;


	};
	
	// return true if currentNode parent needs to be changed
	bool	recursiveSetVoxelContent(VOctreeNode* currentNode,const v3i& coordinate, unsigned int content, int currentDepth);


	// set coord to real cube center
	// minimal cube is 2 units wide
	void	setValidCubeCenter(v3i& pos, unsigned int decal);

	VOctreeNode* mRootNode = nullptr;
	
	// default depth max is 8 => 256x256x256 cubes = 512 x 512 x 512 units
	maInt		mMaxDepth = INIT_ATTRIBUTE(MaxDepth, 8);

	unsigned int	mCurrentVisibilityFlag = 0;

	const v3i				mNeightboursDecalVectors[6] = { {-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1} };
	const int				mInvDir[6] = {1,0,3,2,5,4};
};
