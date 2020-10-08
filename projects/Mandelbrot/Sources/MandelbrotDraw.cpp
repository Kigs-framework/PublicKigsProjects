

struct RGBA
{
	unsigned char R;
	unsigned char G;
	unsigned char B;
	unsigned char A;
};

int getIndex(int posX, int posY, int sizeX, int sizeY)
{
	if ((posX >= 0) && (posX < sizeX) && (posY >= 0) && (posY < sizeY))
	{
		return posX + posY * sizeX;
	}
	return -1;
}

void	DrawMandelbrot(unsigned char* pixelsdata, int sizeX, int sizeY, float zoomCenterX, float zoomCenterY, float zoomCoef)
{
	RGBA* rgbaPixels = (RGBA*)pixelsdata;

	for (int j = 0; j < sizeY; j++)
	{
		float Cy = (j - sizeY/2) * (1.0f / zoomCoef) + zoomCenterY;

		for (int i = 0; i < sizeX; i++)
		{
			float Cx = (i - sizeX/2) * (1.0f / zoomCoef) + zoomCenterX;
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

			int index = getIndex(i, j, sizeX, sizeY);
			if (index != -1)
			{
				RGBA currentColor;
				currentColor.R = (unsigned char)((Iteration << 4) & 255);
				currentColor.G = 0;
				currentColor.B = (unsigned char)(((Iteration >> 4) & 255) << 4);
				currentColor.A = 255;

				rgbaPixels[index] = currentColor;
			}
		}
	}
}