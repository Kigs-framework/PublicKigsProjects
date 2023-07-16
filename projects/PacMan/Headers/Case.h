#pragma once
#include "CoreModifiable.h"

namespace Kigs
{
	using namespace Core;

	class Case
	{
	protected:

		u32		mInitType;
		u32		mCurrentType;
		CMSP	mGraphicRep = nullptr;
	public:

		Case() : mInitType(-1), mCurrentType(-1)
		{

		}

		Case(u32 initType) : mInitType(initType), mCurrentType(initType)
		{

		}

		void setInitType(u32 initType)
		{
			if (mInitType != (u32)-1) // already set
			{
				return;
			}
			mInitType = initType;
			mCurrentType = initType;
		}

		void setType(u32 cType)
		{
			if (mCurrentType == (u32)-1)
			{
				// ERROR
				return;
			}
			mCurrentType = cType;
		}

		u32 getType() const
		{
			return mCurrentType;
		}

		void reset()
		{
			mCurrentType = mInitType;
		}

		void	setGraphicRepresentation(CMSP rep)
		{
			mGraphicRep = rep;
		}

		CMSP getGraphicRepresentation()
		{
			return mGraphicRep;
		}
	};
}