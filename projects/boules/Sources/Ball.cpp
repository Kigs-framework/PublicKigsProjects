#include "Ball.h"
#include "Equation2.h"
#include "CoreModifiableAttribute.h"

using namespace Kigs;

// update graphics
void	Ball::Update(double time)
{
	// get current pos according to time
	v2f currentPos(GetPos(time));

	currentPos.x /= 1280.0f;
	currentPos.y /= 800.0f;

	mGraphicBall("Dock") = currentPos;
}

// return time to the next collision with the other ball or -1 if no collision found in the future
std::pair<double,double>	Ball::getCollisionTimeWithOther(const Ball& other)
{
	// compute coefs
	v2f DS(mSpeed - other.mSpeed);
	v2f DP(GetPos(0.0) - other.GetPos(0.0));

	double CoefA = DS.x * DS.x + DS.y * DS.y;
	double CoefB = 2.0 * (DS.x * DP.x + DS.y * DP.y);
	double CoefC = DP.x * DP.x + DP.y * DP.y;

	Equation2	toSolve(CoefA, CoefB, CoefC);

	// get equation results for d^2 = 4.0f * mR * other.mR
	std::vector<double>	results = toSolve.Solve(4.0 * mR * other.mR);

	// we need two results for a real intersection
	if (results.size() == 2)
	{
		double t1 = results[0];
		double t2 = results[1];
		// compute better approximation
		double midtime = (t1 + t2) * 0.5;
		if (midtime > 0.0)
		{
			DP.Set(GetPos(midtime) - other.GetPos(midtime));
			CoefB = 2.0 * (DS.x * DP.x + DS.y * DP.y);
			CoefC = DP.x * DP.x + DP.y * DP.y;

			Equation2	toSolveBetter(CoefA, CoefB, CoefC);

			// get equation results for d^2 = 4.0f * mR * other.mR
			std::vector<double>	resultsBetter = toSolveBetter.Solve(4.0 * mR * other.mR);
			if (resultsBetter.size() != 2)
			{
				return { -1.0,-1.0 };
			}
			t1 = resultsBetter[0] + midtime;
			t2 = resultsBetter[1] = midtime;
		}

		// sort solutions so that t1<t2
		if (t1 > t2)
		{
			double swp = t2;
			t2 = t1;
			t1 = swp;
		}

		// return the good solution
		return { t1,t2 };
	}
	// no intersection
	return { -1.0,-1.0 };
}

// return time to the next collision with the given wall
double	Ball::getCollisionTimeWithWall(const Wall& other)
{
	v2f	DP(GetPos(0.0) - other.GetPos());
	// compute projected distance on wall normal, from the ball to the wall
	double projectDist = Dot(DP, other.GetNormal());
	// compute projected speed on wall normal 
	double projectSpeed = Dot(mSpeed, other.GetNormal());

	// if projectSpeed >= 0 then the wall is "behind" de ball direction
	if (projectSpeed < 0.0)
	{
		// once projected on wall normal, we have d = projectDist + t projectSpeed
		// we want to solve R = projectDist + t projectSpeed <=> t = (R - projectDist)/projectSpeed
		double t = (mR - projectDist) / projectSpeed;
		if (t > 0.0) // if time is in the future, return it
		{
			return t;
		}
	}
	// no collision found
	return -1.0;
}