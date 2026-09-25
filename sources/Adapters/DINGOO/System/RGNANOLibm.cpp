// The RG Nano's musl libm gets the exp family wrong: exp(-1) is 0.3672
// instead of 0.3679, pow(2,7/12) is 3 cents sharp, and exp(x) for small x
// returns exactly 1.0. Envelopes use exp(-4.6/samples), so every decay and
// release coefficient became 1.0 and released notes never faded.
// (log, sin, cos, tan and sqrt are correct.)
//
// These definitions replace the library ones for the whole app. They only
// use arithmetic, log() and ldexp(), which are correct on the device.
// Checked under qemu-arm by tools/dsp-harness/math_check.cpp.

#include <math.h>

// floor() is wrong too: for x >= 1 it returns x unchanged (floor(1.5) =
// 1.5), so phase wraps like x-floor(x) collapsed to 0. Whole-number parts
// come from an integer conversion instead.
extern "C" double floor(double x) {
	if (x!=x || x>=4503599627370496.0 || x<=-4503599627370496.0) return x ;
	double whole=(double)(long long)x ;
	return (whole>x)?whole-1.0:whole ;
}

extern "C" double ceil(double x) {
	if (x!=x || x>=4503599627370496.0 || x<=-4503599627370496.0) return x ;
	double whole=(double)(long long)x ;
	return (whole<x)?whole+1.0:whole ;
}

static const double LN2_HI=6.93147180369123816490e-01 ;
static const double LN2_LO=1.90821492927058770002e-10 ;
static const double INV_LN2=1.44269504088896338700e+00 ;

extern "C" double exp(double x) {
	if (x!=x) return x ;
	if (x>709.782712893384) return HUGE_VAL ;
	if (x<-745.1332191019412) return 0.0 ;

	// x = k*ln2 + r, |r| <= ln2/2
	double kd=x*INV_LN2 ;
	int k=(int)(kd<0.0?kd-0.5:kd+0.5) ;
	double r=(x-k*LN2_HI)-k*LN2_LO ;

	// Taylor series to r^13: error below 1e-17 for |r| <= 0.347
	double term=1.0 ;
	double sum=1.0 ;
	for (int n=1;n<=13;n++) {
		term*=r/n ;
		sum+=term ;
	}
	return ldexp(sum,k) ;
}

extern "C" double exp2(double x) {
	if (x!=x) return x ;
	// Whole powers of two stay exact
	double whole=floor(x) ;
	return ldexp(exp((x-whole)*6.93147180559945309417e-01),(int)whole) ;
}

extern "C" double pow(double x,double y) {
	if (y==0.0 || x==1.0) return 1.0 ;
	if (x!=x || y!=y) return x+y ;
	if (x==0.0) return (y>0.0)?0.0:HUGE_VAL ;
	if (x<0.0) {
		// Only whole exponents are defined for negative bases
		if (floor(y)!=y) return (x-x)/(x-x) ;
		double mag=exp(y*log(-x)) ;
		return (fmod(y,2.0)!=0.0)?-mag:mag ;
	}
	return exp(y*log(x)) ;
}

extern "C" float expf(float x) { return (float)exp((double)x) ; }
extern "C" float exp2f(float x) { return (float)exp2((double)x) ; }
extern "C" float powf(float x,float y) { return (float)pow((double)x,(double)y) ; }
