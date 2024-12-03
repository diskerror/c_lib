//
// Created by Reid Woodbury on 11/15/24.
//

#ifndef FIR_WINDOWEDSINC_H
#define FIR_WINDOWEDSINC_H

#include <cmath>

class WindowedSinc
{
	long double *H  = nullptr;    //  points to the list of coeficients, sizeof (H) = M
	long double *Hc = nullptr;    //  points to the center of the list of coeficients
	uint32_t    M   = 0;          //  number of coeficients

public:
	WindowedSinc(void) {}
	
	WindowedSinc(double_t Fc, double_t transition)
	{
		SetSinc(Fc, transition);
	}

//	WindowedSinc(WindowedSinc &s); //  copy constructor
	
	~WindowedSinc() { if (H) { delete[] H; }}
	
	//  Cutoff frequency Fc in fraction of sample rate;
	//  Transition width in fraction of sample rate;
	void SetSinc(double_t Fc, double_t transition);
	
	//  returns coeficient at index without range checking
	inline long double GetCoeff(uint32_t i) { return H[i]; }
	
	//	return pointer to array of coefficients
	inline long double *Get_H(void) { return H; }
	
	//	returns size of H in count of coefficients
	inline uint32_t Get_M(void) { return M; }
	
	// Normalize then apply gain.
	void NormalGain(double_t gain = 1.0);
	
	void Gain(float_t a); //  gain only, -1 inverts. "a" for alpha, the scalor
	
	//  Applies window to H.
	void ApplyBlackman();
	
	void ApplyHamming();
	
	void MakeIntoHighPass();
	
	//  overloaded operators
	inline long double operator[](int32_t i) { return *(Hc + i); } // i: -Mo2 <= i <= Mo2
//	inline WindowedSinc &operator+(WindowedSinc &s);
//	inline WindowedSinc &operator-(WindowedSinc &s);//{ return ( *this + (-s) ); }
//	inline WindowedSinc &operator-(void);
//	WindowedSinc &operator*(double_t &a) { Gain(a); }

private:
//  extra house keeping functions
//	void CopyH(WindowedSinc &s);

};


#endif //FIR_WINDOWEDSINC_H
