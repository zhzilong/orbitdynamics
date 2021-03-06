// LTAttiCal.cpp: 定义控制台应用程序的入口点。
//

#include <iostream>
#include <string>
using namespace std;

#include <orbitdyn.h>
using namespace Constant;

#include "AttDyn.h"

AttDyn attdyn;
CSatellite sat;

struct ACalInit {
	string epoch; //初始参数对应的北京时间（如：2018 - 01 - 01 00:00 : 00)	字符串
	//double phi;   //滚动角,度
	//double theta; //俯仰角,度
	//double psi;   //偏航角,度
	//double dphi;  //滚动角速度,度/秒
	//double dtheta;//俯仰角速度,度/秒
	//double dpsi;  //偏航角速度,度/秒
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

void AttiCal_Init(struct ACalInit ai)
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

struct ACalIn {
	int* WThr; //电推力器开关状态,0为关，1为开
	double* FThr; //推力器推力
};

struct ACalOut {
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
};

void AttiCal_Step(double steptime, struct ACalIn in, struct ACalOut& out)
{
	attdyn.Thrust(in.WThr, in.FThr);
	attdyn.rigidstep(1);
	sat.Propagate(1, 1);
	mat33 coi = GetCoi(sat.GetOrbitElements());
	mat33 cbo = attdyn.q.C()*(coi.t());

	CEuler eu;
	eu.SetValueFromMatrix312(cbo);

	out.phi = eu.phi * DEG;
	out.theta = eu.theta * DEG;
	out.psi = eu.psi * DEG;
	out.dphi = (cos(eu.theta)*attdyn.w(0) + sin(eu.theta)*attdyn.w(2))*DEG;
	out.dtheta = tan(eu.phi)*(sin(eu.theta)*attdyn.w(0) + attdyn.w(1) - cos(eu.theta)*attdyn.w(2))*DEG;
	out.dpsi = (sin(eu.theta)*attdyn.w(0) + cos(eu.theta)*attdyn.w(2)) / cos(eu.phi)*DEG;

	out.qx = attdyn.q.qx;
	out.qy = attdyn.q.qy;
	out.qz = attdyn.q.qz;
	out.qs = attdyn.q.qs;
	out.wx = attdyn.w(0)*DEG;
	out.wy = attdyn.w(1)*DEG;
	out.wz = attdyn.w(2)*DEG;
}

//初始化函数：void AttiCal_Init(struct ACalInit)
//运行一步函数：void AttiCal_Step(double steptime, struct ACalIn, struct ACalOut)

int main()
{
	struct ACalInit ainit;
	struct ACalIn ain;
	struct ACalOut aout;

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
	ainit.w[0] = 0*RAD;
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
	/*	
	ainit.NThrustor = 6; //电推力器个数
	ainit.ThrPos[0] = new double[ainit.NThrustor];   //电推力器安装位置,m
	ainit.ThrPos[1] = new double[ainit.NThrustor];
	ainit.ThrPos[2] = new double[ainit.NThrustor];
	ainit.ThrDir[0] = new double[ainit.NThrustor];   //电推力器推力方向
	ainit.ThrDir[1] = new double[ainit.NThrustor];
	ainit.ThrDir[2] = new double[ainit.NThrustor];
	ain.WThr = new int[ainit.NThrustor];
	ain.FThr = new double[ainit.NThrustor];

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
	*/
///////////////////////////////////////////////////////////////////////
	ainit.NThrustor = 4; //电推力器个数
	ainit.ThrPos[0] = new double[ainit.NThrustor];   //电推力器安装位置,m
	ainit.ThrPos[1] = new double[ainit.NThrustor];
	ainit.ThrPos[2] = new double[ainit.NThrustor];
	ainit.ThrDir[0] = new double[ainit.NThrustor];   //电推力器推力方向
	ainit.ThrDir[1] = new double[ainit.NThrustor];
	ainit.ThrDir[2] = new double[ainit.NThrustor];
	ain.WThr = new int[ainit.NThrustor];
	ain.FThr = new double[ainit.NThrustor];

	double s2 = sqrt(2) / 2;
	// T1:
	ainit.ThrPos[0][0] =  0.0;  ainit.ThrDir[0][0] = 0;
	ainit.ThrPos[1][0] = -0.1;	ainit.ThrDir[1][0] = -s2;
	ainit.ThrPos[2][0] = -0.1;	ainit.ThrDir[2][0] = s2;
	// T2:
	ainit.ThrPos[0][1] =  0.0;  ainit.ThrDir[0][1] = 0;
	ainit.ThrPos[1][1] =  0.1;	ainit.ThrDir[1][1] = s2;
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
	ainit.a     = 7000;
	ainit.e     = 0.001;
	ainit.i     = 98*RAD;
	ainit.Omega = 0;
	ainit.wp    = 0;
	ainit.M     = 0;
	ainit.Mass  = 60;
	ainit.Area  = 0.16;
	// 初始化
	AttiCal_Init(ainit);

	// 外推步长
	double step = 1;
	// 外推总步数
	int len = 600;

	// 输出STK星历文件的头部
	fstream fstk("AttCal.a", ios::out);
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
	fstream fo("AttCal.txt", ios::out);
	fo.precision(12);

	for (int KK = 0;KK < len;KK++)
	{
		// 推力器开关
		for (int n = 0; n < ainit.NThrustor; n++)
		{
			ain.WThr[n] = 0;
			ain.FThr[n] = 0.0003;  // 推力10mN 
		}
		// 六台推力器1~6依次开启，各开启10秒
		//if (KK < 100)
		//	ain.WThr[0] = 1;
		//else if (KK < 200)
		//	ain.WThr[1] = 1;
		//else if (KK < 300)
		//	ain.WThr[2] = 1;
		//else if (KK < 400)
		//	ain.WThr[3] = 1;
		//else if (KK < 500)
		//	ain.WThr[4] = 1;
		//else if (KK < 600)
		//	ain.WThr[5] = 1;

		// 四台推力器依次组合开启
		if (KK < 10)
			ain.WThr[1] = ain.WThr[3] = 1;
		else if (KK > 100 && KK < 110)
			ain.WThr[0] = ain.WThr[2] = 1;
		else if (KK > 200 && KK < 210)
			ain.WThr[0] = ain.WThr[1] = 1;
		else if (KK > 300 && KK < 310)
			ain.WThr[2] = ain.WThr[3] = 1;
		else if (KK > 400 && KK < 410)
			ain.WThr[0] = ain.WThr[3] = 1;
		else if (KK > 500 && KK < 510)
			ain.WThr[1] = ain.WThr[2] = 1;

		// 外推一步
		AttiCal_Step(step, ain, aout);

		// 保存数据文件
		fo << KK * step << "\t" << aout.phi << "\t" << aout.theta << "\t" << aout.psi 
			<< "\t" << aout.dphi
			<< "\t" << aout.dtheta << "\t" << aout.dpsi << "\t"
			<< aout.qx << "\t" << aout.qy << "\t" << aout.qz << "\t"
			<< aout.qs << "\t" << aout.wx << "\t" << aout.wy << "\t"
			<< aout.wz << "\n";

		// 输出STK星历文件
		fstk << KK * step << "\t" << aout.qx << "\t" << aout.qy  << "\t" << aout.qz  << "\t"
			<< aout.qs  << "\t" << aout.wx << "\t" << aout.wy << "\t" << aout.wz << "\n";

		// 控制台提示
		printf("phi = %lf theta = %lf psi = %lf \n", aout.phi, aout.theta, aout.psi);
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
	delete ain.WThr;
	delete ain.FThr;
    return 0;
}

