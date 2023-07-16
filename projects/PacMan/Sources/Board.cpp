#include "Board.h"
#include "JSonFileParser.h"
#include "Ghost.h"
#include "Player.h"
#include "Core.h"
#include "NotificationCenter.h"
#include "CoreBaseApplication.h"
#include "CharacterBase.h"

using namespace Kigs;
using namespace Kigs::Draw2D;

std::string ghostNames[4] = { "inky","clyde","pinky","blinky" };

Case				Board::mErrorCase;
#define tileSize 25.0f

int		Board::directionFromDelta(const v2i& deltap)
{
	v2i newdelta(deltap);
	// need "normalization" ?
	if ((deltap.x) && (deltap.y))
	{
		v2i absdelta(abs(newdelta.x), abs(newdelta.y));
		if (absdelta.x > absdelta.y)
		{
			newdelta.y = 0;
		}
		else if (absdelta.x == absdelta.y)
		{
			newdelta[rand()%2] = 0;
		}
		else
		{
			newdelta.x = 0;
		}
	}
	else if(deltap.x == deltap.y) // both are 0
	{
		return -1;
	}

	if (newdelta.x > 0)
	{
		return 0;
	}
	else if (newdelta.y > 0)
	{
		return 1;
	}
	else if (newdelta.x < 0)
	{
		return 2;
	}

	return 3;
}

void	Board::manageTouchGhost()
{
	if (mPlayer->isDead()) // already dead
		return;

	v2f pacpos = mPlayer->getCurrentPos();
	for (int i = 0; i < 4; i++)
	{
		if (mGhosts[i]->isDead())
			continue;
		v2f gpos = mGhosts[i]->getCurrentPos();

		if (DistSquare(pacpos, gpos) < 1.0f)
		{
			if (mGhosts[i]->isHunted())
			{
				mGhosts[i]->setDead();
				mDeltaScore += 200;
			}
			else 
			{
				KigsCore::GetNotificationCenter()->postNotificationName("PacManDie");
				mPlayer->setDead();
				int lives = KigsCore::GetCoreApplication()->getValue<int>("Lives");
				if (lives)
				{
					lives -= 1;
					KigsCore::GetCoreApplication()->setValue("Lives",lives);
					if (lives ==0)
					{
						KigsCore::GetNotificationCenter()->postNotificationName("GameOver");
					}
				}
				break;
			}
		}
	}
}

Board::Board(const std::string& filename, SP<UIItem> minterface) : mParentInterface(minterface)
{
	JSonFileParser L_JsonParser;
	CoreItemSP initP = L_JsonParser.Get_JsonDictionary(filename);

	mBoardSize = (v2f)initP["Size"];

	if ((mBoardSize.y > 0) && (mBoardSize.x > 0))
	{
		CoreItemSP cases = initP["Cases"];
		size_t caseIndex = 0;
		mCases.resize(mBoardSize.y);
		for (int i = 0; i < mBoardSize.y; i++)
		{
			mCases[i].resize(mBoardSize.x);
			for (int j = 0; j < mBoardSize.x; j++)
			{
				mCases[i][j].setInitType((int)cases[caseIndex]);

				if ((int)cases[caseIndex] == 3)
				{
					mGhostAppearPos.push_back({ j,i });
				}
				else if ((int)cases[caseIndex] == 2)
				{
					mTotalEatCount++;
				}

				caseIndex++;
			}
		}
	}

	mScreenSize = mParentInterface->getValue<v2f>("Size");
}

void	Board::InitGhosts(float speedcoef)
{
	// Init Ghosts
	for (int i = 0; i < 4; i++)
	{
		mGhosts.push_back(KigsCore::GetInstanceOf("gg", "Ghost"));
		mGhosts.back()->setBoard(this);
		mGhosts.back()->setValue("Name", ghostNames[i]);
		mGhosts.back()->setSpeedCoef(speedcoef);
		mGhosts.back()->Init();
	}
}

void	Board::InitPlayer(float speedcoef)
{
	mPlayer= KigsCore::GetInstanceOf("player", "Player");
	mPlayer->setBoard(this);
	mPlayer->setSpeedCoef(speedcoef);
	mPlayer->Init();
}

Board::~Board()
{
	mGhosts.clear();
	mParentInterface->removeItem(mLabyBG);
	mLabyBG = nullptr;
	mPlayer = nullptr;
}


void	Board::Update()
{
	if (KigsCore::GetCoreApplication()->getValue<int>("Lives") == 0)
		return;
	const auto& t=KigsCore::GetCoreApplication()->GetApplicationTimer();
	for (auto g : mGhosts)
		g->CallUpdate(*t.get(),nullptr);

	mPlayer->CallUpdate(*t.get(), nullptr);

	manageTouchGhost();

	// update application score

	int score = KigsCore::GetCoreApplication()->getValue<int>("Score");
	score += mDeltaScore;
	mDeltaScore = 0;
	KigsCore::GetCoreApplication()->setValue("Score",score);

	if (mEatCount == mTotalEatCount)
	{
		KigsCore::GetNotificationCenter()->postNotificationName("LevelWon");
	}
}

bool	Board::manageTeleport(const v2i& pos, int direction, CharacterBase* character)
{
	if ((pos == mTeleport[0]) && (character->getDirection() ==2))
	{
		character->setCurrentPos(mTeleport[1]);
		character->setDestPos(mTeleport[1] + movesVector[2]);
		return true;
	}
	else if ((pos == mTeleport[1]) && (character->getDirection() == 0))
	{
		character->setCurrentPos(mTeleport[0]);
		character->setDestPos(mTeleport[0] + movesVector[0]);
		return true;
	}
	return false;
}


void	Board::initGraphicBoard()
{
	mLabyBG = KigsCore::GetInstanceOf("bg", "UIItem");
	mLabyBG->setValue("Size", mScreenSize);
	mLabyBG->setValue("Priority", 10);

	v2i labySize = getBoardSize();

	for (int i = 0; i < labySize.y; i++)
	{
		for (int j = 0; j < labySize.x; j++)
		{
			Case& currentC = getCase({ j,i });
			SP<UIItem>	uicase = nullptr;
			switch (currentC.getType())
			{
			case 1:
			case 3:
			{
				uicase = KigsCore::GetInstanceOf("case", "UIPanel");
				uicase->setValue("Anchor", v2f(0.5f, 0.5f));
				uicase->setValue("Size", v2f(tileSize * 0.9f, tileSize * 0.9f));
				uicase->setValue("Dock", v2f(0.5 + (float)(j - labySize.x / 2) * (tileSize / mScreenSize.x), 0.5 + (float)(i - labySize.y / 2) * (tileSize / mScreenSize.y)));
				uicase->setValue("Priority", 20);
				uicase->setValue("Color", v3f(0.0, 0.0, 1.0));
				uicase->setValue("Opacity", (currentC.getType() == 1) ? 1.0 : 0.5);
				mLabyBG->addItem(uicase);
				uicase->Init();
			}
			break;
			case 2:
			{
				uicase = KigsCore::GetInstanceOf("pastille", "UIImage");
				uicase->setValue("TextureName", "Pacman.json:dot");
				uicase->setValue("Anchor", v2f(0.5f, 0.5f));
				uicase->setValue("ForceNearest", true);
				uicase->setValue("Size", v2f(tileSize * 2.0f, tileSize * 2.0f));
				uicase->setValue("Dock", v2f(0.5 + (float)(j - labySize.x / 2) * (tileSize / mScreenSize.x), 0.5 + (float)(i - labySize.y / 2) * (tileSize / mScreenSize.y)));
				uicase->setValue("Priority", 3);
				mLabyBG->addItem(uicase);
				uicase->Init();
			}
			break;
			case 4:
			{
				uicase = KigsCore::GetInstanceOf("pomme", "UIImage");
				uicase->setValue("TextureName", "Pacman.json:apple");
				uicase->setValue("Anchor", v2f(0.5f, 0.5f));
				uicase->setValue("ForceNearest", true);
				uicase->setValue("Size", v2f(tileSize, tileSize));
				uicase->setValue("Dock", v2f(0.5 + (float)(j - labySize.x / 2) * (tileSize / mScreenSize.x), 0.5 + (float)(i - labySize.y / 2) * (tileSize / mScreenSize.y)));
				uicase->setValue("Priority", 3);
				mLabyBG->addItem(uicase);
				uicase->Init();
			}
			break;
			case 5:
			{
				mTeleport[0].Set(j, i);
			}
			break;
			case 6:
			{
				mTeleport[1].Set(j, i);
			}
			break;
			}
			if (uicase)
			{
				currentC.setGraphicRepresentation(uicase);
			}
		}
	}
	mParentInterface->addItem(mLabyBG);
	mLabyBG->Init();
}


v2i		Board::getAppearPosForGhost()
{
	v2i foundpos(0, 0);
	bool found = false;

	while (!found)
	{
		foundpos=mGhostAppearPos[rand() % (mGhostAppearPos.size())];
		if (!checkForGhostOnCase(foundpos))
		{
			found = true;
		}
	}

	return foundpos;
}

v2f		Board::convertBoardPosToDock(const v2f& p)
{
	v2i labySize = getBoardSize();

	v2f result(0.5f + (p.x - (float)(labySize.x / 2)) * (tileSize / mScreenSize.x), 0.5f + (p.y - (float)(labySize.y / 2)) * (tileSize / mScreenSize.y));

	return result;
}

bool	Board::checkForGhostOnCase(const v2i& pos,const Ghost* me)
{
	for (auto g : mGhosts)
	{
		if (!g->isDead())
		{
			if (g.get() != me)
			{
				v2i gpos = g->getDestPos();
				
				if (pos == gpos)
				{
					return true;
				}
			}
		}
	}
	return false;
}

bool	Board::checkForWallOnCase(const v2i& pos)
{
	if (mCases[pos.y][pos.x].getType() == 1)
		return true;

	return false;
}

bool	Board::checkColumnVisibility(int x, int y1, int y2)
{
	if (y1 > y2)
	{
		int swp = y1;
		y1 = y2;
		y2 = swp;
	}
	for (int y = y1; y <= y2; y++)
	{
		if (mCases[y][x].getType() == 1)
		{
			return false;
		}
	}

	return true;
}

bool	Board::checkRowVisibility(int y, int x1, int x2)
{
	if (x1 > x2)
	{
		int swp = x1;
		x1 = x2;
		x2 = swp;
	}

	for (int x = x1; x <= x2; x++)
	{
		if (mCases[y][x].getType() == 1)
		{
			return false;
		}
	}

	return true;
}

v2i	Board::ghostSeePacman(const v2i& pos)
{
	if (mPlayer->isDead())
		return { -1,-1 };
	auto poses = mPlayer->getPoses();

	for (const auto& p : poses)// first check if on the same line or column
	{
		if (p.x == pos.x)
		{
			if (checkColumnVisibility(p.x, p.y, pos.y))
			{
				return p;
			}
		}
		if (p.y == pos.y)
		{
			if (checkRowVisibility(p.y, p.x, pos.x))
			{
				return p;
			}
		}
	}

	return {-1,-1};
}

void	Board::checkEat(const v2i& pos)
{
	bool needRemoveGraphicRep = false;
	switch (mCases[pos.y][pos.x].getType())
	{
	case 4:
		mPlayer->startHunting();
		mDeltaScore += 90;
		mEatCount--;
	case 2:
		mDeltaScore += 10;
		mEatCount++;
		needRemoveGraphicRep = true;
		break;
	}

	if (needRemoveGraphicRep)
	{
		mLabyBG->removeItem(mCases[pos.y][pos.x].getGraphicRepresentation());
		mCases[pos.y][pos.x].setType(0);
		mCases[pos.y][pos.x].setGraphicRepresentation(nullptr);
	}

}


std::vector<bool> Board::getAvailableDirection(const v2i& pos)
{
	std::vector<bool> result;
	result.reserve(4);
	
	bool checkGhostHouse = true;
	if (mCases[pos.y][pos.x].getType() == 3) // in ghost house
	{
		checkGhostHouse = false;
	}

	for (int direction = 0; direction < 4; direction++)
	{
		v2i destPos = pos + movesVector[direction];
		bool isOk = false;
		if ((destPos.x >= 0) && (destPos.x < mBoardSize.x))
		{
			if ((destPos.y >= 0) && (destPos.y < mBoardSize.y))
			{
				if (!checkForWallOnCase(destPos))
				{
					// also check for ghost house
					isOk = true;
					if (checkGhostHouse) // not in ghost house, so ghost house cases are not available anymore
					{
						if (mCases[destPos.y][destPos.x].getType() == 3)
						{
							isOk = false;
						}
					}
				}
			}
		}
		result.push_back(isOk);
	}
	return result;
}