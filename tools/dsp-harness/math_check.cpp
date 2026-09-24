// harness: standalone
// libm check on the device toolchain (run under qemu-arm): every function
// the audio code uses, against values known to double precision
#include <math.h>
#include <stdio.h>
struct Case { const char *name; double got; double want; };
int main(int argc,char **argv) {
	volatile double a=1.0,h=0.5,t=0.0013,s=7.0/12.0;
	Case c[]={
		{"exp(-1)",exp(-a),0.36787944117144233},
		{"exp(-0.0013)",exp(-t),0.99870084463981140},
		{"expf(-0.0013)",expf((float)-t),0.99870084463981140},
		{"pow(2,7/12)",pow(2.0,s),1.4983070768766815},
		{"powf(2,7/12)",powf(2.0f,(float)s),1.4983070768766815},
		{"pow(10,0.5)",pow(10.0,h),3.1622776601683795},
		{"exp2(7/12)",exp2(s),1.4983070768766815},
		{"log(2)",log(2.0*a),0.69314718055994531},
		{"log10(2)",log10(2.0*a),0.30102999566398120},
		{"sin(1)",sin(a),0.84147098480789651},
		{"cos(1)",cos(a),0.54030230586813977},
		{"tan(0.5)",tan(h),0.54630248984379051},
		{"tanh(0.5)",tanh(h),0.46211715726000974},
		{"sqrt(2)",sqrt(2.0*a),1.4142135623730951},
	};
	int bad=0;
	for (unsigned i=0;i<sizeof(c)/sizeof(c[0]);i++) {
		double err=fabs(c[i].got-c[i].want)/fabs(c[i].want);
		bool ok=err<1e-6;
		if (!ok) bad++;
		printf("%-14s got=%.10f want=%.10f %s\n",c[i].name,c[i].got,c[i].want,ok?"ok":"WRONG");
	}
	printf(bad?"%d wrong\n":"libm ok\n",bad);
	return bad?1:0;
}
