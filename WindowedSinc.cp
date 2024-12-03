//
// Created by Reid Woodbury on 11/15/24.
//

#include "WindowedSinc.h"
#include <iostream>

using namespace std;

const long double twoPi = 2.0 * M_PI;


/////////////////////////////////////////////////////////////////////////////////////////
//  copy constructor
//WindowedSinc::WindowedSinc(WindowedSinc &s)
//{
//	CopyH(s);
//}


/////////////////////////////////////////////////////////////////////////////////////////
//  initialize sinc function
//	set up coefficients for low pass
//	sinc = sin(2.0 * M_PI * Fc * (i - M / 2)) / (i - M / 2)
//	natFc = 2.0 * M_PI * Fc
//	index i; 0 -> M
//	Mo2 = M / 2
void WindowedSinc::SetSinc(double_t Fc, double_t transition)
{
	long double natFc = twoPi * Fc;
	
	M = (uint32_t) round(4.0 / transition);
	//	If M is not even then add 1
	if (M % 2 != 0) { M++; }
	
	//  if H existed, delete the old
	if (H != nullptr) { delete[] H; }
	H = new long double[M + 1];    //	H has one more slot than M
	if (!H) { throw ("WindowedSinc (new): Error creating H"); }
	
	uint32_t Mo2 = M / 2;
	uint32_t i;
	int32_t  imMo2;
	
	for (i = 0; i < Mo2; i++)
	{
		imMo2 = i - Mo2;
		H[i] = sinl(natFc * imMo2) / imMo2;
	}
	
	//	account for divide by zero
	H[i++] = natFc;
	
	for (; i <= M; i++)
	{
		imMo2 = i - Mo2;
		H[i] = sinl(natFc * imMo2) / imMo2;
	}
	
	Hc = H + Mo2;
	
	//  normalize sum of all points H to 1.0
	NormalGain();
}

/////////////////////////////////////////////////////////////////////////////////////////
//  normalize to 1x the change gain
void WindowedSinc::NormalGain(double_t gain)
{
	double_t sum = 0.0;
	
	for (uint32_t i = 0; i <= M; i++)
	{
		sum += (double_t) H[i];
	}
	
	Gain(gain / sum);
}


/////////////////////////////////////////////////////////////////////////////////////////
void WindowedSinc::Gain(float_t a)
{
	if (a != 1.0)
	{  //  if not unity gain
		long double   al = (long double) a;
		for (uint32_t i  = 0; i <= M; ++i)
		{
			H[i] *= al;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////////
void WindowedSinc::ApplyBlackman()
{
	long double   twoPiIoM;
	for (uint32_t i = 0; i <= M; i++)
	{
		twoPiIoM = twoPi * (i + 1) / (M + 2);
		H[i] *= (0.42 - (0.5 * cosl(twoPiIoM)) + (0.08 * cosl(2.0 * twoPiIoM)));
//		cout << (0.42 - (0.5 * cosl(twoPiIoM)) + (0.08 * cosl(2.0 * twoPiIoM))) << endl;
	}
	
	NormalGain();
}


/////////////////////////////////////////////////////////////////////////////////////////
void WindowedSinc::ApplyHamming()
{
	for (uint32_t i = 0; i <= M; i++)
	{
		H[i] *= (0.54 - (0.46 * cosl(twoPi * i / M)));
//		cout << (0.54 - (0.46 * cosl(twoPi * i / M))) << endl;
	}
	
	NormalGain();
}

/////////////////////////////////////////////////////////////////////////////////////////
void WindowedSinc::MakeIntoHighPass()
{
	Gain(-1.0);
	*Hc += 1.0;
}
