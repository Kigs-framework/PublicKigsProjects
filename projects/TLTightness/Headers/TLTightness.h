#pragma once

#include "DataDrivenBaseApplication.h"
#include "CoreFSMState.h"
#include "HTTPConnect.h"
#include "Texture.h"
#include "TwitterConnect.h"

class TLTightness : public DataDrivenBaseApplication
{
public:
	DECLARE_CLASS_INFO(TLTightness, DataDrivenBaseApplication, Core);
	DECLARE_CONSTRUCTOR(TLTightness);

protected:

	void	ProcessingGraphicUpdate();

	void	ProtectedInit() override;
	void	ProtectedUpdate() override;
	void	ProtectedClose() override;

	void	ProtectedInitSequence(const kstl::string& sequence) override;
	void	ProtectedCloseSequence(const kstl::string& sequence) override;

	maBool	mNeedWait = BASE_ATTRIBUTE(NeedWait, false);

	void	requestDone();
	void	mainUserDone();

	WRAP_METHODS(requestDone, mainUserDone);

	// current treated user
	std::string						mUserName;
	u32								mCurrentUserIndex=0;
	SP<TwitterConnect>				mTwitterConnect;

	std::vector<TwitterConnect::UserStruct>	mRetreivedUsers;

	CMSP			mMainInterface;


};


START_DECLARE_COREFSMSTATE(TLTightness, InitTL)
COREFSMSTATE_WITHOUT_METHODS()
END_DECLARE_COREFSMSTATE()


START_DECLARE_COREFSMSTATE(TLTightness, Wait)
COREFSMSTATE_WITHOUT_METHODS()
END_DECLARE_COREFSMSTATE()

START_DECLARE_COREFSMSTATE(TLTightness, GetFollowing)
COREFSMSTATE_WITHOUT_METHODS()
END_DECLARE_COREFSMSTATE()

START_DECLARE_COREFSMSTATE(TLTightness, TreatUser)
COREFSMSTATE_WITHOUT_METHODS()
END_DECLARE_COREFSMSTATE()

START_DECLARE_COREFSMSTATE(TLTightness, GetUserDetail)
COREFSMSTATE_WITHOUT_METHODS()
END_DECLARE_COREFSMSTATE()

START_DECLARE_COREFSMSTATE(TLTightness, Done)
u32			mTotalReachableAccount=0;
float		mMeanScore = 0.0f;
COREFSMSTATE_METHODS(DoneGraphicUpdate)
END_DECLARE_COREFSMSTATE()