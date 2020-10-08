#pragma once

#include <DataDrivenBaseApplication.h>
#include "VOctree.h"

class VoxelLandscape : public DataDrivenBaseApplication
{
public:
	DECLARE_CLASS_INFO(VoxelLandscape, DataDrivenBaseApplication, Core);
	DECLARE_CONSTRUCTOR(VoxelLandscape);

protected:
	void	ProtectedInit() override;
	void	ProtectedUpdate() override;
	void	ProtectedClose() override;

	
	void	ProtectedInitSequence(const kstl::string& sequence) override;
	void	ProtectedCloseSequence(const kstl::string& sequence) override;


	void	initLandscape();
	v3i		getEmptyNode(const v3i& startingPos, int maxTry=1000);

	SP<VOctree>	mVOctree;
};
