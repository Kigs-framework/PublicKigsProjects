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

	static VoxelOctreeContent canCollapse(OctreeNodeBase** children,bool& doCollapse);

	bool	canOnlyBeSetOnLeaf() const
	{
		return true;
	}

	bool	isEmpty() const
	{
		return (mContent == 0);
	}

protected:
	unsigned int	mContent;

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


// Octree minimal subdivision is 2 units wide, so that the center of each cubic node can be defined using v3i 

class VOctree : public OctreeBase<CoreModifiable>
{
public:
	DECLARE_CLASS_INFO(VOctree, OctreeBase<CoreModifiable>, Core);
	DECLARE_CONSTRUCTOR(VOctree);

	
	// change content type of a voxel given it's coordinates in Octree reference
	void			setVoxelContent(const v3i& coordinate, unsigned int content);
	unsigned int	getVoxelContent(const v3i& coordinate,unsigned int maxLevel=(unsigned int)-1);

	
	std::vector<nodeInfo>	getVisibleCubeList(nodeInfo& startPos, Camera& camera);

protected:


	// utility class to avoid passing the same parameters to the recursive method
	// and mutualise some computation 
	class recurseOrientedFloodFill
	{
	public:
		recurseOrientedFloodFill(VOctree& octree,std::vector<nodeInfo>* notEmptyList, const v3f& viewVector, const v3f& viewPos, const float fieldOfView) :
			mOctree(octree)
			, mNotEmptyList(notEmptyList)
			, mViewVector(viewVector)
			, mViewPos(viewPos)
			, mSinFieldOfView(sinf(fieldOfView))
		{
		}
		~recurseOrientedFloodFill() {}

		void run(nodeInfo& node);

		void	reset(int dir)
		{
			
		}
	private:
		v3f		mViewVector;
		v3f		mViewPos;
		float	mSinFieldOfView;

		VOctree& mOctree;

		std::vector<nodeInfo>*		mNotEmptyList;
		std::vector< nodeInfo>		mChildToTreat;
		recurseVoxelSideChildren	mSideChildrenGrabber;


	};
	
	// return true if currentNode parent needs to be changed
	bool	recursiveSetVoxelContent(VOctreeNode* currentNode,const v3i& coordinate, unsigned int content, int currentDepth);

};
