#include "VOctree.h"
#include "CullingObject.h"

#ifdef _DEBUG
unsigned int	VOctreeNode::mCurrentAllocatedNodeCount = 0;
#endif

IMPLEMENT_CLASS_INFO(VOctree);

IMPLEMENT_CONSTRUCTOR(VOctree)
{
	mRootNode = new VOctreeNode(0);
}


// return depth of the node
unsigned int VOctreeNode::getDepth()
{
	if (isLeaf())
		return 0;

	unsigned int maxD=0;
	for (int i=0;i<8;i++)
	{
		unsigned int cd =mChildren[i].getDepth();
		if (cd > maxD)
		{
			maxD = cd;
		}
	}
	return maxD + 1;
}

void	VOctree::setVoxelContent(const v3i& coordinate, unsigned int content)
{
	recursiveSetVoxelContent(mRootNode, coordinate, content, 0);
}

void	VOctree::setValidCubeCenter(v3i& pos, unsigned int decal)
{
	// set coord center according to level
	unsigned int mask = 1 << (decal + 1);
	mask--;
	mask ^= 0xFFFFFFFF;
	unsigned int half = 1 << decal;

	pos.x = (pos.x & mask) + half;
	pos.y = (pos.y & mask) + half;
	pos.z = (pos.z & mask) + half;
}

nodeInfo VOctree::getVoxelAt(const v3i& coordinate, unsigned int maxDepth)
{
	nodeInfo result;
	result.coord = coordinate;
	result.level = 0;
	result.vnode = mRootNode;
	int currentDecal = mMaxDepth;
	do
	{
		if (result.vnode->isLeaf())
		{
			break;
		}
#ifdef _DEBUG
		if (result.level >= mMaxDepth)
		{
			result.vnode = nullptr;
			printf("should not occur\n");
			break;
		}
#endif
		if (result.level >= maxDepth)
		{
			break;
		}

		unsigned int index = ((coordinate.x >> currentDecal) & 1) | (((coordinate.y >> currentDecal) & 1) << 1) | (((coordinate.z >> currentDecal) & 1) << 2);
		result.vnode = result.vnode->getChild(index);
		result.level++;
		currentDecal--;

	} while (1);

	setValidCubeCenter(result.coord, currentDecal);
	return result;
}

unsigned int	VOctree::getVoxelContent(const v3i& coordinate, unsigned int maxDepth)
{
	int currentDepth = 0;
	int currentDecal = mMaxDepth;
	VOctreeNode* currentNode = mRootNode;
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
		currentNode = currentNode->getChild(index);
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

	VOctreeNode* nextNode = currentNode->getChild(index);
	currentDepth++;

	if (currentDepth >= maxDepth)
	{
		if (nextNode->getContentType() != content)
		{
			nextNode->setContentType(content);
			return currentNode->refresh();
		}
	}

	if (recursiveSetVoxelContent(nextNode, coordinate, content, currentDepth))
	{
		return currentNode->refresh();
	}

	return false;
}


bool VOctreeNode::refresh()
{
	if (isLeaf())
	{
		return false;
	}

	std::unordered_map<unsigned int, unsigned int>	countChildrenTypes;
	unsigned int	leafCount = 0;
	for (int i=0;i<8;i++)
	{
		auto& c = mChildren[i];
		if (c.isLeaf())
		{
			leafCount++;
		}
		if (countChildrenTypes.find(c.mContentType) == countChildrenTypes.end())
		{
			countChildrenTypes[c.mContentType] = 1;
		}
		else
		{
			countChildrenTypes[c.mContentType]++;
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
	if ((highestCount == 8) && (leafCount==8))
	{
		mContentType = bestFound;

		collapse();
		// need to refresh parent
		return true;
	}

	if (bestFound == mContentType)
	{
		return false;
	}

	mContentType = bestFound;
	return true;
}

nodeInfo	VOctree::getVoxelNeighbour(const nodeInfo& node,int dir)
{
	nodeInfo result;
	result.vnode = nullptr;

	const unsigned int maxSize = 2 << mMaxDepth;

	int dpos = 2 << (mMaxDepth - node.level);

	v3i	dposv(node.coord);

	dposv += mNeightboursDecalVectors[dir]*dpos;
	
	if (dir < 2)
	{
		if ((dposv.x >= 0)&& (dposv.x < maxSize))
		{
			return getVoxelAt(dposv, node.level);
		}
		return result;
	}
	else if (dir < 4)
	{
		if ((dposv.y >= 0) && (dposv.y < maxSize))
		{
			return getVoxelAt(dposv, node.level);
		}
		return result;
	}
	else
	{
		if ((dposv.z >= 0) && (dposv.z < maxSize))
		{
			return getVoxelAt(dposv, node.level);
		}
		
	}
	return result;
		
}

void	VOctree::recurseVoxelSideChildren::run(const nodeInfo& node)
{

	int dpos = 1 << (mMaxDepth - (node.level + 1));

	// check all sons
	for (int c = 0; c < 8; c++)
	{
		if ((c & mMaskTest) == mMaskResult)
		{
			nodeInfo toAdd;
			toAdd.vnode = &(node.vnode->mChildren[c]);
			toAdd.level = node.level + 1;

			// add block so childCoord is not pushed for recursion
			{
				v3i childCoord = node.coord;
				childCoord.x += (((c & 1) << 1) - 1) * dpos;
				childCoord.y += ((c & 2) - 1) * dpos;
				childCoord.z += (((c & 4) >> 1) - 1) * dpos;

				toAdd.coord = childCoord;
			}

			if (toAdd.vnode->isLeaf())
			{
				(*mChildList).push_back(toAdd);
			}
			else
			{
				run(toAdd);
			}
		}
	}

}

void	VOctree::recurseOrientedFloodFill::run(const nodeInfo& startPos)
{
	// set current node as "treated"
	startPos.vnode->mVisibilityFlag = mOctree.mCurrentVisibilityFlag;

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

			if (n.vnode == nullptr) // TODO => outside of this octree -> should check other octrees
			{
				continue;
			}
			if (n.vnode->mVisibilityFlag == mOctree.mCurrentVisibilityFlag) // already treated continue
			{
				continue;
			}

			child.clear();
			if (n.vnode->isLeaf()) // if this node is a leaf, then this is the only one to treat
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
				if (c.vnode->mVisibilityFlag != mOctree.mCurrentVisibilityFlag)
				{
					if (c.vnode->mContentType == 0) // node is empty, recurse flood fill
					{
						// recursion is here
						run(c);
						continue;
					}
					else // add this node to visibility list
					{
						c.vnode->mVisibilityFlag = mOctree.mCurrentVisibilityFlag;
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
	startPos.vnode->mVisibilityFlag = mCurrentVisibilityFlag;

	// for each adjacent node
	for(int dir=0;dir<6;dir++)
	{
		// get adjacent node
		nodeInfo	n = getVoxelNeighbour(startPos,dir);

		if (n.vnode == nullptr) // TODO => outside of this octree -> should check other octrees
		{
			continue; 
		}
		if (n.vnode->mVisibilityFlag == mCurrentVisibilityFlag) // already treated continue
		{
			continue;
		}

		std::vector< nodeInfo> child;
		if (n.vnode->isLeaf()) // if this node is a leaf, then this is the only one to treat
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
			if (c.vnode->mVisibilityFlag != mCurrentVisibilityFlag)
			{
				if (c.vnode->mContentType == 0) // node is empty, recurse flood fill
				{
					recurseFloodFill(c, notEmptyList);
					continue;
				}
				else // add this node to visibility list
				{
					c.vnode->mVisibilityFlag = mCurrentVisibilityFlag;
					notEmptyList.push_back(c);
				}
			}
		}
	}
}

std::vector<nodeInfo>	VOctree::getVisibleCubeList(const nodeInfo& startPos, Camera& cam)
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
unsigned int VOctree::getMaxDepth()
{
	return mRootNode->getDepth();
}