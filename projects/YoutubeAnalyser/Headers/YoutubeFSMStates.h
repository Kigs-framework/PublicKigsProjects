#pragma once

#include "YoutubeAnalyser.h"
#include "CoreFSMState.h"
#include "CoreFSM.h"

namespace Kigs
{
	using namespace Kigs::Core;
	using namespace Kigs::Fsm;
	// base state for Step State
	START_DECLARE_COREFSMSTATE(YoutubeAnalyser, BaseStepState)
public:
	u32		mStateStep = 0;
protected:
	STARTCOREFSMSTATE_WRAPMETHODS();
	void	nextStep()
	{
		GetUpgrador()->mStateStep++;
	}
	void	changeStep(u32 step)
	{
		GetUpgrador()->mStateStep = step;
	}
	ENDCOREFSMSTATE_EMPTYWRAPMETHODS()
	END_DECLARE_COREFSMSTATE()


	START_DECLARE_COREFSMSTATE(YoutubeAnalyser, InitChannel)
	COREFSMSTATE_WITHOUT_METHODS()
	END_DECLARE_COREFSMSTATE()

	// wait state
	START_DECLARE_COREFSMSTATE(YoutubeAnalyser, Wait)
	COREFSMSTATE_WITHOUT_METHODS()
	END_DECLARE_COREFSMSTATE()

	// everything is done
	START_DECLARE_COREFSMSTATE(YoutubeAnalyser, Done)
	COREFSMSTATE_WITHOUT_METHODS()
	END_DECLARE_COREFSMSTATE()

	START_INHERITED_COREFSMSTATE(YoutubeAnalyser, RetrieveVideo, BaseStepState)
protected:
	COREFSMSTATE_WITHOUT_METHODS()
	END_DECLARE_COREFSMSTATE()

	START_DECLARE_COREFSMSTATE(YoutubeAnalyser, GetVideo)
public:
	bool	mNeedMoreData = false;
protected:
	STARTCOREFSMSTATE_WRAPMETHODS();
	void	manageRetrievedVideo(const std::vector< YoutubeConnect::videoStruct>& videolist, const std::string& nextPage);
	ENDCOREFSMSTATE_WRAPMETHODS(manageRetrievedVideo)
	END_DECLARE_COREFSMSTATE()


	START_INHERITED_COREFSMSTATE(YoutubeAnalyser, ProcessVideo, BaseStepState)
public:
	std::string				mVideoID;
	std::set<std::string>	mAuthorList;
protected:
	STARTCOREFSMSTATE_WRAPMETHODS();
	void	manageRetrievedComment(const std::set<std::string>& commentlist, const std::string& nextPage);
	ENDCOREFSMSTATE_WRAPMETHODS(manageRetrievedComment)
	END_DECLARE_COREFSMSTATE()

	START_INHERITED_COREFSMSTATE(YoutubeAnalyser, TreatAuthors, BaseStepState)
public:
	std::vector<std::string>	mAuthorList;
	size_t						mCurrentTreatedAuthor=0;
	size_t						mValidAuthorCount = 0;
protected:
	COREFSMSTATE_WITHOUT_METHODS()
	END_DECLARE_COREFSMSTATE()

	START_INHERITED_COREFSMSTATE(YoutubeAnalyser, GetAuthorInfos, BaseStepState)
public:
	std::string						mAuthorID;
	bool							mIsValidAuthor;
	YoutubeConnect::UserStruct		mCurrentUser;
protected:
	COREFSMSTATE_WITHOUT_METHODS()
	END_DECLARE_COREFSMSTATE()

	START_INHERITED_COREFSMSTATE(YoutubeAnalyser, RetrieveSubscriptions, BaseStepState)
public:
	std::string									mAuthorID;
	std::string									mNextPageToken;
	std::vector<std::string>					mSubscriptions;
protected:
	STARTCOREFSMSTATE_WRAPMETHODS();
	void	manageRetrievedSubscriptions(const std::vector<std::string>& subscrlist, const std::string& nextPage);
	ENDCOREFSMSTATE_WRAPMETHODS(manageRetrievedSubscriptions)
	END_DECLARE_COREFSMSTATE()

}