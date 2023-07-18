#pragma once


#include "CoreModifiable.h"
#include "CoreBaseApplication.h"
#include "CoreFSMState.h"
#include "YouTubeConnect.h"
#include "KigsBitmap.h"

namespace Kigs
{
	using namespace Kigs::Core;
	using namespace Kigs::Fsm;

	class YoutubeAnalyser;

	// manage drawing of current graph
	class GraphDrawer : public CoreModifiable
	{
	protected:
		CMSP				mMainInterface;

		YoutubeAnalyser* mYoutubeAnalyser = nullptr;
		void	drawSpiral(std::vector<std::tuple<unsigned int, float, std::string>>& toShow);
		void	drawForce();
		void	drawStats(SP<Draw::KigsBitmap> bitmap);
		void	drawGeneralStats();

		bool	mEverythingDone = false;

		std::unordered_map<std::string, CMSP>									mShowedUser;

		enum Measure
		{
			Percent = 0,
			Similarity = 1,
			Normalized = 2,
			AnonymousCount = 3,
			Opening = 4,
			Closing = 5,
			Reverse_Percent = 6,
			MEASURE_COUNT = 7
		};

		const std::string	mUnits[MEASURE_COUNT] = { " \%"," sc"," n",""," Op"," Cl", " r%" };

		u32		mCurrentUnit = 0;

		double mForcedBaseStartingTime = 0.0;

		void	prepareForceGraphData();

		std::unordered_map < std::string, YoutubeConnect::PerAccountUserMap > mAccountSubscriberMap;

		maBool	mDrawForce = BASE_ATTRIBUTE(DrawForce, false);
		maBool	mDrawUnsub = BASE_ATTRIBUTE(DrawUnsub, false);
		bool	mCurrentStateHasForceDraw = false;
		bool	mCurrentStateHasUnsubscribedDraw = false;
		bool	mShowMyself = false;

		class Diagram
		{
			v2i				mZonePos = { 128,128 };
			v2i				mZoneSize = { 384,256 };
			v2f				mLimits = { 0.0f, -1.0f };
			std::string		mTitle = "Diagram";
			SP<Draw::KigsBitmap>	mBitmap;
			u32				mColumnCount = 12;
			bool			mUseLog;
			// apply multiplier, then shift
			float			mMultiplier = 0.0f;
			float			mShift = 0.0f;
			u32				mTitleFontSize = 32;

			Draw::KigsBitmap::KigsBitmapPixel	mColumnColor = { 0,0,0,255 };
			friend class GraphDrawer;

		public:
			Diagram(SP<Draw::KigsBitmap> bitmap) : mBitmap(bitmap)
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

	// Normalized drawing
	START_DECLARE_COREFSMSTATE(GraphDrawer, Normalized)
	COREFSMSTATE_WITHOUT_METHODS()
	END_DECLARE_COREFSMSTATE()

	// Jaccard Drawing
	START_DECLARE_COREFSMSTATE(GraphDrawer, Jaccard)
	COREFSMSTATE_WITHOUT_METHODS()
	END_DECLARE_COREFSMSTATE()

	// Reverse % Drawing (only on followers/following)
	START_DECLARE_COREFSMSTATE(GraphDrawer, ReversePercent)
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

}