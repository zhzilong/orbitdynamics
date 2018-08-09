// semi-major axis maneuver
#include <OrbitDyn.h>
#include <PerfTimer.h>

using namespace Constant;

//! LEO轨道倾角捕获
// 在升交点改变倾角，近圆轨道，小修正量
// a 轨道半长轴，km
// i0 初始轨道倾角，度
// it 目标轨道倾角，度
// ascordes = 1 在升交点
// ascordes = 0 在降交点
// 返回：轨道Y方向的速度增量m/s
double leo_Linc(double a,double i0,double it,int ascordes)
{	
	double v = sqrt(GE /a);
	if(ascordes == 1) // 升交点
		return v*(i0-it)*RAD*1000;
	else
		return v*(it-i0)*RAD*1000;
}

//! LEO轨道倾角捕获
// 在升降交点改变倾角，近圆轨道，大倾角改变
// a 轨道半长轴，km
// i0 初始轨道倾角，度
// it 目标轨道倾角，度
// ascordes = 1 在升交点
// ascordes = 0 在降交点
// 返回：轨道系的速度增量m/s
vec3 leo_Binc(double a,double i0,double it,int ascordes)
{
	double v = sqrt(GE /a);
	double dv; 
	vec3 vo;
	if(ascordes == 1) // 升交点
	{
		dv = v*sin((i0-it)*RAD/2)*2*1000;
		vo(0) = dv*sin((i0-it)/2);
		vo(1) = dv*cos((i0-it)/2);
	}
	else
	{
		dv = v*sin((i0-it)*RAD/2)*2*1000;
		vo(0) = dv*sin((i0-it)/2);
		vo(1) = dv*cos((i0-it)/2);
	}	
	
	
}


int main(int argc, char* argv[])
{
	double dv;

	dv = leo_inclination(7000, 0.1);

	dv = sma_ellipse(7000, 0.2, 0, 10);

	return 0;
}