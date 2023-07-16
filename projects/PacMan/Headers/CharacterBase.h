#pragma once

#include "CoreModifiable.h"
#include "UI/UIItem.h"

namespace Kigs
{
	using namespace Core;

	class Board;

	const v2i	movesVector[4] = { {1,0},{0,1},{-1,0},{0,-1} };

	class CharacterBase : public CoreModifiable
	{
	public:
		DECLARE_ABSTRACT_CLASS_INFO(CharacterBase, CoreModifiable, PacMan);
		DECLARE_CONSTRUCTOR(CharacterBase);

		void	initAtPos(const v2i& pos);

		void	setCurrentPos(const v2f& pos);

		const v2f& getCurrentPos()
		{
			return mCurrentPos;
		}

		void	setBoard(Board* b)
		{
			mBoard = b;
		}

		Board* getBoard()
		{
			return mBoard;
		}

		void	setGraphicRepresentation(CMSP rep)
		{
			mGraphicRepresentation = rep;
		}

		CMSP getGraphicRepresentation()
		{
			return mGraphicRepresentation;
		}

		bool	isDead()
		{
			return mIsDead;
		}

		void	setDead(bool d = true)
		{
			mIsDead = d;
		}

		v2i getRoundPos()
		{
			return v2i(round(mCurrentPos.x), round(mCurrentPos.y));
		}

		// return current and dest pos if not the same
		std::vector<v2i>	getPoses()
		{
			std::vector<v2i> result;
			v2i cpos = getRoundPos();
			result.push_back(cpos);
			if (cpos != mDestPos)
			{
				result.push_back(mDestPos);
			}
			return result;
		}

		v2i getDestPos()
		{
			return mDestPos;
		}

		void setSpeed(float s)
		{
			mSpeed = s;
		}
		void	setSpeedCoef(float sc)
		{
			mSpeedCoef = sc;
		}

		int getDirection()
		{
			return mDirection;
		}

		void	setDestPos(const v2i& dp)
		{
			mDestPos = dp;
		}

		virtual ~CharacterBase();

	protected:

		// return true if is at dest
		bool		moveToDest(const Time::Timer& timer, v2f& newpos);


		v2f			mCurrentPos;
		Board* mBoard = nullptr;
		SP<Draw2D::UIItem>	mGraphicRepresentation;

		bool		mIsDead = false;
		// -1 for no move direction, 
		int			mDirection = -1;
		v2i			mDestPos;

		float		mSpeed = 4.0f;
		float		mSpeedCoef = 1.0;
	};
}
