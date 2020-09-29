#include <Mandelbrot.h>
#include <FilePathManager.h>
#include <NotificationCenter.h>

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

		v2f zoomCenter(-0.743643887037151, 0.13182590420533);

		for (int j = 0; j < 800; j++)
		{
			float Cy = (j - 400) * (1.0f / mZoomCoef) + zoomCenter.y;

			for (int i = 0; i < 1280; i++)
			{
				float Cx = (i - 640) * (1.0f / mZoomCoef) + zoomCenter.x;
				float Zx = 0.0;
				float Zy = 0.0;
				float Zx2 = 0.0;
				float Zy2 = 0.0;
				float ER2 = 4.0f;

				int IterationMax = 32;
				int Iteration = 0;
				for (Iteration = 0; Iteration < IterationMax && ((Zx2 + Zy2) < ER2); Iteration++)
				{
					Zy = 2 * Zx * Zy + Cy;
					Zx = Zx2 - Zy2 + Cx;
					Zx2 = Zx * Zx;
					Zy2 = Zy * Zy;
				}
				mBitmap->PutPixel(i,j,{(unsigned char)((Iteration<<4)&255),0,(unsigned char)(((Iteration>>4) & 255)<<4),255 });
			}
		}

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

