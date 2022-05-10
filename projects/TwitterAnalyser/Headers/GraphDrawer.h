#pragma once

#include "CoreModifiable.h"
#include "CoreBaseApplication.h"
#include "CoreFSMState.h"
#include "TwitterConnect.h"
#include "KigsBitmap.h"


class TwitterAnalyser;

// manage drawing of current graph
class GraphDrawer : public CoreModifiable
{
protected:
	CMSP				mMainInterface;
		
	TwitterAnalyser* mTwitterAnalyser = nullptr;
	void	drawSpiral(std::vector<std::tuple<unsigned int, float,u64>>& toShow);
	void	drawForce();
	void	drawStats(SP<KigsBitmap> bitmap);
	
	void	drawGeneralStats();

	bool	mEverythingDone=false;

	std::unordered_map<u64, CMSP>									mShowedUser;

	enum Measure
	{
		Percent = 0,
		Similarity = 1,
		Normalized = 2,
		FollowerCount = 3,
		MEASURE_COUNT = 4
	};

	const std::string	mUnits[MEASURE_COUNT] = { "\%","sc","n",""};

	u32		mCurrentUnit = 0;

	double mForcedBaseStartingTime = 0.0;

	void	prepareForceGraphData();

	std::unordered_map<u64, TwitterConnect::PerAccountUserMap>	mAccountSubscriberMap;

	maBool	mDrawForce = BASE_ATTRIBUTE(DrawForce, false);
	bool	mCurrentStateHasForceDraw = false;

	class Diagram
	{
		v2i				mZonePos={128,128};
		v2i				mZoneSize={384,256};
		v2f				mLimits = { 0.0f, -1.0f };
		std::string		mTitle="Diagram";
		SP<KigsBitmap>	mBitmap;
		u32				mColumnCount = 12;
		bool			mUseLog;

		KigsBitmap::KigsBitmapPixel	mColumnColor = {0,0,0,255};
		friend class GraphDrawer;

	public:
		Diagram(SP<KigsBitmap> bitmap) : mBitmap(bitmap)
		{

		}

		template<typename T>
		void	Draw(const std::vector<T>& values);
	};

public:
	DECLARE_CLASS_INFO(GraphDrawer, CoreModifiable, GraphDrawer);
	DECLARE_CONSTRUCTOR(GraphDrawer);

	void	setInterface(CMSP minterface)
	{
		mMainInterface = minterface;
	}

	void InitModifiable() override;

	maBool	mDrawTop = BASE_ATTRIBUTE(DrawTop, false);
	maBool	mHasJaccard = BASE_ATTRIBUTE(HasJaccard, false);
	maBool  mGoNext = BASE_ATTRIBUTE(GoNext, false);
	void	setEverythingDone()
	{
		mEverythingDone = true;
	}

	void	nextDrawType();

};

// Percent drawing
START_DECLARE_COREFSMSTATE(GraphDrawer, Percent)
COREFSMSTATE_WITHOUT_METHODS()
END_DECLARE_COREFSMSTATE()

START_DECLARE_COREFSMSTATE(GraphDrawer, TopDraw)
COREFSMSTATE_WITHOUT_METHODS()
END_DECLARE_COREFSMSTATE()

// Normalized drawing
START_DECLARE_COREFSMSTATE(GraphDrawer, Normalized)
COREFSMSTATE_WITHOUT_METHODS()
END_DECLARE_COREFSMSTATE()

// Jaccard Drawing
START_DECLARE_COREFSMSTATE(GraphDrawer, Jaccard)
COREFSMSTATE_WITHOUT_METHODS()
END_DECLARE_COREFSMSTATE()

// user stats drawing
START_DECLARE_COREFSMSTATE(GraphDrawer, UserStats)
CMSP	mBitmap;
COREFSMSTATE_WITHOUT_METHODS()
END_DECLARE_COREFSMSTATE()

// Force drawing
START_DECLARE_COREFSMSTATE(GraphDrawer, Force)
COREFSMSTATE_WITHOUT_METHODS()
END_DECLARE_COREFSMSTATE()

