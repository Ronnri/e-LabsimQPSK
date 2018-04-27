// QPSK.cpp : 定义 DLL 的初始化例程。
//

#include "stdafx.h"
#include "QPSK.h"
#include <math.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

//
//TODO: 如果此 DLL 相对于 MFC DLL 是动态链接的，
//		则从此 DLL 导出的任何调入
//		MFC 的函数必须将 AFX_MANAGE_STATE 宏添加到
//		该函数的最前面。
//
//		例如:
//
//		extern "C" BOOL PASCAL EXPORT ExportedFunction()
//		{
//			AFX_MANAGE_STATE(AfxGetStaticModuleState());
//			// 此处为普通函数体
//		}
//
//		此宏先于任何 MFC 调用
//		出现在每个函数中十分重要。这意味着
//		它必须作为函数中的第一个语句
//		出现，甚至先于所有对象变量声明，
//		这是因为它们的构造函数可能生成 MFC
//		DLL 调用。
//
//		有关其他详细信息，
//		请参阅 MFC 技术说明 33 和 58。
//

// CQPSKApp

BEGIN_MESSAGE_MAP(CQPSKApp, CWinApp)
END_MESSAGE_MAP()


// CQPSKApp 构造

CQPSKApp::CQPSKApp()
{
	// TODO: 在此处添加构造代码，
	// 将所有重要的初始化放置在 InitInstance 中
}


// 唯一的一个 CQPSKApp 对象

CQPSKApp theApp;


// CQPSKApp 初始化

BOOL CQPSKApp::InitInstance()
{
	CWinApp::InitInstance();

	return TRUE;
}

//创建一个算法对象，并返回算法对象指针
void *LtCreateObject()
{
	CAlgorithm *pAlgorithm = new CAlgorithm();
	//UserGui->Create(IDD_DIALOG1,NULL);
	return static_cast<void*>( pAlgorithm );
}

//删除一个算法对象，此函数期待的参数（pObject）正是LtCreateObject()的返回值
void LtDestroyObject( void * pObject )
{
	ASSERT( pObject != NULL );
	ASSERT( !IsBadWritePtr( pObject, sizeof(CAlgorithm) ) );
	CAlgorithm * pAlgorithm = static_cast<CAlgorithm *>( pObject );
	//pAlgorithm->DestroyWindow();
	delete pAlgorithm;	//删除一个算法对象
}

void LtDLLMain(	void * pObject, const bool *pbIsPortUsing, const double *pdInput, double *pdOutput, const int nSimuStep )
{
	ASSERT( pObject != NULL );
	CAlgorithm * pAlgorithm = static_cast<CAlgorithm *>( pObject );
	pAlgorithm->RunAlgorithm( pdInput, pdOutput );
}

void LtResetModule( void *pObject )
{
	ASSERT( pObject != NULL );
	ASSERT( !IsBadWritePtr( pObject, sizeof(CAlgorithm) ) );
	CAlgorithm * pAlgorithm = static_cast<CAlgorithm *>( pObject );
	pAlgorithm->Reset();
}

CAlgorithm::CAlgorithm()
{
	Reset();
}

CAlgorithm::~CAlgorithm()
{
}

void CAlgorithm::Reset()
{
	m_Clk_256k = 0;
	m_Clk_256k_Mid = 0;
	m_Clk_16k = 0;
	m_Clk_16k_mid = 0;
	m_Clk_8k = 0;
	m_Clk_8k_mid = 0;
	m_FIR1_Counter = 0;
	m_FIR1_Result = 0;
	memset( m_FIR1_Buffer, 0, sizeof( m_FIR1_Buffer ) );
	m_FIR1_Num = 0;
	m_FIR2_Counter = 0;
	m_FIR2_Result = 0;
	memset( m_FIR2_Buffer, 0, sizeof( m_FIR2_Buffer ) );
	m_FIR2_Num = 0;

	m_n10MCounter = 0;
	m_10MSin = 0;
	m_10MCos = 0;
	m_MainClk = 0;
	m_nTableIndex = 0;
	m_Cos10M = 0;
	m_Sin10M = 0;

	m_AddResult1 = 0;
	m_AddResult2 = 0;
	m_MultiResult_I_A = 0;
	m_MultiResult_I_B = 0;
	m_MultiResult_Q_A = 0;
	m_MultiResult_Q_B = 0;
	m_MultiResult = 0;
	ResetCostas();
}

void CAlgorithm::ResetCostas()
{
	m_Costas_vco_out = 0;
	m_Costas_sum_ut = 0;
	m_Costas_v8 = 0;
	m_Costas_Counter = 0;
	m_Costas_t = 0;
	m_Costas_vco_out_mid = 0;

	m_LoopFIR_Counter = 0;
	m_LoopFIR_Result = 0;
	memset( m_LoopFIR_Buffer, 0, sizeof( m_LoopFIR_Buffer ) );
	m_LoopFIR_Num = 0;
}

void CAlgorithm::RunAlgorithm(const double *pdInput, double *pdOutput)
{
	MakeModulationCarrier();	//产生调制载波
	//输入时钟二分频,QPSK调制后，数据分为I、Q两路，速率减半
	if( pdInput[IN2] > 1.0)//64K时钟
	{
		if( m_bClkState == LOW )
		{
			m_bClkState = HIGH;
			m_HalfClk++;
			m_HalfClk %= 2;
		}
	}
	else
		m_bClkState = LOW;

	//将输入数据电平调整为二进制数，以便进行差分变换
	if( pdInput[IN1] > 1.0 )
		m_nDataIn = 1;
	else
		m_nDataIn = 0;
	if( pdInput[IN2] < m_LastClkState )
		QPSKmid_1 = m_nDataIn;
	if( pdInput[IN2] > m_LastClkState )
		QPSKmid_2 = QPSKmid_1;            /*移位半个相位*/
	if( pdInput[IN2] < m_LastClkState )
	{
		QPSKmid_I = QPSKmid_2;
		QPSKmid_Q = m_nDataIn;
	}
	if( m_HalfClk > m_LastHalfClkState )//32k时钟
	{
		I_QPSK = QPSKmid_I*(-2)+1;         /*QPSK的I路输出，这里将数字信号0、1变为-1、1，以便接下来的调制过程中与256K载波相乘*/
		Q_QPSK = QPSKmid_Q*(-2)+1;         /*QPSK的Q路输出*/
	}
	if( I_QPSK == 1 )
		NRZ_I_QPSK = 3.3;      
	else
		NRZ_I_QPSK = 0;        
	if( Q_QPSK == 1 )
		NRZ_Q_QPSK = 3.3;
	else
		NRZ_Q_QPSK = 0;        /*QPSK调制的NRZ输出*/
	m_LastClkState = int(pdInput[IN2]);
	m_LastHalfClkState = m_HalfClk;

	
//	pdOutput[OUT1] = NRZ_I_QPSK;	//NRZ-I输出
//	pdOutput[OUT2] = NRZ_Q_QPSK;	//NRZ-Q输出
//	pdOutput[OUT1] = I_QPSK*3.3;	//调制前正负电平的基带信号输出
//	pdOutput[OUT2] = Q_QPSK*3.3;
	//调制输出
	
	pdOutput[OUT5] = I_QPSK*3.3*m_10MCos;	//I路调制输出
	pdOutput[OUT6] = Q_QPSK*3.3*m_10MSin;	//Q路调制输出
	pdOutput[OUT7] = I_QPSK*3.3*m_10MCos + Q_QPSK*3.3*m_10MSin;
	pdOutput[OUT3] = m_10MCos;
	//解调
	//采用松尾环实现QPSK载波同步
	MakeDeModulationCarrier();		//产生载波
	LoopFilter( m_MultiResult );	//环路滤波器
	pdOutput[OUT1] = m_Costas_v8;
	pdOutput[OUT2] = m_MultiResult;
	pdOutput[OUT3] = m_Costas_vco_out;
	VCO(pdInput[W1]);
	//第一步:调制信号与同步载波相乘
	m_DeModulation_I = pdInput[IN3]*m_Cos10M;
	m_DeModulation_Q = pdInput[IN3]*m_Sin10M;
	pdOutput[OUT10] = m_Cos10M;
	//第二步:相乘后信号经过低通滤波去除高频部分
	m_DecisionIn_I = Fir16K_I( m_DeModulation_I );
	m_DecisionIn_Q = Fir16K_Q( m_DeModulation_Q );
        //pdOutput[OUT7]=m_DecisionIn_I;
	//对信号进行门限判决并消除毛刺
	Decision( m_DecisionIn_I, m_DecisionIn_Q );
	//松尾环部分
	m_AddResult1 = m_DecisionIn_I + m_DecisionIn_Q;
	m_AddResult2 = m_DecisionIn_I - m_DecisionIn_Q;
	//将数据转换为正负值，方便接下来的相乘
	if( m_AddResult1 > 0 )
		m_MultiResult_I_A = 2;
	else
		m_MultiResult_I_A = -2;
	if( m_AddResult2 > 0 )
		m_MultiResult_Q_A = 2;
	else
		m_MultiResult_Q_A = -2;
	if( m_Decision_I > 1 )
		m_MultiResult_I_B = 2;
	else
		m_MultiResult_I_B = -2;
	if( m_Decision_Q > 1 )
		m_MultiResult_Q_B = 2;
	else
		m_MultiResult_Q_B = -2;
	m_MultiResult = (m_MultiResult_I_A*m_MultiResult_I_B)*(m_MultiResult_Q_A*m_MultiResult_Q_B);//控制信号
	//m_MultiResult经过二阶环路滤波器得到结果就是压控晶振的控制电压

	ClkGen( m_Decision_I );

	//信号并串变换得到结果
	if( m_Decision_I > 1.0 )              /*I路输入*/
		m_DA_Out2 = 0;
	else
		m_DA_Out2 = 3.3;
	if( m_Decision_Q > 1.0 )               /*Q路输入*/
		m_DA_Out3 = 0;
	else
		m_DA_Out3 = 3.3;
	if( m_Clk_16k > m_Clk_16k_mid )
		m_QPSK_Q_DE = m_DA_Out3;
	if( m_Clk_16k < m_Clk_16k_mid )
		m_QPSK_Q_DE1 = m_QPSK_Q_DE;
	if( m_Clk_16k > m_Clk_16k_mid && m_Clk_8k < 1 )
		m_QPSK_Diffout = m_DA_Out2;
	else if( m_Clk_16k > m_Clk_16k_mid && m_Clk_8k > 1 )
		m_QPSK_Diffout = m_QPSK_Q_DE1;

	m_Clk_256k_Mid = m_Clk_256k;
	m_Clk_16k_mid = m_Clk_16k;
	m_Clk_8k_mid = m_Clk_8k;

	m_Dout = m_QPSK_Diffout;
	m_BS_Out = m_Clk_16k;

	pdOutput[OUT8] = m_Clk_16k;
	pdOutput[OUT9] = m_Dout;
}

void CAlgorithm::MakeModulationCarrier()
{
	//仿真系统我们采用2048K载波来代替10.7M载波
	m_n10MCounter++;
	m_n10MCounter %= 64;
	int nTableIndex = m_n10MCounter/4;
	if( m_n10MCounter %2 == 0 )
	{
		if( m_MainClk < 1.0 )
		{
			m_MainClk = 3.0;
			m_10MSin = SinTable[nTableIndex];
			m_10MCos = CosineTable[nTableIndex];
		}
		else
			m_MainClk = 0;
	}
}

//2M载波
void CAlgorithm::MakeDeModulationCarrier()
{
	if( m_Costas_vco_out >= 0 && m_Costas_vco_out_mid < 0 )//上升沿
	{
		m_nTableIndex++;
		m_nTableIndex %= 16;
	}
	m_Cos10M = CosineTable[m_nTableIndex]*3.3;
	m_Sin10M = SinTable[m_nTableIndex]*3.3;
	m_Costas_vco_out_mid = m_Costas_vco_out;
}

void CAlgorithm::VCO(const double dInverse)
{
	m_Costas_t += Ot;//时间累加
	m_Costas_vco_out = Ac*cos(2*pi*fc*m_Costas_t+2*pi*kc*m_Costas_sum_ut+in_phas);
	m_Costas_sum_ut += Ot*m_Costas_v8*(dInverse-2.5)*50;
}

//环路滤波器,工作频率2048K
void CAlgorithm::LoopFilter(const double DataIn)
{
	m_LoopFIR_Counter++;
	if( m_LoopFIR_Counter > 63 )
	{
		m_LoopFIR_Result = 0.0;
		m_LoopFIR_Counter = 0;
		m_LoopFIR_Buffer[m_LoopFIR_Num] = DataIn;			//将传进来的数放进缓冲区
		m_LoopFIR_Num++;
		m_LoopFIR_Num %= 51;

		for(int i=0;i<51;i++)		//计算fir滤波器的值
		{
			m_LoopFIR_Result = m_LoopFIR_Result + ( m_LoopFIR_Buffer[(m_LoopFIR_Num+i)%51] )*B_LoopFIR[i];
		}
	}
	m_Costas_v8 = m_LoopFIR_Result;                      //返回计算的值
}


double CAlgorithm::Fir16K_I(const double DataIn)
{
	m_FIR1_Counter++;
	if( m_FIR1_Counter > 4 )
	{
		m_FIR1_Result = 0.0;
		m_FIR1_Counter = 0;
		m_FIR1_Buffer[m_FIR1_Num] = DataIn;			//将传进来的数放进缓冲区
		m_FIR1_Num++;
		m_FIR1_Num %= 51;

		for(int i=0;i<51;i++)		//计算fir滤波器的值
		{
			m_FIR1_Result = m_FIR1_Result + ( m_FIR1_Buffer[(m_FIR1_Num+i)%51] )*B_16K[i];
		}
	}
	return m_FIR1_Result;                      //返回计算的值
}

double CAlgorithm::Fir16K_Q(const double DataIn)
{
	m_FIR2_Counter++;
	if( m_FIR2_Counter > 4 )
	{
		m_FIR2_Result = 0.0;
		m_FIR2_Counter = 0;
		m_FIR2_Buffer[m_FIR2_Num] = DataIn;			//将传进来的数放进缓冲区
		m_FIR2_Num++;
		m_FIR2_Num %= 51;

		for(int i=0;i<51;i++)		//计算fir滤波器的值
		{
			m_FIR2_Result = m_FIR2_Result + ( m_FIR2_Buffer[(m_FIR2_Num+i)%51] )*B_16K[i];
		}
	}
	return m_FIR2_Result;                      //返回计算的值
}

//门限判决+消除毛刺
void CAlgorithm::Decision(const double DataIn_I, const double DataIn_Q)
{
	//I路二值电平门限判决
	if( DataIn_I > -0.25 )
	{
		I2 = 3.3;
		I2_Opposition = 0;
	}
	else
	{
		I2 = 0;
		I2_Opposition = 3.3;
	}
	if( DataIn_I > 0.25 )
		I3 = 3.3;
	else
		I3 = 0;
	//I路信号消除毛刺
	if( I2_Opposition < 1.0 && I3 > 1.0 )
	{
		m_Decision_I = 3.3;
	}
	if( I3 < 1.0 && I2_Opposition > 1.0 )
	{
		m_Decision_I = 0;
	}

	//Q路二值电平门限判决
	if( DataIn_Q > -0.25 )
	{
		Q2 = 3.3;
		Q2_Opposition = 0;
	}
	else
	{
		Q2 = 0;
		Q2_Opposition = 3.3;
	}
	if( DataIn_Q > 0.25 )
		Q3 = 3.3;
	else
		Q3 = 0;

	if( Q2_Opposition < 1.0 && Q3 > 1.0 )
	{
		m_Decision_Q = 3.3;
	}
	if( Q3 < 1.0 && Q2_Opposition > 1.0 )
	{
		m_Decision_Q = 0;
	}
}
//位时钟产生和同步
void CAlgorithm::ClkGen(double DataIn)
{
	m_Counter1++;
	if( m_Counter1 > 63 )
	{
		m_Counter1 = 0;
		m_Counter2++;
		m_Counter2 %= 32;
		if( m_Clk_256k == 0 )
			m_Clk_256k = 5;
		else
			m_Clk_256k = 0;
	}
	if( m_Counter2 < 16 )
	{
		m_Clk_16k = 5;
	}
	else
	{
		m_Clk_16k = 0;
	}
	if( m_Clk_16k > m_Clk_16k_mid )
	{
		if( m_Clk_8k == 0 )
			m_Clk_8k = 5;
		else
			m_Clk_8k = 0;
	}

	if( DataIn > 1.0 )
	{
		if( m_DPLL_DataIn_State == false )
		{
			m_DPLL_DataIn_State = true;
			if( m_pulse %16 == 0 )
				m_bIsPhaseLocked = true;
			else
				m_bIsPhaseLocked = false;
			if( m_bIsPhaseLocked == false )
			{
				//这里我们不管是超前还是滞后，均将计数器拉到中间值，这样可以确保每次能够锁定
				if( ( m_pulse%16 > 0 ) && ( m_pulse%16 < 8 ) )	//超前
					m_Counter2 = 16;
				else	//滞后
					m_Counter2 = 16;
			}
			m_pulse = 0;
		}
		//16倍时钟计数，上升沿和下降沿均计数
		if( m_Clk_256k > 1 )
		{
			if( m_Clk_256K_State == false )
			{
				m_Clk_256K_State = true;
				m_pulse++;
			}
		}
		else
		{
			if( m_Clk_256K_State == true )
			{
				m_Clk_256K_State = false;
				m_pulse++;
			}
		}
	}
	else
	{
		if( m_DPLL_DataIn_State == true )
		{
			m_DPLL_DataIn_State = false;
			if( m_pulse == 16 )
				m_bIsPhaseLocked = true;
			else
				m_bIsPhaseLocked = false;
			if( m_bIsPhaseLocked == false )
			{
				if( ( m_pulse%16 > 0 ) && ( m_pulse%16 < 8 ) )	//超前
					m_Counter2 = 16;
				else	//滞后
					m_Counter2 = 16;
			}
			m_pulse = 0;
		}
		//16倍时钟计数，上升沿和下降沿均计数
		if( m_Clk_256k > 1 )
		{
			if( m_Clk_256K_State == false )
			{
				m_Clk_256K_State = true;
				m_pulse++;
			}
		}
		else
		{
			if( m_Clk_256K_State == true )
			{
				m_Clk_256K_State = false;
				m_pulse++;
			}
		}
	}
	/*****************时钟产生***********************/
}
