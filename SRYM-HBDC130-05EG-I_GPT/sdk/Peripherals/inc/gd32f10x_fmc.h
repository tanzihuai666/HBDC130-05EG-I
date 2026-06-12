/**
  ******************************************************************************
  * @file    gd32f10x_fmc.h
  * @author  MCU SD
  * @version V1.0.1
  * @date    26-Dec-2014
  * @brief   FMC functions of the firmware library.
  ******************************************************************************
  */

#ifndef __GD32F10X_FMC_H
#define __GD32F10X_FMC_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "gd32f10x.h"

#define FMC_KEY1                         ((uint32_t)0x45670123)
#define FMC_KEY2                         ((uint32_t)0xCDEF89AB)
#define FMC_TIMEOUT_COUNT                ((uint32_t)0x000B0000)

#define FMC_BANK1_SIZE                   ((uint32_t)0x00080000)
#define FMC_B1_END_ADDRESS               ((uint32_t)0x0807FFFF)

#ifndef FMC_SIZE
#define FMC_SIZE                         ((uint32_t)0x00040000)
#endif

#define RDP_LEVEL_0                      ((uint16_t)0x00A5)
#define RDP_LEVEL_1                      ((uint16_t)0x0000)

#define OB_IWDG_SW                       ((uint8_t)0x01)
#define OB_IWDG_HW                       ((uint8_t)0x00)
#define OB_DEEPSLEEP_NORST               ((uint8_t)0x02)
#define OB_DEEPSLEEP_RST                 ((uint8_t)0x00)
#define OB_STDBY_NORST                   ((uint8_t)0x04)
#define OB_STDBY_RST                     ((uint8_t)0x00)
#define OB_BOOT_B1                       ((uint8_t)0x00)
#define OB_BOOT_B2                       ((uint8_t)0x10)

#ifndef OB_USER_BFB2
#define OB_USER_BFB2                     ((uint16_t)0x0010)
#endif

#define OB_WRP0_WRP0                     ((uint32_t)0x000000FF)
#define OB_WRP0_nWRP0                    ((uint32_t)0x0000FF00)
#define OB_WRP1_WRP1                     ((uint32_t)0x00FF0000)
#define OB_WRP1_nWRP1                    ((uint32_t)0xFF000000)

#define WRP_SECTOR0                      ((uint32_t)0x00000001)
#define WRP_SECTOR1                      ((uint32_t)0x00000002)
#define WRP_SECTOR2                      ((uint32_t)0x00000004)
#define WRP_SECTOR3                      ((uint32_t)0x00000008)
#define WRP_SECTOR4                      ((uint32_t)0x00000010)
#define WRP_SECTOR5                      ((uint32_t)0x00000020)
#define WRP_SECTOR6                      ((uint32_t)0x00000040)
#define WRP_SECTOR7                      ((uint32_t)0x00000080)
#define WRP_SECTOR8                      ((uint32_t)0x00000100)
#define WRP_SECTOR9                      ((uint32_t)0x00000200)
#define WRP_SECTOR10                     ((uint32_t)0x00000400)
#define WRP_SECTOR11                     ((uint32_t)0x00000800)
#define WRP_SECTOR12                     ((uint32_t)0x00001000)
#define WRP_SECTOR13                     ((uint32_t)0x00002000)
#define WRP_SECTOR14                     ((uint32_t)0x00004000)
#define WRP_SECTOR15                     ((uint32_t)0x00008000)
#define WRP_SECTOR16                     ((uint32_t)0x00010000)
#define WRP_SECTOR17                     ((uint32_t)0x00020000)
#define WRP_SECTOR18                     ((uint32_t)0x00040000)
#define WRP_SECTOR19                     ((uint32_t)0x00080000)
#define WRP_SECTOR20                     ((uint32_t)0x00100000)
#define WRP_SECTOR21                     ((uint32_t)0x00200000)
#define WRP_SECTOR22                     ((uint32_t)0x00400000)
#define WRP_SECTOR23                     ((uint32_t)0x00800000)
#define WRP_SECTOR24                     ((uint32_t)0x01000000)
#define WRP_SECTOR25                     ((uint32_t)0x02000000)
#define WRP_SECTOR26                     ((uint32_t)0x04000000)
#define WRP_SECTOR27                     ((uint32_t)0x08000000)
#define WRP_SECTOR28                     ((uint32_t)0x10000000)
#define WRP_SECTOR29                     ((uint32_t)0x20000000)
#define WRP_SECTOR30                     ((uint32_t)0x40000000)
#define WRP_SECTOR31                     ((uint32_t)0x80000000)
#define WRP_ALLSECTORS                   ((uint32_t)0xFFFFFFFF)

#ifndef FMC_CMR_PG
#define FMC_CMR_PG                       ((uint32_t)0x00000001)
#endif
#ifndef FMC_CMR_PE
#define FMC_CMR_PE                       ((uint32_t)0x00000002)
#endif
#ifndef FMC_CMR_ME
#define FMC_CMR_ME                       ((uint32_t)0x00000004)
#endif
#ifndef FMC_CMR_OBPG
#define FMC_CMR_OBPG                     ((uint32_t)0x00000010)
#endif
#ifndef FMC_CMR_OBER
#define FMC_CMR_OBER                     ((uint32_t)0x00000020)
#endif
#ifndef FMC_CMR_START
#define FMC_CMR_START                    ((uint32_t)0x00000040)
#endif
#ifndef FMC_CMR_LK
#define FMC_CMR_LK                       ((uint32_t)0x00000080)
#endif
#ifndef FMC_CMR_OBWE
#define FMC_CMR_OBWE                     ((uint32_t)0x00000200)
#endif
#ifndef FMC_CMR_ERRIE
#define FMC_CMR_ERRIE                    ((uint32_t)0x00000400)
#endif
#ifndef FMC_CMR_EOPIE
#define FMC_CMR_EOPIE                    ((uint32_t)0x00001000)
#endif
#ifndef FMC_CMR_OPTR
#define FMC_CMR_OPTR                     ((uint32_t)0x00002000)
#endif

#ifndef FMC_CSR_BUSY
#define FMC_CSR_BUSY                     ((uint32_t)0x00000001)
#endif
#ifndef FMC_CSR_PGEF
#define FMC_CSR_PGEF                     ((uint32_t)0x00000004)
#endif
#ifndef FMC_CSR_WPEF
#define FMC_CSR_WPEF                     ((uint32_t)0x00000010)
#endif
#ifndef FMC_CSR_ENDF
#define FMC_CSR_ENDF                     ((uint32_t)0x00000020)
#endif

#ifndef FMC_CSR2_BUSY
#define FMC_CSR2_BUSY                    ((uint32_t)0x80000001)
#endif
#ifndef FMC_CSR2_PGEF
#define FMC_CSR2_PGEF                    ((uint32_t)0x80000004)
#endif
#ifndef FMC_CSR2_WPEF
#define FMC_CSR2_WPEF                    ((uint32_t)0x80000010)
#endif
#ifndef FMC_CSR2_ENDF
#define FMC_CSR2_ENDF                    ((uint32_t)0x80000020)
#endif

#ifndef FMC_OPTR_PLEVEL1
#define FMC_OPTR_PLEVEL1                 ((uint32_t)0x00000002)
#endif

#define FMC_INT_EOP                      FMC_CMR_EOPIE
#define FMC_INT_ERR                      FMC_CMR_ERRIE
#define FMC_INT_B2_EOP                   (0x80000000u | FMC_CMR_EOPIE)
#define FMC_INT_B2_ERR                   (0x80000000u | FMC_CMR_ERRIE)

#define FMC_FLAG_BSY                     FMC_CSR_BUSY
#define FMC_FLAG_PERR                    FMC_CSR_PGEF
#define FMC_FLAG_WERR                    FMC_CSR_WPEF
#define FMC_FLAG_EOP                     FMC_CSR_ENDF
#define FMC_FLAG_OPTERR                  FMC_OPTR_PLEVEL1
#define FMC_FLAG_B2_BSY                  FMC_CSR2_BUSY
#define FMC_FLAG_B2_PERR                 FMC_CSR2_PGEF
#define FMC_FLAG_B2_WERR                 FMC_CSR2_WPEF
#define FMC_FLAG_B2_EOP                  FMC_CSR2_ENDF

typedef enum {
    FMC_READY = 0,
    FMC_BSY,
    FMC_PGERR,
    FMC_WRPERR,
    FMC_TIMEOUT_ERR
} FMC_State;

void FMC_Unlock(void);
void FMC_UnlockB1(void);
void FMC_UnlockB2(void);
void FMC_Lock(void);
void FMC_LockB1(void);
void FMC_LockB2(void);
FMC_State FMC_ErasePage(uint32_t Page_Address);
FMC_State FMC_MassErase(void);
FMC_State FMC_MassB1Erase(void);
FMC_State FMC_MassB2Erase(void);
FMC_State FMC_ProgramWord(uint32_t Address, uint32_t Data);
void FMC_OB_Unlock(void);
void FMC_OB_Lock(void);
void FMC_OB_Reset(void);
FMC_State FMC_OB_Erase(void);
FMC_State FMC_OB_EnableWRP(uint32_t OB_WRP);
FMC_State FMC_ReadOutProtection(TypeState NewValue);
FMC_State FMC_OB_RDPConfig(uint8_t OB_RDP);
FMC_State FMC_OB_UserConfig(uint8_t OB_IWDG, uint8_t OB_DEEPSLEEP, uint8_t OB_STDBY);
FMC_State FMC_OB_BOOTConfig(uint8_t OB_BOOT);
FMC_State FMC_OB_WriteUser(uint8_t OB_USER);
FMC_State FMC_ProgramOptionByteData(uint32_t Address, uint8_t Data);
uint8_t FMC_OB_GetUser(void);
uint32_t FMC_OB_GetWRP(void);
TypeState FMC_OB_GetRDP(void);
void FMC_INTConfig(uint32_t FMC_INT, TypeState NewValue);
TypeState FMC_GetBitState(uint32_t FMC_FLAG);
void FMC_ClearBitState(uint32_t FMC_FLAG);
FMC_State FMC_GetState(void);
FMC_State FMC_GetB1State(void);
FMC_State FMC_GetB2State(void);
FMC_State FMC_WaitReady(uint32_t uCount);
FMC_State FMC_B1_WaitReady(uint32_t uCount);
FMC_State FMC_B2_WaitReady(uint32_t uCount);

#ifdef __cplusplus
}
#endif

#endif /* __GD32F10X_FMC_H */
