#include "Player.h"
#include "Board.h"
#include "InputIncludes.h"
#include "CoreBaseApplication.h"
#include "NotificationCenter.h"

using namespace Kigs;

IMPLEMENT_CLASS_INFO(Player)

Player::Player(const std::string& name, CLASS_NAME_TREE_ARG) : CharacterBase(name, PASS_CLASS_NAME_TREE_ARG)
{

}

void	Player::InitModifiable()
{
	ParentClassType::InitModifiable();
	if (IsInit())
	{
		// graphic representation first
		mGraphicRepresentation = KigsCore::GetInstanceOf("pacman", "UIImage");
		mGraphicRepresentation->setValue("TextureName", "Pacman.json");
		mGraphicRepresentation->setValue("Anchor", v2f(0.5f, 0.5f));
		mGraphicRepresentation->setValue("Priority", 25);
		mGraphicRepresentation->setValue("CurrentAnimation", "pacman");
		mGraphicRepresentation->setValue("Loop", "true");
		mGraphicRepresentation->setValue("FramePerSecond", "4");
		mGraphicRepresentation->setValue("PreScale", "[1.2,1.2]");
		mBoard->getGraphicInterface()->addItem(mGraphicRepresentation);
		mGraphicRepresentation->Init();

		setCurrentPos(v2f(13.0f, 23.0f));
		mDestPos = v2i(13, 23);

		auto theInputModule = KigsCore::GetModule<Input::ModuleInput>();
		Input::KeyboardDevice* theKeyboard = theInputModule->GetKeyboard();
		KigsCore::Connect(theKeyboard, "KeyboardEvent", this, "UpdateKeyboard");

		mKeyDirection = { 0.0,-1 };
	}
}


void	Player::manageDeathState()
{
	if (mDeathTime < 0.0)
	{
		mDeathTime = KigsCore::GetCoreApplication()->GetApplicationTimer()->GetTime();
	}
	double currentTime = KigsCore::GetCoreApplication()->GetApplicationTimer()->GetTime();
	if ((currentTime - mDeathTime) < 2.0f)
	{
		mGraphicRepresentation->setValue("RotationAngle", (currentTime - mDeathTime)*6.0);
	}
	else
	{
		if (!mBoard->checkForGhostOnCase(v2i(13, 23))) // wait till a ghost is here
		{
			setCurrentPos(v2f(13.0f, 23.0f));
			mDestPos = v2i(13, 23);
			mKeyDirection = { 0.0,-1 };
			mIsDead = false;
			mDeathTime = -1.0;
			mGraphicRepresentation->setValue("RotationAngle", 0.0);
			mDirection = -1;
		}
	}
}


void Player::UpdateKeyboard(std::vector<KeyEvent>& keys)
{
	if (!keys.empty())
	{
		for (auto& key : keys)
		{
			if (key.Action != key.ACTION_DOWN)
				continue;
			
			const auto& t = KigsCore::GetCoreApplication()->GetApplicationTimer();
			double currentT = t->GetTime();

			if (key.KeyCode == VK_LEFT)//Left
			{
				mKeyDirection = { currentT,2 };
			}
			else if (key.KeyCode == VK_RIGHT)//Right
			{
				mKeyDirection = { currentT,0 };
			}
			else if (key.KeyCode == VK_UP)//Up
			{
				mKeyDirection = { currentT,3 };
			}
			else if (key.KeyCode == VK_DOWN)//Down
			{
				mKeyDirection = { currentT,1 };
			}
		}
	}
}



void Player::Update(const Time::Timer& timer, void* addParam)
{
	if (mIsDead)
	{
		manageDeathState();
		return;
	}

	v2f newpos = mCurrentPos;
	v2f rpos = getRoundPos();

	bool haskeypressed = (mKeyDirection.second >= 0);
	double currentT = timer.GetTime();
	if ((currentT - mKeyDirection.first) >= 1.5)
	{
		haskeypressed = false;
	}

	int prevDir = mDirection;

	if (mDirection >= 0)
	{
		bool destReached = moveToDest(timer, newpos);

		if (destReached)
		{
			bool teleport = mBoard->manageTeleport(mDestPos, mDirection, this);
			if (teleport)
			{
				newpos = mCurrentPos;
			}
			else
			{
				// check if it's possible to change direction
				std::vector<bool> availableCases = mBoard->getAvailableDirection(mDestPos);
				// if direction is not available continue
				if (haskeypressed)
				{
					if (!availableCases[mKeyDirection.second])
					{
						haskeypressed = false;
					}
				}

				if (availableCases[mDirection] && (!haskeypressed)) // continue on it's path
				{
					mCurrentPos = mDestPos;
					mDestPos = rpos;
					mDestPos += movesVector[mDirection];
				}
				else // can choose another direction
				{
					mCurrentPos = mDestPos;
					newpos = mCurrentPos;
					rpos = getRoundPos();
					mDirection = -1;
				}
			}
		}

		setCurrentPos(newpos);
	}

	if (mDirection == -1) // no given direction
	{
		std::vector<bool> availableCases = mBoard->getAvailableDirection(rpos);

		if (mKeyDirection.second >= 0)
		{
			if (availableCases[mKeyDirection.second])
			{
				double currentT = timer.GetTime();
				if ((currentT - mKeyDirection.first) < 1.5)
				{
					mDirection = mKeyDirection.second;
					mDestPos = rpos;
					mDestPos += movesVector[mDirection];
				}
			}
		}
	}
	if ((mDirection >= 0) && (prevDir != mDirection))
	{
		mGraphicRepresentation->setValue("RotationAngle", ((float)mDirection) * glm::pi<float>() * 0.5f);
	}

	mBoard->checkEat(rpos);

}

void Player::startHunting()
{
	if (mIsDead)
		return;

	KigsCore::GetNotificationCenter()->postNotificationName("PacManHunting");

}
