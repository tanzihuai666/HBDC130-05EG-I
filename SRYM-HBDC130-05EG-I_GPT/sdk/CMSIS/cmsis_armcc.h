/**************************************************************************//**
 * @file     cmsis_armcc.h
 * @brief    CMSIS compiler ARMCC header for ARM Compiler 5
 ******************************************************************************/

#ifndef __CMSIS_ARMCC_H
#define __CMSIS_ARMCC_H

#if defined(__ARMCC_VERSION) && (__ARMCC_VERSION < 400677)
  #error "Please use ARM Compiler Toolchain V4.0.677 or later!"
#endif

#ifndef   __ASM
  #define __ASM                                  __asm
#endif
#ifndef   __INLINE
  #define __INLINE                               __inline
#endif
#ifndef   __STATIC_INLINE
  #define __STATIC_INLINE                        static __inline
#endif
#ifndef   __STATIC_FORCEINLINE
  #define __STATIC_FORCEINLINE                   static __forceinline
#endif
#ifndef   __NO_RETURN
  #define __NO_RETURN                            __declspec(noreturn)
#endif
#ifndef   __USED
  #define __USED                                 __attribute__((used))
#endif
#ifndef   __WEAK
  #define __WEAK                                 __attribute__((weak))
#endif
#ifndef   __PACKED
  #define __PACKED                               __attribute__((packed))
#endif
#ifndef   __PACKED_STRUCT
  #define __PACKED_STRUCT                        __packed struct
#endif
#ifndef   __PACKED_UNION
  #define __PACKED_UNION                         __packed union
#endif
#ifndef   __UNALIGNED_UINT16_WRITE
  #define __UNALIGNED_UINT16_WRITE(addr, val)    (*((__packed uint16_t *)(addr)) = (val))
#endif
#ifndef   __UNALIGNED_UINT16_READ
  #define __UNALIGNED_UINT16_READ(addr)          (*((const __packed uint16_t *)(addr)))
#endif
#ifndef   __UNALIGNED_UINT32_WRITE
  #define __UNALIGNED_UINT32_WRITE(addr, val)    (*((__packed uint32_t *)(addr)) = (val))
#endif
#ifndef   __UNALIGNED_UINT32_READ
  #define __UNALIGNED_UINT32_READ(addr)          (*((const __packed uint32_t *)(addr)))
#endif
#ifndef   __UNALIGNED_UINT32
  #define __UNALIGNED_UINT32(x)                  (*((__packed uint32_t *)(x)))
#endif

#define __NOP                                    __nop
#define __WFI                                    __wfi
#define __WFE                                    __wfe
#define __SEV                                    __sev
#define __ISB()                                  __isb(0xF)
#define __DSB()                                  __dsb(0xF)
#define __DMB()                                  __dmb(0xF)
#define __REV                                    __rev
#define __REV16                                  __rev16
#define __REVSH                                  __revsh
#define __ROR                                    __ror
#define __BKPT(value)                            __breakpoint(value)
#define __RBIT                                   __rbit
#define __CLZ                                    __clz
#define __SSAT                                   __ssat
#define __USAT                                   __usat

#define __disable_irq                            __disable_irq
#define __enable_irq                             __enable_irq

#endif /* __CMSIS_ARMCC_H */
