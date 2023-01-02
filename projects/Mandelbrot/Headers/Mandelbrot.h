#pragma once

#include "DataDrivenBaseApplication.h"
#include "KigsBitmap.h"
#include "UI/UIItem.h"

namespace Kigs
{
	namespace Pict
	{
		class TinyImage;
	}
	using namespace DDriven;

	class Mandelbrot : public DataDrivenBaseApplication
	{
	public:
		DECLARE_CLASS_INFO(Mandelbrot, DataDrivenBaseApplication, Core);
		DECLARE_CONSTRUCTOR(Mandelbrot);

	protected:
		void	ProtectedInit() override;
		void	ProtectedUpdate() override;
		void	ProtectedClose() override;


		void	ProtectedInitSequence(const std::string& sequence) override;
		void	ProtectedCloseSequence(const std::string& sequence) override;

		SP<Draw::KigsBitmap>	mBitmap;
		SP<Draw2D::UIItem>		mBitmapDisplay;

		float			mZoomCoef;

		double			mStartTime = -1.0;
		double			mRotationAngle = 0.0f;

		SP<Pict::TinyImage>	mImage = nullptr;
	};
}