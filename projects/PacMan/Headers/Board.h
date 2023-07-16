#pragma once
#include "Case.h"
#include <vector>
#include <string>
#include "CoreModifiable.h"
#include "UI/UIItem.h"

namespace Kigs
{
	using namespace Core;
	class Ghost;
	class Player;
	class CharacterBase;

	class Board
	{
	protected:

		std::vector<std::vector<Case>>	mCases;
		v2i								mBoardSize;
		v2i								mTeleport[2];
		static Case						mErrorCase;
		SP<Draw2D::UIItem>				mLabyBG;
		CMSP							mParentInterface;
		std::vector<v2i>				mGhostAppearPos;

		v2f								mScreenSize;

		std::vector<SP<Ghost>>			mGhosts;
		SP<Player>						mPlayer;

		bool	checkColumnVisibility(int x, int y1, int y2);
		bool	checkRowVisibility(int y, int x1, int x2);


		void	manageTouchGhost();

		u32		mTotalEatCount = 0;
		u32		mEatCount = 0;

		u32		mDeltaScore = 0;

	public:

		Board(const std::string& filename, SP<Draw2D::UIItem> minterface);
		virtual ~Board();

		const v2i& getBoardSize()
		{
			return mBoardSize;
		}

		Case& getCase(const v2i& pos)
		{
			if ((pos.x >= 0) && (pos.x < mBoardSize.x))
			{
				if ((pos.y >= 0) && (pos.y < mBoardSize.y))
				{
					return mCases[pos.y][pos.x];
				}
			}

			return mErrorCase;
		}

		void	initGraphicBoard();

		v2i		getAppearPosForGhost();
		int		directionFromDelta(const v2i& deltap);
		bool	manageTeleport(const v2i& pos, int direction, CharacterBase* character);

		SP<Draw2D::UIItem>	getGraphicInterface()
		{
			return mParentInterface;
		}

		v2f		convertBoardPosToDock(const v2f& p);

		void							InitGhosts(float speedcoef);
		void							InitPlayer(float speedcoef);

		bool	checkForGhostOnCase(const v2i& pos, const Ghost* me = nullptr);
		bool	checkForWallOnCase(const v2i& pos);

		std::vector<bool>	getAvailableDirection(const v2i& pos);

		void	Update();

		v2i	ghostSeePacman(const v2i& pos);

		void	checkEat(const v2i& pos);
	};
}