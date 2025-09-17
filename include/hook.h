//
// Created by liujilan on 25-9-17.
//

#ifndef HOOK_H
#define HOOK_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "default64mbdtc.h"

extern uint32_t ram_amt;		// # 模拟的内存大小
extern int fail_on_all_faults;	// # Flag, 用于解读-d指令, 如果为 1 则大循环立刻报错停机;
								// # 否则进入HandleException后原地返回(可以在hook里处理些东西)
								// # 这之后直接继续在内部转跳到 mtvec 的逻辑

#ifdef __cplusplus
extern "C" {
#endif

	int64_t SimpleReadNumberInt( const char * number, int64_t defaultNumber ); 	// # 辅助函数, 用于参数的数字解读
	uint64_t GetTimeMicroseconds();												// # 获取系统时间的胶水
	void ResetKeyboardInput();													// # UART相关
	void CaptureKeyboardInput();												// # UART相关
	uint32_t HandleException( uint32_t ir, uint32_t retval );					// # fail_on_all_faults = 1 时的处理函数
	uint32_t HandleControlStore( uint32_t addy, uint32_t val );					// 用于MMIO写操作的处理
	uint32_t HandleControlLoad( uint32_t addy );								// 用于MMIO读操作的处理
	void HandleOtherCSRWrite( uint8_t * image, uint16_t csrno, uint32_t value );// 用于未知CSR的写操作的处理
	int32_t HandleOtherCSRRead( uint8_t * image, uint16_t csrno );				// 用于未知CSR的读操作的处理
	void MiniSleep();															// # WFI时主机以来这个小睡
	int IsKBHit();																// # UART相关
	int ReadKBByte();															// # UART相关

#ifdef __cplusplus
}
#endif

// This is the functionality we want to override in the emulator.
//  think of this as the way the emulator's processor is connected to the outside world.
#define MINIRV32WARN( x... ) printf( x );
#define MINIRV32_DECORATE 	// 改为留空
#define MINI_RV32_RAM_SIZE ram_amt
#define MINIRV32_IMPLEMENTATION
#define MINIRV32_POSTEXEC( pc, ir, retval ) { if( retval > 0 ) { if( fail_on_all_faults ) { printf( "FAULT\n" ); return 3; } else retval = HandleException( ir, retval ); } }
#define MINIRV32_HANDLE_MEM_STORE_CONTROL( addy, val ) if( HandleControlStore( addy, val ) ) return val;
#define MINIRV32_HANDLE_MEM_LOAD_CONTROL( addy, rval ) rval = HandleControlLoad( addy );
#define MINIRV32_OTHERCSR_WRITE( csrno, value ) HandleOtherCSRWrite( image, csrno, value );
#define MINIRV32_OTHERCSR_READ( csrno, value ) value = HandleOtherCSRRead( image, csrno );

#ifndef MINIRV32_RAM_IMAGE_OFFSET
	#define MINIRV32_RAM_IMAGE_OFFSET  0x80000000
#endif

#ifndef MINIRV32_MMIO_RANGE
	#define MINIRV32_MMIO_RANGE(n)  (0x10000000 <= (n) && (n) < 0x12000000)
#endif

#ifndef MINIRV32_CUSTOM_MEMORY_BUS
	#define MINIRV32_STORE4( ofs, val ) *(uint32_t*)(image + ofs) = val
	#define MINIRV32_STORE2( ofs, val ) *(uint16_t*)(image + ofs) = val
	#define MINIRV32_STORE1( ofs, val ) *(uint8_t*)(image + ofs) = val
	#define MINIRV32_LOAD4( ofs ) *(uint32_t*)(image + ofs)
	#define MINIRV32_LOAD2( ofs ) *(uint16_t*)(image + ofs)
	#define MINIRV32_LOAD1( ofs ) *(uint8_t*)(image + ofs)
	#define MINIRV32_LOAD2_SIGNED( ofs ) *(int16_t*)(image + ofs)
	#define MINIRV32_LOAD1_SIGNED( ofs ) *(int8_t*)(image + ofs)
#endif

#endif //HOOK_H
