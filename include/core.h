//
// Created by liujilan on 25-9-17.
//

#ifndef CORE_H
#define CORE_H

#include "hook.h"

// As a note: We quouple-ify these, because in HLSL, we will be operating with
// uint4's.  We are going to uint4 data to/from system RAM.
//
// We're going to try to keep the full processor state to 12 x uint4.
struct MiniRV32IMAState
{
    uint32_t regs[32];

    uint32_t pc;
    uint32_t mstatus;
    uint32_t cyclel;
    uint32_t cycleh;

    uint32_t timerl;
    uint32_t timerh;
    uint32_t timermatchl;
    uint32_t timermatchh;

    uint32_t mscratch;
    uint32_t mtvec;
    uint32_t mie;
    uint32_t mip;

    uint32_t mepc;
    uint32_t mtval;
    uint32_t mcause;

    // Note: only a few bits are used.  (Machine = 3, User = 0)
    // Bits 0..1 = privilege.
    // Bit 2 = WFI (Wait for interrupt)
    // Bit 3+ = Load/Store reservation LSBs.
    uint32_t extraflags;
};

#ifndef MINIRV32_CUSTOM_INTERNALS
#define CSR( x ) state->x
#define SETCSR( x, val ) { state->x = val; }
#define REG( x ) state->regs[x]
#define REGSET( x, val ) { state->regs[x] = val; }
#endif

// # 即使是纯C函数, 也要extern "C", 否则 C++ 会把它们当成 C++ 符号
#ifdef __cplusplus
extern "C" {
#endif

    int32_t MiniRV32IMAStep(struct MiniRV32IMAState* state,
                            uint8_t* image,
                            uint32_t vProcAddress,
                            uint32_t elapsedUs,
                            int count);

    int32_t MyMiniRV32IMAStep(struct MiniRV32IMAState* state,
                              uint8_t* image,
                              uint32_t vProcAddress,
                              uint32_t elapsedUs,
                              int count);

#ifdef __cplusplus
}
#endif

#endif //CORE_H
