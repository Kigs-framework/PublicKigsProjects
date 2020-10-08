#include "VOctree.h"


#ifdef _DEBUG
unsigned int	VOctreeNode::mCurrentAllocatedNodeCount = 0;
#endif

IMPLEMENT_CLASS_INFO(VOctree);

IMPLEMENT_CONSTRUCTOR(VOctree)
{
	mRootNode = new VOctreeNode(0);
}


void	VOctree::setVoxelContent(const v3i& coordinate, unsigned int content)
{
	recursiveSetVoxelContent(mRootNode, coordinate, content, 1);
}

nodeInfo VOctree::getVoxelAt(const v3i& coordinate, unsigned int maxDepth)
{
	nodeInfo result;
	result.coord = coordinate;
	result.level = 1;
	result.vnode = mRootNode;
	int currentDecal = mMaxDepth - 1;
	do
	{
		if (result.vnode->isLeaf())
		{
			return result;
		}
#ifdef _DEBUG
		if (result.level > mMaxDepth)
		{
			result.vnode = nullptr;
			printf("should not occur\n");
			return result;
		}
#endif
		if (result.level > maxDepth)
		{
			return result;
		}

		unsigned int index = ((coordinate.x >> currentDecal) & 1) | (((coordinate.y >> currentDecal) & 1) << 1) | (((coordinate.z >> currentDecal) & 1) << 2);
		result.vnode = result.vnode->getChild(index);
		result.level++;
		currentDecal--;

	} while (1);

}

unsigned int	VOctree::getVoxelContent(const v3i& coordinate, unsigned int maxDepth)
{
	int currentDepth = 1;
	int currentDecal = mMaxDepth - 1;
	VOctreeNode* currentNode = mRootNode;
	do
	{
		if (currentNode->isLeaf())
		{
			return currentNode->getContentType();
		}
#ifdef _DEBUG
		if (currentDepth > mMaxDepth)
		{
			printf("should not occur\n");
			return 0;
		}
#endif
		if (currentDepth > maxDepth)
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

	if (currentDepth > maxDepth)
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
	for (auto& c : mChildren)
	{
		if (c->isLeaf())
		{
			leafCount++;
		}
		if (countChildrenTypes.find(c->mContentType) == countChildrenTypes.end())
		{
			countChildrenTypes[c->mContentType] = 1;
		}
		else
		{
			countChildrenTypes[c->mContentType]++;
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

	const unsigned int maxSize = 1 << mMaxDepth;

	int dpos = 1 << (mMaxDepth - node.level);

	std::set<VOctreeNode*>	alreadyFound;

	v3i	dposv;
	for (int i = -1; i <= 1; i++)
	{
		dposv.x = node.coord.x + dpos * i;
		if ((dposv.x >= 0) && (dposv.x < maxSize))
		{
			for (int j = -1; j <= 1; j++)
			{
				dposv.y = node.coord.y + dpos * j;
				if ((dposv.y >= 0) && (dposv.y < maxSize))
				{
					for (int k = -1; k <= 1; k++)
					{
						if (k == 0)
						{
							if ((j == 0) && (i == 0))
							{
								// skip current node
								continue;
							}
						}
						dposv.z = node.coord.z + dpos * k;
						if ((dposv.z >= 0) && (dposv.z < maxSize))
						{
							nodeInfo	f = getVoxelAt(dposv, node.level);
							if (alreadyFound.find(f.vnode) == alreadyFound.end())
							{
								alreadyFound.insert(f.vnode);
								result.push_back(f);
							}
						}
					}
				}
			}
		}
	}

	return result;
}

std::vector<nodeInfo>	VOctree::getVisibleCubeList(const v3i& startPos, const v3f& viewVector)
{
	std::vector<nodeInfo> result;

	mCurrentVisibilityFlag++;


	nodeInfo current = getVoxelAt(startPos);

	// say this node was treated by this search
	current.vnode->mVisibilityFlag = mCurrentVisibilityFlag;

	std::vector<nodeInfo>	n= getVoxelNeighbours(current);

	return result;
}