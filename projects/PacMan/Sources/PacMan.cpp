#include "PacMan.h"
#include "FilePathManager.h"
#include "NotificationCenter.h"
#include "GameLoop.h"
#include "Ghost.h"
#include "CoreFSM.h"
#include "Player.h"

using namespace Kigs;

IMPLEMENT_CLASS_INFO(PacMan);

IMPLEMENT_CONSTRUCTOR(PacMan)
{

}

void	PacMan::ProtectedInit()
{
	// Base modules have been created at this point

	// lets say that the update will sleep 1ms
	SetUpdateSleepTime(1);

	SP<File::FilePathManager> pathManager = KigsCore::Singleton < File::FilePathManager > ();
	pathManager->AddToPath(".", "xml");
	pathManager->AddToPath(".", "kpkg");
	initCoreFSM();

	DECLARE_FULL_CLASS_INFO(KigsCore::Instance(), Ghost, Ghost, PacMan);
	DECLARE_FULL_CLASS_INFO(KigsCore::Instance(), Player, Player, PacMan);

	KigsCore::GetNotificationCenter()->addObserver(this, "GameOver", "GameOver");
	KigsCore::GetNotificationCenter()->addObserver(this, "LevelWon", "LevelWon");

	// Load AppInit, GlobalConfig then launch first sequence
	DataDrivenBaseApplication::ProtectedInit();
}

void	PacMan::ProtectedUpdate()
{
	DataDrivenBaseApplication::ProtectedUpdate();

	if (mGameLoop)
	{
		mGameLoop->update();
		mMainInterface["Score"]("Text") = "Score : " + std::to_string((int)mScore);
		mMainInterface["Lives"]("Text") = "Lives : " + std::to_string((int)mLives);

	}
}

void	PacMan::ProtectedClose()
{
	KigsCore::GetNotificationCenter()->removeObserver(this, "GameOver");
	KigsCore::GetNotificationCenter()->removeObserver(this, "LevelWon");

	DataDrivenBaseApplication::ProtectedClose();
}

void	PacMan::ProtectedInitSequence(const std::string& sequence)
{
	if (sequence == "sequencemain")
	{
		mMainInterface = GetFirstInstanceByName("UIItem", "Interface");
		mGameLoop = new GameLoop(mMainInterface);
	}
}
void	PacMan::ProtectedCloseSequence(const std::string& sequence)
{
	if (sequence == "sequencemain")
	{
		delete mGameLoop;
		mGameLoop = nullptr;
		mMainInterface = nullptr;
	}
}

void	PacMan::GameOver()
{
	auto gameovertxt = KigsCore::GetInstanceOf("gameovertxt", "UIText");
	gameovertxt->setValue("Priority", 50);
	gameovertxt->setValue("Text", "Game Over");
	gameovertxt->setValue("Dock", v2f(0.5f,0.5f));
	gameovertxt->setValue("Anchor", v2f(0.5f, 0.5f));
	gameovertxt->setValue("Size", "[-1, -1]");
	gameovertxt->setValue("Font", "Calibri.ttf");
	gameovertxt->setValue("FontSize",72);
	gameovertxt->setValue("MaxWidth", 1000);
	gameovertxt->setValue("TextAlignment", 1);
	gameovertxt->setValue("Color", "[0.8, 1.0, 1.0]");

	mMainInterface->addItem(gameovertxt);
	gameovertxt->Init();
}
void	PacMan::LevelWon()
{
	float increaseSpeed = mSpeedCoef;
	increaseSpeed *= 1.1f;
	mSpeedCoef = increaseSpeed;
	mGameLoop->reset(mSpeedCoef);
}
