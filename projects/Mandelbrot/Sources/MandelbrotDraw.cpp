#include <stdio.h>
#include <string.h>
#include <math.h>

int IterationMax = 64;

struct RGBA
{
	unsigned char R;
	unsigned char G;
	unsigned char B;
	unsigned char A;
};

int getIndex(int posX, int posY, int sizeX, int sizeY)
{
	// no check, we know we are inside array
	//if ((posX >= 0) && (posX < sizeX) && (posY >= 0) && (posY < sizeY))
	{
		return posX + posY * sizeX;
	}
	return -1;
}

inline void colorFromIteration(RGBA* currentColor,int Iteration)
{
	if (Iteration == IterationMax)
	{
		currentColor->R = 0;
		currentColor->G = 0;
		currentColor->B = 0;
		currentColor->A = 255;
	}
	else
	{
		currentColor->R = (unsigned char)((Iteration << 4) & 255);
		currentColor->G = 0;
		currentColor->B = (unsigned char)(((Iteration >> 4) & 255) << 4);
		currentColor->A = 255;
	}
}

inline int iterationLoop(float Cx, float Cy)
{
	float Zx = Cx;
	float Zy = Cy;
	float Zx2 = Zx * Zx;
	float Zy2 = Zy * Zy;
	const float ER2 = 4.0f;


	int Iteration = 0;
	for (Iteration = 0; Iteration < IterationMax && ((Zx2 + Zy2) < ER2); Iteration++)
	{
		Zy = 2.0f * Zx * Zy + Cy;
		Zx = Zx2 - Zy2 + Cx;
		Zx2 = Zx * Zx;
		Zy2 = Zy * Zy;
	}

	return Iteration;
}

void	drawRectangle(unsigned char* pixelsdata, int sizeX, int sizeY, int startX, int startY, int RectSizeX,int RectSizeY, float startCx, float startCy, float Dx, float Dy)
{
	// draw square border 
	RGBA* rgbaPixels = (RGBA*)pixelsdata;
	int Iteration = 0;
	// first draw horizontal borders
	RGBA currentColor;
	for (int j = 0; j < RectSizeY; j++)
	{
		float Cy = startCy + j * Dy;
		int index = getIndex(startX, j + startY, sizeX, sizeY);

		for (int i = 0; i < RectSizeX; i++)
		{
			float Cx = startCx + i * Dx;

			Iteration = iterationLoop(Cx, Cy);
			colorFromIteration(&currentColor, Iteration);	
			rgbaPixels[index] = currentColor;
			index++;
		}
	}

}

void	drawRecursiveSquare(unsigned char* pixelsdata, int sizeX, int sizeY,int startX,int startY, int SquareSize,float startCx,float startCy,float Dx,float Dy)
{
	// draw square border 
	RGBA* rgbaPixels = (RGBA*)pixelsdata;

	int checkRecurse1 = 0;
	int checkRecurse2 = 0;

	int Iteration = 0;
	// first draw horizontal borders
	RGBA currentColor;
	for (int j = 0; j < SquareSize; j+= SquareSize-1)
	{
		float Cy = startCy + j * Dy;

		for (int i = 0; i < SquareSize; i++)
		{
			float Cx = startCx + i * Dx;

			Iteration = iterationLoop(Cx, Cy);
			checkRecurse1 |= Iteration;
			checkRecurse2 += Iteration;
			colorFromIteration(&currentColor, Iteration);
			int index = getIndex(i+startX, j+startY, sizeX, sizeY);
			rgbaPixels[index] = currentColor;
		}
	}

	// then draw vertical borders
	for (int i = 0; i < SquareSize; i += SquareSize - 1)
	{
		float Cx = startCx + i * Dx;
		
		for (int j = 1; j < SquareSize-1; j++)
		{
			float Cy = startCy + j * Dy;

			Iteration = iterationLoop(Cx, Cy);
			checkRecurse1 |= Iteration;
			checkRecurse2 += Iteration;
			colorFromIteration(&currentColor, Iteration);
			int index = getIndex(i + startX, j + startY, sizeX, sizeY);
			rgbaPixels[index] = currentColor;
			
		}
	}

	int countIteration = 4 * (SquareSize-1);

	if ((checkRecurse1 == Iteration) && (checkRecurse2 == (Iteration* countIteration)))// fill square
	{
		for (int i = 1; i < SquareSize - 1; i++)
		{
			int index = getIndex(1 + startX, i + startY, sizeX, sizeY);
			for (int j = 1; j < SquareSize - 1; j++)
			{
				rgbaPixels[index] = currentColor;
				index++;
			}
		}
		
	}
	else  // need recurse
	{
		if (SquareSize == 3)
		{
			float Cy = startCy + Dx;
			float Cx = startCx + Dy;

			int Iteration = iterationLoop(Cx, Cy);
			int index = getIndex(startX + 1, startY + 1, sizeX, sizeY);
			colorFromIteration(&currentColor, Iteration);
			rgbaPixels[index] = currentColor;
		}
		else
		{
			int subSize = (SquareSize - 2) / 2;
			drawRecursiveSquare(pixelsdata, sizeX, sizeY, startX + 1, startY + 1, subSize, startCx + Dx * (1.0f), startCy + Dy * (1.0f), Dx, Dy);
			drawRecursiveSquare(pixelsdata, sizeX, sizeY, startX + 1 + subSize, startY + 1, subSize, startCx + Dx * (1.0f + subSize), startCy + Dy * (1.0f), Dx, Dy);
			drawRecursiveSquare(pixelsdata, sizeX, sizeY, startX + 1, startY + 1 + subSize, subSize, startCx + Dx * (1.0f), startCy + Dy * (1.0f + subSize), Dx, Dy);
			drawRecursiveSquare(pixelsdata, sizeX, sizeY, startX + 1 + subSize, startY + 1 + subSize, subSize, startCx + Dx * (1.0f + subSize), startCy + Dy * (1.0f + subSize), Dx, Dy);
		}
	}
}

void	DrawMandelbrot(unsigned char* pixelsdata, int sizeX, int sizeY, float zoomCenterX, float zoomCenterY, float zoomCoef)
{
	RGBA* rgbaPixels = (RGBA*)pixelsdata;

	memset(rgbaPixels, 0, sizeof(RGBA) * sizeX * sizeY);

	float oneOnZoomCoef = 1.0f / zoomCoef;

	IterationMax = sqrtf(zoomCoef);
	if (IterationMax > 255)
	{
		IterationMax = 255;
	}

	#pragma omp parallel for
	for (int j = 0; j < sizeY-78; j+=78)
	{
		float Cy = (j - sizeY/2) * oneOnZoomCoef + zoomCenterY;

		for (int i = 0; i < sizeX-78; i+=78)
		{
			float Cx = (i - sizeX/2) * oneOnZoomCoef + zoomCenterX;

			drawRecursiveSquare(pixelsdata, sizeX, sizeY, i, j, 78, Cx, Cy, oneOnZoomCoef, oneOnZoomCoef);
		}
	}

	int testX = sizeX / 78;
	int rectStartX = testX * 78;
	int rectSizeX = sizeX - rectStartX;
	int testY = sizeY / 78;
	int rectStartY = testY * 78;
	int rectSizeY = sizeY - rectStartY;

	float Cx = (rectStartX - sizeX / 2) * oneOnZoomCoef + zoomCenterX;
	float Cy = (0 - sizeY / 2) * oneOnZoomCoef + zoomCenterY;

	drawRectangle(pixelsdata, sizeX, sizeY, rectStartX, 0, rectSizeX, sizeY, Cx, Cy, oneOnZoomCoef, oneOnZoomCoef);

	Cx = (0 - sizeX / 2) * oneOnZoomCoef + zoomCenterX;
	Cy = (rectStartY - sizeY / 2)* oneOnZoomCoef + zoomCenterY;

	drawRectangle(pixelsdata, sizeX, sizeY, 0, rectStartY, rectStartX, rectSizeY, Cx, Cy, oneOnZoomCoef, oneOnZoomCoef);
}