#include "Equation2.h"

// at^2 + bt + c = 0
// at^2 + bt + c = Y <=> at^2 + bt + c-Y = 0

double Equation2::delta(double Y)
{
	// for a given Y delta = b^2 - 4 * a * (c-Y)
	double result = mB * mB - 4.0 * mA * (mC - Y);
	return result;
}

std::vector<double>	Equation2::Solve(double forY)
{
	// empty result vector
	std::vector<double> result;

	// get delta
	double d = delta(forY);

	if (d < 0.0) // no solution, return empty vector
	{
		return result;
	}
	
	// for precision : 

	double r1, r2;

	if (mB < 0.0)
	{
		r1 = (-mB + sqrt(d)) / (2.0 * mA);		
	}
	else
	{
		r1 = (-mB - sqrt(d)) / (2.0 * mA);
	}

	result.push_back(r1);

	if (d > 0.0)
	{
		r2 = mC / (mA * r1);
		result.push_back(r2);
	}

	return result;
	
	// compute first solution ( -b - sqrt(delta))/2a and push it on result vector
	/*result.push_back((-mB - sqrt(d)) / (2.0 * mA));
	// if d is > 0.0 then compute the other solution ( -b + sqrt(delta))/2a and push it on result vector
	if (d > 0.0)
	{
		result.push_back((-mB + sqrt(d)) / (2.0 * mA));
	}
	// return solutions
	return result;*/
}