#pragma once

#include <DataDrivenBaseApplication.h>
#include "KigsBitmap.h"
#include "UI/UIItem.h"

class Mandelbrot : public DataDrivenBaseApplication
{
public:
	DECLARE_CLASS_INFO(Mandelbrot, DataDrivenBaseApplication, Core);
	DECLARE_CONSTRUCTOR(Mandelbrot);

protected:
	void	ProtectedInit() override;
	void	ProtectedUpdate() override;
	void	ProtectedClose() override;

	
	void	ProtectedInitSequence(const kstl::string& sequence) override;
	void	ProtectedCloseSequence(const kstl::string& sequence) override;

	SP<KigsBitmap>	mBitmap;
	SP<UIItem>		mBitmapDisplay;

	float			mZoomCoef;

	double			mStartTime = -1.0;
	double			mRotationAngle = 0.0f;
};
