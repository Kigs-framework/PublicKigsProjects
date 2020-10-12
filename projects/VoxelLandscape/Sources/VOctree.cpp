#include "VOctree.h"


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

std::vector<nodeInfo>	VOctree::getVoxelNeighbours(const nodeInfo& node)
{
	std::vector<nodeInfo> result;
	result.resize(6);

	const unsigned int maxSize = 2 << mMaxDepth;

	int dpos = 2 << (mMaxDepth - node.level);

	v3i	dposv;
	// x
	dposv = node.coord;
	dposv.x = node.coord.x - dpos;
	if (dposv.x >= 0)
	{
		result[0]=getVoxelAt(dposv, node.level);
	}

	dposv = node.coord;
	dposv.x = node.coord.x + dpos;
	if (dposv.x < maxSize)
	{
		result[1]=getVoxelAt(dposv, node.level);
	}
	// y
	dposv = node.coord;
	dposv.y = node.coord.y - dpos;
	if (dposv.y >= 0)
	{
		result[2] = getVoxelAt(dposv, node.level);
	}

	dposv = node.coord;
	dposv.y = node.coord.y + dpos;
	if (dposv.y < maxSize)
	{
		result[3] = getVoxelAt(dposv, node.level);
	}
	// z
	dposv = node.coord;
	dposv.z = node.coord.z - dpos;
	if (dposv.z >= 0)
	{
		result[4]=getVoxelAt(dposv, node.level);
	}

	dposv = node.coord;
	dposv.z = node.coord.z + dpos;
	if (dposv.z < maxSize)
	{
		result[5]=getVoxelAt(dposv, node.level);
	}

	return result;
}


void	VOctree::recurseFloodFill(const nodeInfo& startPos, std::vector<nodeInfo>& notEmptyList)
{
	startPos.vnode->mVisibilityFlag = mCurrentVisibilityFlag;
	std::vector<nodeInfo>	n = getVoxelNeighbours(startPos);
	for (auto& neighbour : n)
	{
		if (neighbour.vnode->mVisibilityFlag == mCurrentVisibilityFlag) // already treated
		{
			continue;
		}

		if (neighbour.vnode->isLeaf())
		{
			if (neighbour.vnode->mContentType == 0)
			{
				recurseFloodFill(neighbour, notEmptyList);
				continue;
			}
			else
			{
				notEmptyList.push_back(neighbour);
			}
		}
		else
		{

		}
	}
}

std::vector<nodeInfo>	VOctree::getVisibleCubeList(const v3i& startPos, const v3f& viewVector)
{
	std::vector<nodeInfo> result;

	mCurrentVisibilityFlag++;

	nodeInfo current = getVoxelAt(startPos);

	recurseFloodFill(current, result);

	return result;
}

// return max depth in octree
unsigned int VOctree::getMaxDepth()
{
	return mRootNode->getDepth();
}