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
	float Zx = 0.0;
	float Zy = 0.0;
	float Zx2 = 0.0;
	float Zy2 = 0.0;
	float ER2 = 4.0f;


	int Iteration = 0;
	for (Iteration = 0; Iteration < IterationMax && ((Zx2 + Zy2) < ER2); Iteration++)
	{
		Zy = 2 * Zx * Zy + Cy;
		Zx = Zx2 - Zy2 + Cx;
		Zx2 = Zx * Zx;
		Zy2 = Zy * Zy;
	}

	return Iteration;
}

void	drawRecursiveSquare(unsigned char* pixelsdata, int sizeX, int sizeY,int startX,int startY, int SquareSize,float startCx,float startCy,float Dx,float Dy)
{
	// draw square border 
	RGBA* rgbaPixels = (RGBA*)pixelsdata;

	int needRecurse = 0;

	int prevIteration = -1;
	// first draw horizontal borders
	RGBA currentColor;
	for (int j = 0; j < SquareSize; j+= SquareSize-1)
	{
		float Cy = startCy + j * Dy;

		for (int i = 0; i < SquareSize; i++)
		{
			float Cx = startCx + i * Dx;

			int Iteration = iterationLoop(Cx, Cy);
			if (prevIteration == -1)
			{
				prevIteration = Iteration;
				colorFromIteration(&currentColor, Iteration);
			}
			else if(Iteration != prevIteration)
			{
				needRecurse = 1;
				prevIteration = Iteration;
				colorFromIteration(&currentColor, Iteration);
			}
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

			int Iteration = iterationLoop(Cx, Cy);
			if (Iteration != prevIteration)
			{
				needRecurse = 1;
				prevIteration = Iteration;
				colorFromIteration(&currentColor, Iteration);
			}
			int index = getIndex(i + startX, j + startY, sizeX, sizeY);
			rgbaPixels[index] = currentColor;
			
		}
	}

	if (needRecurse)
	{
		if (SquareSize == 3)
		{
			float Cy = startCy+Dx;
			float Cx = startCx+Dy;

			int Iteration = iterationLoop(Cx, Cy);
			int index = getIndex(startX+1, startY+1, sizeX, sizeY);
			if (index != -1)
			{
				colorFromIteration(&currentColor, Iteration);
				rgbaPixels[index] = currentColor;
			}
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
	else // fill square
	{
		colorFromIteration(&currentColor, prevIteration);

		for (int i = 1; i < SquareSize - 1; i++)
		{
			for (int j = 1; j < SquareSize - 1; j++)
			{

				int index = getIndex(i + startX, j + startY, sizeX, sizeY);
				rgbaPixels[index] = currentColor;
			}
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

	for (int j = 0; j < sizeY-78; j+=78)
	{
		float Cy = (j - sizeY/2) * oneOnZoomCoef + zoomCenterY;

		for (int i = 0; i < sizeX-78; i+=78)
		{
			float Cx = (i - sizeX/2) * oneOnZoomCoef + zoomCenterX;
			drawRecursiveSquare(pixelsdata, sizeX, sizeY, i, j, 78, Cx, Cy, oneOnZoomCoef, oneOnZoomCoef);
		}
	}
}