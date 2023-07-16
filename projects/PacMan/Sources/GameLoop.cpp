#include "GameLoop.h"
#include "Core.h"

using namespace Kigs;

void	GameLoop::update()
{
	mBoard->Update();
}

void	GameLoop::reset(float speedcoef)
{
	if(mBoard)
		delete mBoard;

	mBoard = new Board("laby.json", mMainInterface);
	mBoard->initGraphicBoard();
	mBoard->InitGhosts(speedcoef);
	mBoard->InitPlayer(speedcoef);
}

GameLoop::GameLoop(CMSP linterface) :mMainInterface(linterface)
{
	srand(time(NULL));
	// Init Board
	reset(1.0f);
}
