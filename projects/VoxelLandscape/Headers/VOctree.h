#pragma once

#include "CoreModifiable.h"
#include "CoreModifiableAttribute.h"

class VOctreeNode;

struct nodeInfo
{
	int				level;
	v3i				coord;
	VOctreeNode*	vnode;
};

class VOctreeNode
{
public:

	friend class VOctree;

	VOctreeNode()
	{
#ifdef _DEBUG
		mCurrentAllocatedNodeCount++;
#endif
	}
	VOctreeNode(unsigned int content) : mContentType(content)
	{
#ifdef _DEBUG
		mCurrentAllocatedNodeCount++;
#endif
	}

	~VOctreeNode()
	{
#ifdef _DEBUG
		mCurrentAllocatedNodeCount--;
#endif
		// destroy sons
		collapse();
	}

	inline bool isLeaf()
	{
		// children are all set or all null, so test only one
		if (mChildren[0])
		{
			return false;
		}
		return true;
	}

	// return content type of the node
	unsigned int getContentType()
	{
		return mContentType;
	}



protected:

	// destroy sons
	// methods don't test if collapse is valid or not
	// collapse children with different content is a bad idea
	void	collapse()
	{
		// destroy sons
		for (auto& n : mChildren)
		{
			if (n)
			{
				delete n;
				n = nullptr;
			}
		}
	}

	// return depth of the node
	unsigned int getDepth();

	// try to set content of the node
	// only content of leaf node can be set
	// return true if content was set
	bool	setContentType(unsigned int content)
	{
		if (isLeaf())
		{
			mContentType = content;
			return true;
		}
		return false;
	}

	// try to split node ( create 8 children )
	// only leaf nodes can be splitted 
	bool split(unsigned int content)
	{
		if (isLeaf())
		{
			for (auto& n : mChildren)
			{
				n = new VOctreeNode(content);
			}

			return true;
		}
		return false;
	}

	// recompute mContentType according to sons content type
	// if all childs have same contentType collapse node
	// return true if mContentType was changed 
	bool refresh();

	// return child at given index
	// don't test if index is in [0-7] but we suppose function is only called by VOctree
	VOctreeNode* getChild(unsigned int index)
	{
		return mChildren[index];
	}


	VOctreeNode*	mChildren[8] = { nullptr };
	unsigned int	mContentType = 0;
	unsigned int	mVisibilityFlag=0;

#ifdef _DEBUG
	static unsigned int	mCurrentAllocatedNodeCount;
#endif
};

// Octree minimal subdivision is 2 units wide, so that the center of each cubic node can be defined using v3i 

class VOctree : public CoreModifiable
{
public:
	DECLARE_CLASS_INFO(VOctree, CoreModifiable, Core);
	DECLARE_CONSTRUCTOR(VOctree);

	

	// change content type of a voxel given it's coordinates in Octree reference
	void			setVoxelContent(const v3i& coordinate, unsigned int content);
	unsigned int	getVoxelContent(const v3i& coordinate,unsigned int maxLevel=(unsigned int)-1);

	nodeInfo getVoxelAt(const v3i& coordinate, unsigned int maxDepth = (unsigned int)-1);
	std::vector<nodeInfo>	getVisibleCubeList(const v3i& startPos, const v3f& viewVector);

#ifdef _DEBUG
	void	printAllocatedNodeCount()
	{
		printf("allocated nodes : %d\n", VOctreeNode::mCurrentAllocatedNodeCount);
	}
#endif

	// return max depth in octree
	unsigned int getMaxDepth();

protected:

	// get 26 neighbours (maximum) at the same depth   
	std::vector<nodeInfo>	getVoxelNeighbours(const nodeInfo& node);

	
	// return true if currentNode parent needs to be changed
	bool	recursiveSetVoxelContent(VOctreeNode* currentNode,const v3i& coordinate, unsigned int content, int currentDepth);


	// set coord to real cube center
	// minimal cube is 2 units wide
	void	setValidCubeCenter(v3i& pos, unsigned int decal);




	VOctreeNode* mRootNode = nullptr;
	
	// default depth max is 8 => 256x256x256 cubes = 512 x 512 x 512 units
	maInt		mMaxDepth = INIT_ATTRIBUTE(MaxDepth, 8);

	unsigned int	mCurrentVisibilityFlag = 0;
};
