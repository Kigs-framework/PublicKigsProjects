#include <Mandelbrot.h>
#include <FilePathManager.h>
#include <NotificationCenter.h>

extern void	DrawMandelbrot(unsigned char* pixelsdata, int sizeX, int sizeY, float zoomCenterX, float zoomCenterY, float zoomCoef);

IMPLEMENT_CLASS_INFO(Mandelbrot);

IMPLEMENT_CONSTRUCTOR(Mandelbrot)
{

}

void	Mandelbrot::ProtectedInit()
{
	// Base modules have been created at this point

	// lets say that the update will sleep 1ms
	SetUpdateSleepTime(1);

	SP<FilePathManager>& pathManager = KigsCore::Singleton<FilePathManager>();
	pathManager->AddToPath(".", "xml");


	// Load AppInit, GlobalConfig then launch first sequence
	DataDrivenBaseApplication::ProtectedInit();

	mZoomCoef = 200.0f;
}

void	Mandelbrot::ProtectedUpdate()
{
	DataDrivenBaseApplication::ProtectedUpdate();

	if (!mBitmap.isNil())
	{
		v2f bitmapSize = mBitmap->getValue<v2f>("Size");

		DrawMandelbrot(mBitmap->GetPixelBuffer(), (int)bitmapSize.x, (int)bitmapSize.y, -0.743643887037151, 0.13182590420533, mZoomCoef);
		mZoomCoef += 10.0f;

	}
}

void	Mandelbrot::ProtectedClose()
{
	DataDrivenBaseApplication::ProtectedClose();
}

void	Mandelbrot::ProtectedInitSequence(const kstl::string& sequence)
{
	if (sequence == "sequencemain")
	{
		mBitmap = GetFirstInstanceByName("KigsBitmap", "mandelbrot");
	}
}
void	Mandelbrot::ProtectedCloseSequence(const kstl::string& sequence)
{
	if (sequence == "sequencemain")
	{
		
	}
}

