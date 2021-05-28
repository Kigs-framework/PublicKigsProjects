#include <Mandelbrot.h>
#include <FilePathManager.h>
#include <NotificationCenter.h>
#include "TinyImage.h"

extern void	DrawMandelbrot(unsigned char* pixelsdata, int sizeX, int sizeY, float zoomCenterX, float zoomCenterY, float zoomCoef,TinyImage* bmp);

IMPLEMENT_CLASS_INFO(Mandelbrot);

IMPLEMENT_CONSTRUCTOR(Mandelbrot)
{

}

void	Mandelbrot::ProtectedInit()
{
	// Base modules have been created at this point

	// lets say that the update will sleep 1ms
	SetUpdateSleepTime(1);

	SP<FilePathManager> pathManager = KigsCore::Singleton<FilePathManager>();
	pathManager->AddToPath(".", "xml");


	// Load AppInit, GlobalConfig then launch first sequence
	DataDrivenBaseApplication::ProtectedInit();

	mZoomCoef = 10.0f;

	//mImage =TinyImage::CreateImage("Anneau.png");
	mImage = TinyImage::CreateImage("copper.png");

}

void	Mandelbrot::ProtectedUpdate()
{

	static int count_frame = 0;
	DataDrivenBaseApplication::ProtectedUpdate();

	if (mBitmap)
	{
		if (mStartTime < 0.0)
		{
			mStartTime = DataDrivenBaseApplication::GetApplicationTimer()->GetTime();
		}
		v2f bitmapSize = mBitmap->getValue<v2f>("Size");

		DrawMandelbrot(mBitmap->GetPixelBuffer(), (int)bitmapSize.x, (int)bitmapSize.y, -0.743643887037151, 0.13182590420533, mZoomCoef, mImage.get());
		mZoomCoef *= 1.01;
		count_frame++;
		double totalTime = DataDrivenBaseApplication::GetApplicationTimer()->GetTime() - mStartTime;
		if (count_frame == 100)
		{
			
			printf("time per frame = %f\n", (float)totalTime/100.0f);
			count_frame = 0;
		}

		mBitmapDisplay->setValue("RotationAngle", mRotationAngle);
		mRotationAngle += 0.01*(0.8f+sinf(totalTime));
	}
}

void	Mandelbrot::ProtectedClose()
{
	mBitmap = nullptr;
	mBitmapDisplay = nullptr;
	DataDrivenBaseApplication::ProtectedClose();
}

void	Mandelbrot::ProtectedInitSequence(const kstl::string& sequence)
{
	if (sequence == "sequencemain")
	{
		mBitmap = GetFirstInstanceByName("KigsBitmap", "mandelbrot");
		mBitmapDisplay = GetFirstInstanceByName("UIImage", "uitexture");
		mBitmapDisplay->setValue("PreScale", v2f(1.4f,1.4f));
	}
}
void	Mandelbrot::ProtectedCloseSequence(const kstl::string& sequence)
{
	if (sequence == "sequencemain")
	{
		
	}
}

