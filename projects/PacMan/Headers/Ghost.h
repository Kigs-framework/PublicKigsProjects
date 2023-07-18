#pragma once

#include "CoreModifiable.h"
#include "CoreFSMState.h"
#include "CharacterBase.h"

namespace Kigs
{
	class Board;

	class Ghost : public CharacterBase
	{
	public:
		DECLARE_CLASS_INFO(Ghost, CharacterBase, PacMan);
		DECLARE_CONSTRUCTOR(Ghost);

		void	InitModifiable() override;

		template<typename T>
		friend class Upgrador;

		bool isHunted()
		{
			return mIsHunted;
		}

		void setHunted(bool h)
		{
			mIsHunted = h;
		}
	protected:

		void	checkForNewDirectionNeed();
		void	chooseNewDirection(int prevdirection, int prevdirweight = 1);

		maString mName = BASE_ATTRIBUTE(Name, "");

		bool	mIsHunted = false;
	};
	using namespace Kigs::Core;
	using namespace Kigs::Fsm;

	START_DECLARE_COREFSMSTATE(Ghost, Appear)
	COREFSMSTATE_WITHOUT_METHODS()
	END_DECLARE_COREFSMSTATE()

	START_DECLARE_COREFSMSTATE(Ghost, FreeMove)
	STARTCOREFSMSTATE_WRAPMETHODS();
	bool	seePacMan();
	ENDCOREFSMSTATE_WRAPMETHODS(seePacMan)
	END_DECLARE_COREFSMSTATE()

	START_DECLARE_COREFSMSTATE(Ghost, Hunting)
	v2i	mPacmanSeenPos;
	COREFSMSTATE_WITHOUT_METHODS()
	END_DECLARE_COREFSMSTATE()

	START_DECLARE_COREFSMSTATE(Ghost, Hunted)
	STARTCOREFSMSTATE_WRAPMETHODS();
	bool	checkDead();
	ENDCOREFSMSTATE_WRAPMETHODS(checkDead)
	END_DECLARE_COREFSMSTATE()

	START_DECLARE_COREFSMSTATE(Ghost, Die)
	COREFSMSTATE_WITHOUT_METHODS()
	END_DECLARE_COREFSMSTATE()
}