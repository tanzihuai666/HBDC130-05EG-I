#include "gd32f10x.h"

typedef struct {         
	                uint16_t    AD_Count;
									uint16_t    AD_Cur_Count;
									uint16_t		RS422_Count;
									uint16_t		Status_Count;	//状态检测
									uint16_t		RS422_TX_U1;	//串口通信计时
									uint8_t			RS422_Fault_U1;
									uint8_t			RS422_CMDcode_Flag;
									
}T2_DELAY_STRUCT;

extern T2_DELAY_STRUCT		T2_COUNT; 

void DRV_Timer_Init(void);

