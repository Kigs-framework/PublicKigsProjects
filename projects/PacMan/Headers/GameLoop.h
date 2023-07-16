#pragma once

#include "CoreModifiable.h"
#include "Board.h"
#include "Ghost.h"

namespace Kigs
{
	class GameLoop
	{
	protected:

		CMSP			mMainInterface;

		Board* mBoard = nullptr;

	public:

		GameLoop(CMSP linterface);

		~GameLoop()
		{
			delete mBoard;
			mBoard = nullptr;
		}

		void	update();

		void	reset(float speedcoef);
	};
}