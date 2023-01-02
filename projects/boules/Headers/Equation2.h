#pragma once

#include <vector>

namespace Kigs
{

	// manage second degree equation 
	class Equation2
	{
	protected:
		// three coefficients
		double	mA;
		double	mB;
		double	mC;

		// compute delta for given Y
		double delta(double Y);

	public:
		// init equation
		Equation2(double a, double b, double c) : mA(a), mB(b), mC(c)
		{

		}

		// change coeffs
		void Set(double a, double b, double c)
		{
			mA = a;
			mB = b;
			mC = c;
		}

		// solve equation for given Y and return vector of solutions
		std::vector<double>	Solve(double forY = 0.0);
	};

}