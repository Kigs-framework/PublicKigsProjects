#include "VOctree.h"
#include "CullingObject.h"



IMPLEMENT_CLASS_INFO(VOctree);

IMPLEMENT_CONSTRUCTOR(VOctree)
{
	mRootNode = new VOctreeNode(0);
}

VoxelOctreeContent VoxelOctreeContent::canCollapse(OctreeNodeBase** children, bool& doCollapse)
{
	VoxelOctreeContent result;
	result.mContent = 0;
	result.mVisibilityFlag = 0;

	doCollapse = false;
	
	std::unordered_map<unsigned int, unsigned int>	countChildrenTypes;
	unsigned int	leafCount = 0;
	for (int i = 0; i < 8; i++)
	{
		VOctreeNode& c = *(static_cast<VOctreeNode*>(children[i]));
		if (c.isLeaf())
		{
			leafCount++;
		}
		unsigned int ctype = c.getContentType().getContent();
		if (countChildrenTypes.find(ctype) == countChildrenTypes.end())
		{
			countChildrenTypes[ctype] = 1;
		}
		else
		{
			countChildrenTypes[ctype]++;
		}
	}

	unsigned int	highestCount = 0;
	unsigned int	bestFound = -1;

	for (const auto& i : countChildrenTypes)
	{
		if (i.second > highestCount)
		{
			highestCount = i.second;
			bestFound = i.first;
		}
	}

	// all children have the same type and are leaf ? => collapse children
	if ((highestCount == 8) && (leafCount == 8))
	{
		result.mContent = bestFound;
		doCollapse = true;
	}

	return result;
}


void	VOctree::setVoxelContent(const v3i& coordinate, unsigned int content)
{
	recursiveSetVoxelContent(static_cast<VOctreeNode*>(mRootNode), coordinate, content, 0);
}



unsigned int	VOctree::getVoxelContent(const v3i& coordinate, unsigned int maxDepth)
{
	int currentDepth = 0;
	int currentDecal = mMaxDepth;
	VOctreeNode* currentNode = static_cast<VOctreeNode*>(mRootNode);
	do
	{
		if (currentNode->isLeaf())
		{
			return currentNode->getContentType();
		}
#ifdef _DEBUG
		if (currentDepth >= mMaxDepth)
		{
			printf("should not occur\n");
			return 0;
		}
#endif
		if (currentDepth >= maxDepth)
		{
			return currentNode->getContentType();
		}

		unsigned int index = ((coordinate.x >> currentDecal) & 1) | (((coordinate.y >> currentDecal) & 1) << 1) | (((coordinate.z >> currentDecal) & 1) << 2);
		currentNode = static_cast<VOctreeNode*>(currentNode->getChild(index));
		currentDepth++;
		currentDecal--;

	} while (1);


}

bool	VOctree::recursiveSetVoxelContent(VOctreeNode* currentNode, const v3i& coordinate, unsigned int content, int currentDepth)
{
	const int maxDepth = mMaxDepth;

	if (currentNode->isLeaf() && (content == currentNode->getContentType()))
	{
		// currentNode is a leaf and children already have the good content type
		// so nothing to change
		return false;
	}
	// if it's a leaf, create chilren
	if (currentNode->isLeaf())
	{
		currentNode->split(currentNode->getContentType());
	}
	// first get next depth child index
	int currentDecal = maxDepth - currentDepth;
	unsigned int index = ((coordinate.x >> currentDecal) & 1) | (((coordinate.y >> currentDecal) & 1) << 1) | (((coordinate.z >> currentDecal) & 1) << 2);

	VOctreeNode* nextNode = static_cast<VOctreeNode*>(currentNode->getChild(index));
	currentDepth++;

	if (currentDepth >= maxDepth)
	{
		if (nextNode->getContentType() != content)
		{
			nextNode->setContent(content);
			return currentNode->refresh();
		}
	}

	if (recursiveSetVoxelContent(nextNode, coordinate, content, currentDepth))
	{
		return currentNode->refresh();
	}

	return false;
}






void	VOctree::recurseOrientedFloodFill::run(nodeInfo& startPos)
{
	// set current node as "treated"
	startPos.getNode<VOctreeNode>()->getContentType().setVisibilityFlag(mOctree.mCurrentVisibilityFlag);

	v3f centralVPos(startPos.coord.x, startPos.coord.y, startPos.coord.z);
	std::vector< nodeInfo> child;
	// for each adjacent node
	for (int dir = 0; dir < 6; dir++)
	{
		// get adjacent node
		nodeInfo	n = mOctree.getVoxelNeighbour(startPos, dir);

		// test node is in front of viewer
		v3f	dirV(n.coord.x, n.coord.y, n.coord.z);
		dirV -= mViewPos;

		if (Dot(mViewVector, dirV) > 0)
		{
			// test that flood fill don't go back
			{
				dirV.Set(n.coord.x, n.coord.y, n.coord.z);
				dirV -= centralVPos;
				dirV.Normalize();
				if (Dot(mViewVector, dirV) < -mSinFieldOfView)
				{
					continue;
				}
			}

			if (n.node == nullptr) // TODO => outside of this octree -> should check other octrees
			{
				continue;
			}
			if (n.getNode<VOctreeNode>()->getContentType().getVisibilityFlag() == mOctree.mCurrentVisibilityFlag) // already treated continue
			{
				continue;
			}

			child.clear();
			if (n.getNode<VOctreeNode>()->isLeaf()) // if this node is a leaf, then this is the only one to treat
			{
				child.push_back(n);
			}
			else // else get all sons on the correct side of n
			{
				mSideChildrenGrabber.reset(mOctree.mInvDir[dir], mOctree, &child);
				mSideChildrenGrabber.run(n);
			}

			// for all the found nodes, check if they need to be added to visible list or to be flood fill
			for (auto& c : child)
			{
				if (c.getNode<VOctreeNode>()->getContentType().getVisibilityFlag() != mOctree.mCurrentVisibilityFlag)
				{
					if (c.getNode<VOctreeNode>()->getContentType().getContent() == 0) // node is empty, recurse flood fill
					{
						// recursion is here
						run(c);
						continue;
					}
					else // add this node to visibility list
					{
						c.getNode<VOctreeNode>()->getContentType().setVisibilityFlag(mOctree.mCurrentVisibilityFlag);
						(*mNotEmptyList).push_back(c);
					}
				}
			}
		}
	}
}



void	VOctree::recurseFloodFill(const nodeInfo& startPos, std::vector<nodeInfo>& notEmptyList)
{
	// set current node as "treated"
	startPos.getNode<VOctreeNode>()->getContentType().setVisibilityFlag(mCurrentVisibilityFlag);

	// for each adjacent node
	for(int dir=0;dir<6;dir++)
	{
		// get adjacent node
		nodeInfo	n = getVoxelNeighbour(startPos,dir);

		if (n.node == nullptr) // TODO => outside of this octree -> should check other octrees
		{
			continue; 
		}
		if (n.getNode<VOctreeNode>()->getContentType().getVisibilityFlag() == mCurrentVisibilityFlag) // already treated continue
		{
			continue;
		}

		std::vector< nodeInfo> child;
		if (n.getNode<VOctreeNode>()->isLeaf()) // if this node is a leaf, then this is the only one to treat
		{
			child.push_back(n);
		}
		else // else get all sons on the correct side of n
		{
			recurseVoxelSideChildren r(mInvDir[dir], *this, &child);
			r.run(n);
		}

		// for all the found nodes, check if they need to be added to visible list or to be flood fill
		for (auto& c : child)
		{
			if (c.getNode<VOctreeNode>()->getContentType().getVisibilityFlag() != mCurrentVisibilityFlag)
			{
				if (c.getNode<VOctreeNode>()->getContentType().getContent() == 0) // node is empty, recurse flood fill
				{
					recurseFloodFill(c, notEmptyList);
					continue;
				}
				else // add this node to visibility list
				{
					c.getNode<VOctreeNode>()->getContentType().setVisibilityFlag(mCurrentVisibilityFlag);
					notEmptyList.push_back(c);
				}
			}
		}
	}
}

std::vector<nodeInfo>	VOctree::getVisibleCubeList(nodeInfo& startPos, Camera& cam)
{
	std::vector<nodeInfo> result;

	mCurrentVisibilityFlag++;

	/*CullingObject c;
	cam.InitCullingObject(&c);
	*/

	v3f viewPos=cam.GetGlobalPosition();
	v3f viewVector = cam.GetGlobalViewVector();

	viewVector.Normalize();

	recurseOrientedFloodFill t(*this,&result,viewVector,viewPos,PI*0.25f);
	t.run(startPos);

	return result;
}

// return max depth in octree
