#include "Ghost.h"
#include "Core.h"
#include "CoreFSM.h"
#include "Board.h"
#include "Timer.h"

using namespace Kigs;
using namespace Kigs::Fsm;

#define DEFAULT_SPEED	4.0f
#define LOW_SPEED		2.0f
#define HIGH_SPEED		5.0f

IMPLEMENT_CLASS_INFO(Ghost)

Ghost::Ghost(const std::string& name, CLASS_NAME_TREE_ARG) : CharacterBase(name, PASS_CLASS_NAME_TREE_ARG)
{

}

void	Ghost::InitModifiable()
{
	ParentClassType::InitModifiable();
	if (IsInit())
	{
		// graphic representation first

		std::string ghostName="Pacman.json:";
		ghostName += (std::string)mName;

		mGraphicRepresentation = KigsCore::GetInstanceOf("ghost", "UIImage");
		mGraphicRepresentation->setValue("TextureName", ghostName);
		mGraphicRepresentation->setValue("Anchor", v2f(0.5f, 0.5f));
		mGraphicRepresentation->setValue("Priority", 15);
		mGraphicRepresentation->setValue("PreScale", "[1.2,1.2]");
		mBoard->getGraphicInterface()->addItem(mGraphicRepresentation);
		mGraphicRepresentation->Init();

		// add FSM
		SP<CoreFSM> fsm = KigsCore::GetInstanceOf("fsm", "CoreFSM");
		addItem(fsm);
		// Appear state
		fsm->addState("Appear", new CoreFSMStateClass(Ghost, Appear)());
		SP<CoreFSMTransition> wait = KigsCore::GetInstanceOf("wait", "CoreFSMDelayTransition");
		wait->setState("FreeMove");
		wait->Init();
		fsm->getState("Appear")->addTransition(wait);

		// FreeMove state
		fsm->addState("FreeMove", new CoreFSMStateClass(Ghost, FreeMove)());
		SP<CoreFSMTransition> pacmanHunting = KigsCore::GetInstanceOf("pacmanHunting", "CoreFSMOnEventTransition");
		pacmanHunting->setValue("EventName", "PacManHunting");
		pacmanHunting->setState("Hunted");
		pacmanHunting->Init();
		fsm->getState("FreeMove")->addTransition(pacmanHunting);
		SP<CoreFSMTransition> hunt = KigsCore::GetInstanceOf("hunt", "CoreFSMOnMethodTransition");
		hunt->setValue("MethodName", "seePacMan");
		hunt->setState("Hunting");
		hunt->Init();
		fsm->getState("FreeMove")->addTransition(hunt);


		// pacman die transition
		SP<CoreFSMTransition> pacmanDie = KigsCore::GetInstanceOf("pacmanDie", "CoreFSMOnEventTransition");
		pacmanDie->setValue("EventName", "PacManDie");
		pacmanDie->setState("FreeMove");
		pacmanDie->Init();

		// Hunting state
		fsm->addState("Hunting", new CoreFSMStateClass(Ghost, Hunting)());
		fsm->getState("Hunting")->addTransition(pacmanHunting);
		fsm->getState("Hunting")->addTransition(pacmanDie);
		SP<CoreFSMTransition> pacmanNotVisible = KigsCore::GetInstanceOf("pacmanNotVisible", "CoreFSMOnValueTransition");
		pacmanNotVisible->setValue("ValueName", "PacManNotVisible");
		pacmanNotVisible->setState("FreeMove");
		pacmanNotVisible->Init();
		fsm->getState("Hunting")->addTransition(pacmanNotVisible);

		// Hunted state
		fsm->addState("Hunted", new CoreFSMStateClass(Ghost, Hunted)());
		SP<CoreFSMTransition> die = KigsCore::GetInstanceOf("die", "CoreFSMOnMethodTransition");
		die->setValue("MethodName", "checkDead");
		die->setState("Die");
		fsm->getState("Hunted")->addTransition(die);
		SP<CoreFSMTransition> HuntedEnd = KigsCore::GetInstanceOf("HuntedEnd", "CoreFSMDelayTransition");
		HuntedEnd->setState("FreeMove");
		HuntedEnd->setValue("Delay", 7.0f);
		HuntedEnd->Init();
		fsm->getState("Hunted")->addTransition(HuntedEnd);

		// Die state
		fsm->addState("Die", new CoreFSMStateClass(Ghost, Die)());
		SP<CoreFSMTransition> waitresurect = KigsCore::GetInstanceOf("waitresurect", "CoreFSMDelayTransition");
		waitresurect->setState("Appear");
		waitresurect->Init();
		fsm->getState("Die")->addTransition(waitresurect);


		fsm->setStartState("Appear");
		fsm->Init();

	}
	
}

void Ghost::checkForNewDirectionNeed()
{
	// check if it's possible to change direction
	std::vector<bool> availableCases = mBoard->getAvailableDirection(mDestPos);

	int count_available = 0;
	for (int tst = 0; tst < 4; tst++)
	{
		if (tst == 2) // don't look back for this tst
			continue;

		int tstDir = (mDirection + tst) % 4;

		v2i dircase = mDestPos;
		dircase += movesVector[tstDir];

		if (mBoard->checkForGhostOnCase(dircase, this)) // this case is occupied
		{
			availableCases[tstDir] = false;
		}

		if (availableCases[tstDir])
		{
			count_available++;
		}
	}

	if ((availableCases[mDirection]) && (count_available == 1)) // ghost can continue on it's path
	{
		mDestPos += movesVector[mDirection];
	}
	else // choose another direction
	{
		mCurrentPos = mDestPos;
		mDirection = -1;
	}
}

void	Ghost::chooseNewDirection(int prevdirection, int prevdirweight)
{
	v2i rpos = getRoundPos();
	std::vector<bool> availableCases = mBoard->getAvailableDirection(rpos);

	std::vector<std::pair<int, v2i>> reallyAvailable;

	// check if other ghost is on available case
	for (int direction = 0; direction < 4; direction++)
	{
		v2i dircase = rpos;
		dircase += movesVector[direction];

		if (mBoard->checkForGhostOnCase(dircase, this))
		{
			availableCases[direction] = false;
		}

		reallyAvailable.push_back({ availableCases[direction] ? direction : -1,dircase });
	}

	// duplicate current direction, and side direction ( give more weight to move forward )
	if (prevdirection >= 0)
	{
		int halfprevdir = (prevdirweight / 2) + 1;
		int cornerprevdir = (prevdirweight / 6) + 1;
		for (int w = 0; w < halfprevdir; w++)
		{
			reallyAvailable.push_back(reallyAvailable[prevdirection]);
		}
		for (int w = 0; w < cornerprevdir; w++)
		{
			for (int direction = 0; direction < 4; direction++)
			{
				if (direction == 2)
					continue;

				int tstDir = (prevdirection + direction) % 4;
				reallyAvailable.push_back(reallyAvailable[tstDir]);
			}
		}
	}

	// then just keep available dirs
	std::vector<std::pair<int, v2i>> onlyAvailable;
	for (int direction = 0; direction < reallyAvailable.size(); direction++)
	{
		if (reallyAvailable[direction].first >= 0)
		{
			onlyAvailable.push_back(reallyAvailable[direction]);
		}
	}


	if (onlyAvailable.size())
	{
		/*float stats[4] = { 0.0f,0.0f,0.0f,0.0f };
		for (auto o : onlyAvailable)
		{
			stats[o.first] += 1.0f;
		}
		*/
		auto choose = onlyAvailable[rand() % onlyAvailable.size()];
		mDirection = choose.first;
		mDestPos = choose.second;

		/*if(prevdirweight>1)
			printf("choosed direction chances = %f\n", stats[mDirection] / onlyAvailable.size());*/

	}
}

// FSM

void CoreFSMStartMethod(Ghost, Appear)
{
	Board* b = getBoard();
	// set ghost pos
	if (b)
	{
		v2i pos = b->getAppearPosForGhost();
		initAtPos(pos);
		setDead(false); // happy resurection
	}
	getGraphicRepresentation()->setValue("RotationAngle", 0);

#ifdef DEBUG_COREFSM
	SP<CoreFSM>	fsm = GetFirstSonByName("CoreFSM", "fsm");
	fsm->dumpLastStates();
#endif
}

void CoreFSMStopMethod(Ghost, Appear)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(Ghost, Appear))
{
	// empty update
	return false;
}

void	CoreFSMStartMethod(Ghost, FreeMove)
{
	getGraphicRepresentation()->setValue("RotationAngle", 0);
}
void	CoreFSMStopMethod(Ghost, FreeMove)
{

}

bool CoreFSMStateClassMethods(Ghost, FreeMove)::seePacMan()
{
	v2i rpos = getRoundPos();
	v2i pmpos = mBoard->ghostSeePacman(rpos);
	if (pmpos.x != -1)
	{
		return true;
	}

	return false;
}


// update 
DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(Ghost, FreeMove))
{
	v2f newpos = mCurrentPos;
	int prevdirection = mDirection;

	if (mDirection >= 0)
	{
		bool destReached = moveToDest(timer, newpos);
		
		if (destReached)
		{
			checkForNewDirectionNeed();
		}

		if (mDirection == -1)
		{
			newpos = mCurrentPos;
		}

		setCurrentPos(newpos);
	}

	if (mDirection == -1) // no given direction
	{
		chooseNewDirection(prevdirection);
	}
	return false;
}

void	CoreFSMStartMethod(Ghost, Hunting)
{
	GetUpgrador()->mPacmanSeenPos = getBoard()->ghostSeePacman(getCurrentPos());
	// compute direction
	getGraphicRepresentation()->setValue("RotationAngle", glm::pi<float>());

	AddDynamicAttribute(CoreModifiable::ATTRIBUTE_TYPE::BOOL, "PacManNotVisible", false);

	// higher speed a bit
	setSpeed(HIGH_SPEED);
}
void	CoreFSMStopMethod(Ghost, Hunting)
{
	RemoveDynamicAttribute("PacManNotVisible");
	if (getGraphicRepresentation())
	{
		getGraphicRepresentation()->setValue("RotationAngle", 0.0f);
	}
	// go back to normal speed 
	setSpeed(DEFAULT_SPEED);
}

// update
DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(Ghost, Hunting))
{
	v2i lastPacmanpos=GetUpgrador()->mPacmanSeenPos;

	v2f newpos = mCurrentPos;
	v2i	currenPos = getRoundPos();

	v2i pmpos = mBoard->ghostSeePacman(currenPos);
	if (pmpos.x != -1) // update last pacmanpos
	{
		GetUpgrador()->mPacmanSeenPos = pmpos;
		lastPacmanpos = pmpos;
	}

	int prevdirection = mBoard->directionFromDelta(lastPacmanpos- currenPos);

	if (mDirection >= 0)
	{
		bool destReached = moveToDest(timer, newpos);

		if (destReached)
		{
			if ((pmpos.x == -1) && (lastPacmanpos == mDestPos)) // pacman not found and last pacman pos reached, go back to freemove
			{
				setValue("PacManNotVisible", true);
			}
			checkForNewDirectionNeed();
		}

		if (mDirection == -1)
		{
			newpos = mCurrentPos;
		}

		setCurrentPos(newpos);
	}

	if (mDirection == -1) // no given direction
	{
		//printf("hunting direction = %d \n", prevdirection);
		chooseNewDirection(prevdirection,12); 
		//printf("choosed direction = %d \n", mDirection);
	}
	return false;
}


void	CoreFSMStartMethod(Ghost, Hunted)
{
	std::string ghostName = "Pacman.json:blue_ghost";
	getGraphicRepresentation()->setValue("TextureName", ghostName);
	setHunted(true);

	// lower speed a bit
	setSpeed(LOW_SPEED);
}
void	CoreFSMStopMethod(Ghost, Hunted)
{
	if (getGraphicRepresentation())
	{
		std::string ghostName = "Pacman.json:";
		ghostName += getValue<std::string>("Name");
		getGraphicRepresentation()->setValue("TextureName", ghostName);
	}
	setHunted(false);

	// go back to "normal" speed
	setSpeed(DEFAULT_SPEED);
}

bool CoreFSMStateClassMethods(Ghost, Hunted)::checkDead()
{
	if (mIsDead)
	{
		return true;
	}
	return false;
}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(Ghost, Hunted))
{

	SP<CoreFSMDelayTransition> delaytrans = GetUpgrador()->getTransition("HuntedEnd");
	if (delaytrans)
	{
		// less than 2 seconds, make the ghost flash
		double remain=delaytrans->getRemainingTime();
		if (remain < 2.0f)
		{
			int texture = round(remain * 8.0f);
			std::string ghostName = "Pacman.json:blue_ghost";
			if (texture & 1)
			{
				ghostName = "Pacman.json:";
				ghostName += (std::string) mName;
			}
			getGraphicRepresentation()->setValue("TextureName", ghostName);
		}
	}

	v2f newpos = mCurrentPos;
	v2i	currenPos = getRoundPos();

	v2i pmpos = mBoard->ghostSeePacman(currenPos);

	int prevdirection = mDirection;

	if (pmpos.x != -1)
	{
		prevdirection = mBoard->directionFromDelta(currenPos - pmpos);
	}

	if (mDirection >= 0)
	{
		bool destReached = moveToDest(timer, newpos);

		if (destReached)
		{
			checkForNewDirectionNeed();
		}

		if (mDirection == -1)
		{
			newpos = mCurrentPos;
		}

		setCurrentPos(newpos);
	}

	if (mDirection == -1) // no given direction
	{
		chooseNewDirection(prevdirection, 8);
	}
	return false;
}


void CoreFSMStartMethod(Ghost, Die)
{
	
}

void CoreFSMStopMethod(Ghost, Die)
{

}

DEFINE_UPGRADOR_UPDATE(CoreFSMStateClass(Ghost, Die))
{
	SP<CoreFSMDelayTransition> delaytrans = GetUpgrador()->getTransition("waitresurect");
	if (delaytrans)
	{
		double elapsed = delaytrans->getElapsedTime();
		getGraphicRepresentation()->setValue("RotationAngle", 1.6f*sqrtf(elapsed));
	}
	return false;
}