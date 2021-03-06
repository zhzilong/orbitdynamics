// LTAttiControl.cpp: 定义控制台应用程序的入口点。
//

#include <iostream>
#include <string>
using namespace std;

#include <orbitdyn.h>
using namespace Constant;

#include "AttDyn.h"
AttDyn attdyn;
CSatellite sat;
double Tx, Ty, Tz;

#define THRUSTNUM  6
#define PWM 0

struct AConInit {
	string epoch; //初始轨道参数对应的北京时间（如："2018-01-01 00:00:00")

	double phi;   //滚动角,度
	double theta; //俯仰角,度
	double psi;   //偏航角,度
	double dphi;  //滚动角速度,度/秒
	double dtheta;//俯仰角速度,度/秒
	double dpsi;  //偏航角速度,度/秒

	double q[4];    //姿态四元数
	double w[3];    //姿态角速度，本体系相对惯性系的角速度在本体系的三轴分量
	double Is[3][3];  //卫星转动惯量矩阵,kgm^2
	double bc[3];	  //卫星质心位置,m
	int NThrustor;	  //电推力器个数
	double* ThrPos[3]; //电推力器安装位置,m
	double* ThrDir[3]; //电推力器推力方向

	double a;	//半长轴
	double e;	//偏心率
	double i;	//倾角
	double Omega;	//升交点赤径
	double wp;	//近地点幅角
	double M;	//平近点角
	double Mass;	//卫星初始质量
	double Area;  // 大气阻力迎风面积
};

void AttiControl_Init(struct AConInit ai)
{
	attdyn.Is(0, 0) = ai.Is[0][0];
	attdyn.Is(0, 1) = ai.Is[0][1];
	attdyn.Is(0, 2) = ai.Is[0][2];
	attdyn.Is(1, 0) = ai.Is[1][0];
	attdyn.Is(1, 1) = ai.Is[1][1];
	attdyn.Is(1, 2) = ai.Is[1][2];
	attdyn.Is(2, 0) = ai.Is[2][0];
	attdyn.Is(2, 1) = ai.Is[2][1];
	attdyn.Is(2, 2) = ai.Is[2][2];

	attdyn.q.qx = ai.q[0];
	attdyn.q.qy = ai.q[1];
	attdyn.q.qz = ai.q[2];
	attdyn.q.qs = ai.q[3];
	attdyn.w[0] = ai.w[0];
	attdyn.w[1] = ai.w[1];
	attdyn.w[2] = ai.w[2];

	attdyn.bc(0) = ai.bc[0];
	attdyn.bc(1) = ai.bc[1];
	attdyn.bc(2) = ai.bc[2];

	attdyn.SetThrustNum(ai.NThrustor);
	for (int i = 0; i < ai.NThrustor; i++)
	{
		attdyn.ThrDir[i](0) = ai.ThrDir[0][i];
		attdyn.ThrDir[i](1) = ai.ThrDir[1][i];
		attdyn.ThrDir[i](2) = ai.ThrDir[2][i];
		attdyn.ThrPos[i](0) = ai.ThrPos[0][i];
		attdyn.ThrPos[i](1) = ai.ThrPos[1][i];
		attdyn.ThrPos[i](2) = ai.ThrPos[2][i];
	}

	//char c[100] = "2019-01-01 00:00:00";
	int y, m, d, h, min;
	double sec;
	sscanf_s(ai.epoch.c_str(), "%d-%d-%d %d:%d:%lf", &y, &m, &d, &h, &min, &sec);
	//CDateTime ep = StrToDateTime(c);
	//CDateTime ep = StrToDateTime(ocal.epoch.c_str());
	CDateTime ep(y, m, d, h, min, sec);
	Kepler kp;
	kp.a = ai.a;
	kp.e = ai.e;
	kp.i = ai.i*RAD;
	kp.o = ai.Omega*RAD;
	kp.w = ai.wp*RAD;
	kp.M = ai.M*RAD;
	sat.Initialize(ep, kp);
	sat.Mass0 = ai.Mass;
	sat.AirDragArea = ai.Area;
}

struct AConIn {
	//轨道坐标系下姿态目标角，312转序
	double phiT;	//滚动目标角,度
	double thetaT;	//俯仰目标角,度
	double psiT;	//偏航目标角,度
};

struct AConOut {
	string t;		//姿态参数对应的北京时间
	//轨道坐标系下姿态角，312转序
	double phi;		//滚动角,度
	double theta;	//俯仰角,度
	double psi;		//偏航角,度
	//轨道坐标系下姿态角速度，312转序
	double dphi;	//滚动角速度,度/秒
	double dtheta;	//俯仰角速度,度/秒
	double dpsi;	//偏航角速度,度/秒

	double qx;	//相对惯性系姿态四元数X分量
	double qy;	//相对惯性系姿态四元数Y分量
	double qz;	//相对惯性系姿态四元数Z分量
	double qs;	//相对惯性系姿态四元数S分量

	double wx;	//相对惯性系的角速度在卫星本体系X轴的分量,度/秒
	double wy;	//相对惯性系的角速度在卫星本体系Y轴的分量,度/秒
	double wz;	//相对惯性系的角速度在卫星本体系Z轴的分量,度/秒

	int * WThr; //电推力器开关状态,整数1*NThrustor数组,0为关，1为开
};

void AttiControl_Step(double steptime, struct AConIn in, struct AConOut& out)
{
	// controller
	double wn = 0.007;
	double ksi = 0.707;
	double Kpx = attdyn.Is(0, 0)*(wn * PI2)*(wn * PI2);
	double Kpy = attdyn.Is(1, 1)*(wn * PI2)*(wn * PI2);
	double Kpz = attdyn.Is(2, 2)*(wn * PI2)*(wn * PI2);
	double Kdx = 2 * ksi * (wn*PI2) * attdyn.Is(0, 0);
	double Kdy = 2 * ksi * (wn*PI2) * attdyn.Is(1, 1);
	double Kdz = 2 * ksi * (wn*PI2) * attdyn.Is(2, 2);

	mat33 coi = GetCoi(sat.GetOrbitElements());
	mat33 cto = RotationY(in.thetaT*RAD)*RotationX(in.phiT*RAD)*RotationZ(in.psiT*RAD);
	CQuaternion qt = mat33( cto * coi );
	CQuaternion qbt = Qim(attdyn.q, qt);
	vec3 wo;
	wo(0) = 0; wo(1) = -sat.w0; wo(2) = 0;
	vec3 wbt =  cto*wo - attdyn.w;
	double dx = qbt.qx * 2;
	double anglim = 15 * RAD;
	if (fabs(dx) > anglim)
		dx = sign(dx) * anglim;
	double dy = qbt.qy * 2;
	if (fabs(dy) > anglim)
		dy = sign(dy) * anglim;
	double dz = qbt.qz * 2;
	if (fabs(dz) > anglim)
		dz = sign(dz) * anglim;
	Tx = Kpx * dx + Kdx * wbt(0);
	Ty = Kpy * dy + Kdy * wbt(1);
	Tz = Kpz * dz + Kdz * wbt(2);

	double FThr[6];
	for (int i = 0; i < attdyn.NThrustor; i++) {
		FThr[i] = 3e-4;
		out.WThr[i] = 0;
	}
#if PWM
	// pulse width moduler
#if THRUSTNUM == 6
	double fon = 2e-6;
	double foff = 1e-6;
#elif THRUSTNUM == 4
	double fon = 4e-6;
	double foff = 2e-6;
#endif

	double Ht = fon - foff;
	if (Tx + attdyn.WThr[0] * Ht > fon)
		attdyn.WThr[0] = 1;
	else if (Tx + attdyn.WThr[0] * Ht < -fon)
		attdyn.WThr[0] = -1;
	else
		attdyn.WThr[0] = 0;

	if (Ty + attdyn.WThr[1] * Ht > fon)
		attdyn.WThr[1] = 1;
	else if (Ty + attdyn.WThr[1] * Ht < -fon)
		attdyn.WThr[1] = -1;
	else
		attdyn.WThr[1] = 0;

	if (Tz + attdyn.WThr[2] * Ht > fon)
		attdyn.WThr[2] = 1;
	else if (Tz + attdyn.WThr[2] * Ht < -fon)
		attdyn.WThr[2] = -1;
	else
		attdyn.WThr[2] = 0;

#if THRUSTNUM == 6
	if (attdyn.WThr[0] == 1)
		out.WThr[0] = 1;
	else if (attdyn.WThr[0] == -1)
		out.WThr[1] = 1;

	if (attdyn.WThr[1] == 1)
		out.WThr[2] = 1;
	else if (attdyn.WThr[1] == -1)
		out.WThr[3] = 1;

	if (attdyn.WThr[2] == 1)
		out.WThr[4] = 1;
	else if (attdyn.WThr[2] == -1)
		out.WThr[5] = 1;
#elif THRUSTNUM == 4
	//2+4	+ 0	0
	//1+3	- 0	0
	//1+2	0 + 0
	//3+4	0 - 0
	//1+4	0 0 +
	//2+3	0 0 -
	static int cnt = 0;
	cnt = cnt + 1;
	if (cnt == 4) cnt = 1;
	if (cnt==1)
	{
		if (attdyn.WThr[0] == 1)
		{
			out.WThr[1] = out.WThr[3] = 1;
		}
		else if (attdyn.WThr[0] == -1)
		{
			out.WThr[0] = out.WThr[2] = 1;
		}
	}
	else if (cnt == 2)
	{
		if (attdyn.WThr[1] == 1)
		{
			out.WThr[0] = out.WThr[1] = 1;
		}
		else if (attdyn.WThr[1] == -1)
		{
			out.WThr[2] = out.WThr[3] = 1;
		}
	}
	else if (cnt == 3)
	{

		if (attdyn.WThr[2] == 1)
		{
			out.WThr[0] = out.WThr[3] = 1;
		}
		else if (attdyn.WThr[2] == -1)
		{
			out.WThr[1] = out.WThr[2] = 1;
		}
	}

#endif // THRUSTNUM

#else // PWM
	// continue force
	for (int i = 0; i < attdyn.NThrustor; i++) {
		FThr[i] = 0;
	}
	// torque limit
	double torquemax = 0.0003;
	double torquemin = 0.00001;
	double L = 0.1; // arm of force
	if (Tx > torquemax)
		Tx = torquemax;
	else if (Tx < -torquemax)
		Tx = -torquemax;

	if (Tx > torquemin)
	{
		out.WThr[0] = 1;
		FThr[0] = Tx / L;
	}
	else if(Tx < -torquemin){
		out.WThr[1] = 1;
		FThr[1] = - Tx / L;
	}

	if (Ty > torquemax)
		Ty = torquemax;
	else if (Ty < -torquemax)
		Ty = -torquemax;
	if (Ty > torquemin)
	{
		out.WThr[2] = 1;
		FThr[2] = Ty / L;
	}
	else if(Ty < -torquemin){
		out.WThr[3] = 1;
		FThr[3] = - Ty / L;
	}

	if (Tz > torquemax)
		Tz = torquemax;
	else if (Tz < -torquemax)
		Tz = -torquemax;
	if (Tz > torquemin)
	{
		out.WThr[4] = 1;
		FThr[4] = Tz / L;
	}
	else if(Tz < -torquemin){
		out.WThr[5] = 1;
		FThr[5] = - Tz / L;
	}
#endif //PWM

	attdyn.Thrust(out.WThr, FThr);
	attdyn.rigidstep(1);
	sat.Propagate(1, 1);
	coi = GetCoi(sat.GetOrbitElements());
	mat33 cbo = attdyn.q.C()*(coi.t());
	CEuler eu;
	eu.SetValueFromMatrix312(cbo);

	out.phi = eu.phi * DEG;
	out.theta = eu.theta * DEG;
	out.psi = eu.psi * DEG;
	//euler angle rate
	out.dphi = (cos(out.theta*RAD)*attdyn.w(0) + sin(out.theta*RAD)*attdyn.w(2))*DEG;
	out.dtheta = tan(out.phi*RAD)*(sin(out.theta*RAD)*attdyn.w(0) + attdyn.w(1) - cos(out.theta*RAD)*attdyn.w(2))*DEG;
	out.dpsi = (sin(out.theta*RAD)*attdyn.w(0) + cos(out.theta*RAD)*attdyn.w(2)) / cos(out.phi*RAD)*DEG;

	out.qx = attdyn.q.qx;
	out.qy = attdyn.q.qy;
	out.qz = attdyn.q.qz;
	out.qs = attdyn.q.qs;
	out.wx = attdyn.w(0);
	out.wy = attdyn.w(1);
	out.wz = attdyn.w(2);
}


//初始化函数：void AttiControl_Init(struct AConInit)
//运行一步函数：void AttiControl_Step(double steptime, struct AConIn, struct AConOut)
int main()
{
	struct AConInit ainit;
	struct AConIn ain;
	struct AConOut aout;

	// 轨道初值
	ainit.epoch = "2019-01-01 00:00:00";
	//ainit.phi = 0;       //滚动角,度
	//ainit.theta = 0;     //俯仰角,度
	//ainit.psi = 0;       //偏航角,度
	//ainit.dphi = 0;      //滚动角速度,度/秒
	//ainit.dtheta = 0;    //俯仰角速度,度/秒
	//ainit.dpsi = 0;      //偏航角速度,度/秒
	ainit.q[0] = 0;
	ainit.q[1] = 0;
	ainit.q[2] = 1;
	ainit.q[3] = 0;
	ainit.w[0] = 0 * RAD;
	ainit.w[1] = 0;
	ainit.w[2] = 0;
	ainit.Is[0][0] = 0.07;    //卫星转动惯量矩阵,kgm^2
	ainit.Is[0][1] = 0;
	ainit.Is[0][2] = 0;
	ainit.Is[1][0] = 0;
	ainit.Is[1][1] = 0.07;
	ainit.Is[1][2] = 0;
	ainit.Is[2][0] = 0;
	ainit.Is[2][1] = 0;
	ainit.Is[2][2] = 0.07;
	ainit.bc[0] = 0.1;	     //卫星质心位置,m
	ainit.bc[1] = 0.0;
	ainit.bc[2] = 0.0;

	////////////////////////////////////////////////////////////////////////
#if THRUSTNUM == 6	
	ainit.NThrustor = 6; //电推力器个数
	ainit.ThrPos[0] = new double[ainit.NThrustor];   //电推力器安装位置,m
	ainit.ThrPos[1] = new double[ainit.NThrustor];
	ainit.ThrPos[2] = new double[ainit.NThrustor];
	ainit.ThrDir[0] = new double[ainit.NThrustor];   //电推力器推力方向
	ainit.ThrDir[1] = new double[ainit.NThrustor];
	ainit.ThrDir[2] = new double[ainit.NThrustor];

	// T:+X
	ainit.ThrPos[0][0] =  0.1;  ainit.ThrDir[0][0] = 0;
	ainit.ThrPos[1][0] = -0.1;	ainit.ThrDir[1][0] = 1;
	ainit.ThrPos[2][0] = -0.1;	ainit.ThrDir[2][0] = 0;
	// T:-X
	ainit.ThrPos[0][1] =  0.1;  ainit.ThrDir[0][1] = 0;
	ainit.ThrPos[1][1] = -0.1;	ainit.ThrDir[1][1] = 1;
	ainit.ThrPos[2][1] =  0.1;	ainit.ThrDir[2][1] = 0;
	// T:+Y
	ainit.ThrPos[0][2] = 0.0;   ainit.ThrDir[0][2] = 1;
	ainit.ThrPos[1][2] = 0.0;	ainit.ThrDir[1][2] = 0;
	ainit.ThrPos[2][2] = 0.1;	ainit.ThrDir[2][2] = 0;
	// T:-Y
	ainit.ThrPos[0][3] = 0.0;   ainit.ThrDir[0][3] = 1;
	ainit.ThrPos[1][3] = 0.0;	ainit.ThrDir[1][3] = 0;
	ainit.ThrPos[2][3] = -0.1;	ainit.ThrDir[2][3] = 0;
	// T:+Z
	ainit.ThrPos[0][4] = 0.0;   ainit.ThrDir[0][4] = 1;
	ainit.ThrPos[1][4] = -0.1;	ainit.ThrDir[1][4] = 0;
	ainit.ThrPos[2][4] = 0.0;	ainit.ThrDir[2][4] = 0;
	// T:-Z
	ainit.ThrPos[0][5] = 0.0;   ainit.ThrDir[0][5] = 1;
	ainit.ThrPos[1][5] = 0.1;	ainit.ThrDir[1][5] = 0;
	ainit.ThrPos[2][5] = 0.0;	ainit.ThrDir[2][5] = 0;
	
	///////////////////////////////////////////////////////////////////////
#elif THRUSTNUM == 4	
	ainit.NThrustor = 4; //电推力器个数
	ainit.ThrPos[0] = new double[ainit.NThrustor];   //电推力器安装位置,m
	ainit.ThrPos[1] = new double[ainit.NThrustor];
	ainit.ThrPos[2] = new double[ainit.NThrustor];
	ainit.ThrDir[0] = new double[ainit.NThrustor];   //电推力器推力方向
	ainit.ThrDir[1] = new double[ainit.NThrustor];
	ainit.ThrDir[2] = new double[ainit.NThrustor];

	double s2 = sqrt(2) / 2;
	// T1:
	ainit.ThrPos[0][0] = 0.0;  ainit.ThrDir[0][0] = 0;
	ainit.ThrPos[1][0] = -0.1;	ainit.ThrDir[1][0] = -s2;
	ainit.ThrPos[2][0] = -0.1;	ainit.ThrDir[2][0] = s2;
	// T2:
	ainit.ThrPos[0][1] = 0.0;  ainit.ThrDir[0][1] = 0;
	ainit.ThrPos[1][1] = 0.1;	ainit.ThrDir[1][1] = s2;
	ainit.ThrPos[2][1] = -0.1;	ainit.ThrDir[2][1] = s2;
	// T3:
	ainit.ThrPos[0][2] = 0.0;   ainit.ThrDir[0][2] = 0;
	ainit.ThrPos[1][2] = 0.1;	ainit.ThrDir[1][2] = s2;
	ainit.ThrPos[2][2] = 0.1;	ainit.ThrDir[2][2] = -s2;
	// T4:
	ainit.ThrPos[0][3] = 0.0;   ainit.ThrDir[0][3] = 0;
	ainit.ThrPos[1][3] = -0.1;	ainit.ThrDir[1][3] = -s2;
	ainit.ThrPos[2][3] = 0.1;	ainit.ThrDir[2][3] = -s2;
	////////////////////////////////////////////////////////////////////////
#endif

	ainit.a = 7000;
	ainit.e = 0.001;
	ainit.i = 98 * RAD;
	ainit.Omega = 0;
	ainit.wp = 0;
	ainit.M = 0;
	ainit.Mass = 60;
	ainit.Area = 0.16;
	
	// 初始化
	AttiControl_Init(ainit);

	// 外推步长
	double step = 0.1;
	// 外推总步数
	int len = 5000;

	// 输出STK星历文件的头部
	fstream fstk("AttCon.a", ios::out);
	fstk << "stk.v.4.3" << endl;
	fstk << "BEGIN Ephemeris" << endl;
	fstk << "NumberOfEphemerisPoints " << len << endl;
	fstk << "ScenarioEpoch           " << ainit.epoch << endl; //1 Jun 2002 12:00 : 00.000000000
	fstk << "InterpolationMethod     Lagrange" << endl;
	fstk << "InterpolationOrder      5" << endl;
	fstk << "CentralBody             Earth" << endl;
	fstk << "CoordinateSystem        J2000" << endl << endl;
	fstk << "EphemerisTimePosVel" << endl;
	fstk.precision(12);


	// 保存数据文件
	fstream fo("AttCon.txt", ios::out);
	fo.precision(12);

	ain.phiT = 40;
	ain.thetaT = 20;
	ain.psiT = -30;

	aout.WThr = new int[6];
	for (int i = 0; i < 6; i++) {
		aout.WThr[i] = 0;
	}

	for (int KK = 0;KK < len;KK++)
	{
		// 外推一步
		AttiControl_Step(step, ain, aout);

		// 保存数据文件
		fo << KK * step << "\t" << aout.phi << "\t" << aout.theta << "\t" << aout.psi
			<< "\t" << aout.dphi << "\t" << aout.dtheta << "\t" << aout.dpsi << "\t"
			<< aout.qx << "\t" << aout.qy << "\t" << aout.qz << "\t"<< aout.qs << "\t" 
			<< aout.wx << "\t" << aout.wy << "\t" << aout.wz << "\t" 
			<< aout.WThr[0] << "\t" << aout.WThr[1] << "\t"
			<< aout.WThr[2] << "\t" << aout.WThr[3] << "\t"
			<< aout.WThr[4] << "\t" << aout.WThr[5] << "\t"
			<< Tx << "\t" << Ty << "\t" << Tz << "\n";

		// 输出STK星历文件
		fstk << KK * step << "\t" << aout.qx << "\t" << aout.qy << "\t" << aout.qz << "\t"
			<< aout.qs << "\t" << aout.wx << "\t" << aout.wy << "\t" << aout.wz << "\n";

		// 控制台提示
		printf("phi = %lf theta = %lf psi = %lf wx = %lf wy = %lf wz = %lf\n",
			aout.phi, aout.theta, aout.psi,aout.wx,aout.wy,aout.wz);
	}

	fo.close();

	fstk << "END Ephemeris" << endl;
	fstk.close();

	delete ainit.ThrPos[0];
	delete ainit.ThrPos[1];
	delete ainit.ThrPos[2];
	delete ainit.ThrDir[0];
	delete ainit.ThrDir[1];
	delete ainit.ThrDir[2];

	delete aout.WThr;
	
    return 0;
}

