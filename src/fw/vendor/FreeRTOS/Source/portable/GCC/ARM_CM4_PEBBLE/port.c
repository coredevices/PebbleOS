/*
    FreeRTOS V8.2.1 - Copyright (C) 2015 Real Time Engineers Ltd.
    All rights reserved

    VISIT http://www.FreeRTOS.org TO ENSURE YOU ARE USING THE LATEST VERSION.

    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation >>!AND MODIFIED BY!<< the FreeRTOS exception.

    ***************************************************************************
    >>!   NOTE: The modification to the GPL is included to allow you to     !<<
    >>!   distribute a combined work that includes FreeRTOS without being   !<<
    >>!   obliged to provide the source code for proprietary components     !<<
    >>!   outside of the FreeRTOS kernel.                                   !<<
    ***************************************************************************

    FreeRTOS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  Full license text is available on the following
    link: http://www.freertos.org/a00114.html

    ***************************************************************************
     *                                                                       *
     *    FreeRTOS provides completely free yet professionally developed,    *
     *    robust, strictly quality controlled, supported, and cross          *
     *    platform software that is more than just the market leader, it     *
     *    is the industry's de facto standard.                               *
     *                                                                       *
     *    Help yourself get started quickly while simultaneously helping     *
     *    to support the FreeRTOS project by purchasing a FreeRTOS           *
     *    tutorial book, reference manual, or both:                          *
     *    http://www.FreeRTOS.org/Documentation                              *
     *                                                                       *
    ***************************************************************************

    http://www.FreeRTOS.org/FAQHelp.html - Having a problem?  Start by reading
    the FAQ page "My application does not run, what could be wrong?".  Have you
    defined configASSERT()?

    http://www.FreeRTOS.org/support - In return for receiving this top quality
    embedded software for free we request you assist our global community by
    participating in the support forum.

    http://www.FreeRTOS.org/training - Investing in training allows your team to
    be as productive as possible as early as possible.  Now you can receive
    FreeRTOS training directly from Richard Barry, CEO of Real Time Engineers
    Ltd, and the world's leading authority on the world's leading RTOS.

    http://www.FreeRTOS.org/plus - A selection of FreeRTOS ecosystem products,
    including FreeRTOS+Trace - an indispensable productivity tool, a DOS
    compatible FAT file system, and our tiny thread aware UDP/IP stack.

    http://www.FreeRTOS.org/labs - Where new FreeRTOS products go to incubate.
    Come and try FreeRTOS+TCP, our new open source TCP/IP stack for FreeRTOS.

    http://www.OpenRTOS.com - Real Time Engineers ltd. license FreeRTOS to High
    Integrity Systems ltd. to sell under the OpenRTOS brand.  Low cost OpenRTOS
    licenses offer ticketed support, indemnification and commercial middleware.

    http://www.SafeRTOS.com - High Integrity Systems also provide a safety
    engineered and independently SIL3 certified version for use in safety and
    mission critical applications that require provable dependability.

    1 tab == 4 spaces!
*/

/*-----------------------------------------------------------
 * Implementation of functions defined in portable.h for the ARM CM4F port.
 *----------------------------------------------------------*/

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <string.h>

#ifndef __VFP_FP__
	#error This port can only be used when the project options are configured to enable hardware floating point support.
#endif

/* Constants required to access and manipulate the NVIC. */
#define portNVIC_SYSTICK_CTRL					( ( volatile uint32_t * ) 0xe000e010 )
#define portNVIC_SYSTICK_LOAD					( ( volatile uint32_t * ) 0xe000e014 )
#define portNVIC_SYSPRI2						( ( volatile uint32_t * ) 0xe000ed20 )
#define portNVIC_SYSPRI1						( ( volatile uint32_t * ) 0xe000ed1c )
#define portNVIC_SYS_CTRL_STATE					( ( volatile uint32_t * ) 0xe000ed24 )
#define portNVIC_MEM_FAULT_ENABLE				( 1UL << 16UL )

/* Constants required to access and manipulate the MPU. */
#define portMPU_TYPE							( ( volatile uint32_t * ) 0xe000ed90 )
#define portMPU_REGION_BASE_ADDRESS				( ( volatile uint32_t * ) 0xe000ed9C )
#define portMPU_REGION_ATTRIBUTE				( ( volatile uint32_t * ) 0xe000edA0 )
#define portMPU_CTRL							( ( volatile uint32_t * ) 0xe000ed94 )
#define portMPU_REGION_VALID					( 0x10UL )
#define portMPU_REGION_ENABLE					( 0x01UL )
#define portPERIPHERALS_START_ADDRESS			0x40000000UL
#define portPERIPHERALS_END_ADDRESS				0x5FFFFFFFUL

/* Constants required to access and manipulate the SysTick. */
#define portNVIC_SYSTICK_CLK					( 0x00000004UL )
#define portNVIC_SYSTICK_INT					( 0x00000002UL )
#define portNVIC_SYSTICK_ENABLE					( 0x00000001UL )
#define portNVIC_PENDSV_PRI						( ( ( uint32_t ) configKERNEL_INTERRUPT_PRIORITY ) << 16UL )
#define portNVIC_SYSTICK_PRI					( ( ( uint32_t ) configKERNEL_INTERRUPT_PRIORITY ) << 24UL )
#define portNVIC_SVC_PRI						( ( ( uint32_t ) configKERNEL_INTERRUPT_PRIORITY ) << 24UL )

/* Constants required to manipulate the VFP. */
#define portFPCCR								( ( volatile uint32_t * ) 0xe000ef34 ) /* Floating point context control register. */
#define portASPEN_AND_LSPEN_BITS				( 0x3UL << 30UL )

/* Constants required to set up the initial stack. */
#define portINITIAL_XPSR						( 0x01000000 )
#define portINITIAL_EXEC_RETURN					( 0xfffffffd )
#define portINITIAL_CONTROL_IF_UNPRIVILEGED		( 0x03 )
#define portINITIAL_CONTROL_IF_PRIVILEGED		( 0x02 )

/* Offsets in the stack to the parameters when inside the SVC handler. */
#define portOFFSET_TO_PC						( 6 )
#define portOFFSET_TO_LR						( 5 )
#define portOFFSET_TO_PSR						( 7 )

/* Each task maintains its own interrupt status in the critical nesting
variable.  Note this is not saved as part of the task context as context
switches can only occur when uxCriticalNesting is zero. */
#define portCRITICAL_NESTING_INIT_VALUE 0xaaaaaaaa
static UBaseType_t uxCriticalNesting = portCRITICAL_NESTING_INIT_VALUE;

/*
 * Setup the timer to generate the tick interrupts.
 */
static void prvSetupTimerInterrupt( void ) PRIVILEGED_FUNCTION;

/*
 * Standard FreeRTOS exception handlers.
 */
void xPortPendSVHandler( void ) __attribute__ (( naked )) PRIVILEGED_FUNCTION;
void xPortSysTickHandler( void )  __attribute__ ((optimize("3"))) PRIVILEGED_FUNCTION;
void vPortSVCHandler( void ) __attribute__ (( naked )) PRIVILEGED_FUNCTION;

/*
 * Starts the scheduler by restoring the context of the first task to run.
 */
static void prvRestoreContextOfFirstTask( void ) __attribute__(( naked )) PRIVILEGED_FUNCTION;

/*
 * C portion of the SVC handler.  The SVC handler is split between an asm entry
 * and a C wrapper for simplicity of coding and maintenance.
 */
static void prvSVCHandler( uint32_t *pulRegisters ) __attribute__(( noinline )) PRIVILEGED_FUNCTION;

/*
 * Function to enable the VFP.
 */
static void vPortEnableVFP( void ) __attribute__ (( naked ));

/* Used by gdb client (openocd) to determine how registers are stacked */
uint8_t uxFreeRTOSRegisterStackingVersion = 2;

/*-----------------------------------------------------------*/

/*
 * See header file for description.
 */
StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters )
{
	/* Simulate the stack frame as it would be created by a context switch
	interrupt. */
	pxTopOfStack--; /* Offset added to account for the way the MCU uses the stack on entry/exit of interrupts. */
	*pxTopOfStack = portINITIAL_XPSR;	/* xPSR */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) pxCode;	/* PC */
	pxTopOfStack--;
	*pxTopOfStack = 0;	/* LR */
	pxTopOfStack -= 5;	/* R12, R3, R2 and R1. */
	*pxTopOfStack = ( StackType_t ) pvParameters;	/* R0 */

	/* A save method is being used that requires each task to maintain its own exec return value. */
	pxTopOfStack--;
	*pxTopOfStack = portINITIAL_EXEC_RETURN;

	pxTopOfStack -= 9;	/* R11, R10, R9, R8, R7, R6, R5 and R4. */

	*pxTopOfStack = portINITIAL_CONTROL_IF_PRIVILEGED;

	return pxTopOfStack;
}
/*-----------------------------------------------------------*/

void vPortSVCHandler( void )
{
	/* Assumes psp was in use. */
	__asm volatile
	(
		#ifndef USE_PROCESS_STACK	/* Code should not be required if a main() is using the process stack. */
			"	tst lr, #4						\n"
			"	ite eq							\n"
			"	mrseq r0, msp					\n"
			"	mrsne r0, psp					\n"
		#else
			"	mrs r0, psp						\n"
		#endif
			"	b %0							\n"
			::"i"(prvSVCHandler):"r0"
	);
}
/*-----------------------------------------------------------*/

extern bool xApplicationIsAllowedToRaisePrivilege( uint32_t caller_pc );
extern void vSetupSyscallRegisters( uint32_t orig_sp, uint32_t *lr_ptr );

static uintptr_t prvCalculateOriginalSP( uint32_t *exception_sp )
{
	/* This calculation assumes floating point stacking is disabled
	 * on exception entry */

	/* The exception frame is laid out as follows:
	 * {aligner}, xPSR, PC, LR, R12, r3, r2, r1, r0: 0x20 or 0x24 bytes */
	uintptr_t original_sp = (uintptr_t)exception_sp + 0x20;

	/* Determine if the aligner exists: */
	if (exception_sp[portOFFSET_TO_PSR] & SCB_CCR_STKALIGN_Msk) {
		original_sp += 4;
	}

	return original_sp;
}

static void prvSVCHandler( uint32_t *pulParam )
{
uint8_t ucSVCNumber;

	/* The stack contains: r0, r1, r2, r3, r12, r14, the return address and
	xPSR.  The first argument (r0) is pulParam[ 0 ]. */
	ucSVCNumber = ( ( uint8_t * ) pulParam[ portOFFSET_TO_PC ] )[ -2 ];
	switch( ucSVCNumber )
	{
		case portSVC_START_SCHEDULER	:	*(portNVIC_SYSPRI1) |= portNVIC_SVC_PRI;
											prvRestoreContextOfFirstTask();
											break;

		case portSVC_YIELD				:	*(portNVIC_INT_CTRL) = portNVIC_PENDSVSET;
											/* Barriers are normally not required
											but do ensure the code is completely
											within the specified behaviour for the
											architecture. */
											__asm volatile( "dsb" );
											__asm volatile( "isb" );

											break;

		case portSVC_RAISE_PRIVILEGE	:	{
												uint32_t caller_pc = pulParam[portOFFSET_TO_PC];
												if (xApplicationIsAllowedToRaisePrivilege(caller_pc))
												{
													/* Setup necessary information for syscall protection */
													vSetupSyscallRegisters(prvCalculateOriginalSP(pulParam), pulParam + portOFFSET_TO_LR);

													/* Modify the control register to raise the thread mode privilege level */
													__asm volatile
													(
														"	mrs r1, control		\n" /* Obtain current control value. */
														"	bic r1, #1			\n" /* Set privilege bit. */
														"	msr control, r1		\n" /* Write back new control value. */
														"	isb					\n"
														:::"r1"
													);
												}
											}
											break;
				
		  default						:	/* Unknown SVC call. */
											break;
	}
}
/*-----------------------------------------------------------*/

static void prvRestoreContextOfFirstTask( void )
{
	__asm volatile
	(
		"	ldr r0, =0xE000ED08				\n" /* Use the NVIC offset register to locate the stack. */
		"	ldr r0, [r0]					\n"
		"	ldr r0, [r0]					\n"
		"	msr msp, r0						\n" /* Set the msp back to the start of the stack. */
		"	ldr	r3, pxCurrentTCBConst2		\n" /* Restore the context. */
		"	ldr r1, [r3]					\n"
		"	ldr r0, [r1]					\n" /* The first item in the TCB is the task top of stack. */
		"	add r1, r1, #4					\n" /* Move onto the second item in the TCB... */
		"	ldr r2, =0xe000ed9c				\n" /* Region Base Address register. */
		"	ldmia r1!, {r4-r11}				\n" /* Read 4 sets of MPU registers. */
		"	stmia r2!, {r4-r11}				\n" /* Write 4 sets of MPU registers. */
		"	ldmia r0!, {r3, r4-r11, r14}	\n" /* Pop the registers that are not automatically saved on exception entry. */
		"	msr control, r3					\n"
		"	msr psp, r0						\n" /* Restore the task stack pointer. */
		"	mov r0, #0						\n"
		"	msr	basepri, r0					\n"
		"	isb								\n"
		"	bx r14							\n"
		"									\n"
		"	.align 2						\n"
		"pxCurrentTCBConst2: .word pxCurrentTCB	\n"
		"	.ltorg							\n"
	);
}
/*-----------------------------------------------------------*/

/*
 * See header file for description.
 */
BaseType_t xPortStartScheduler( void )
{
	/* configMAX_SYSCALL_INTERRUPT_PRIORITY must not be set to 0.  See
	http://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html */
	configASSERT( ( configMAX_SYSCALL_INTERRUPT_PRIORITY ) );

	/* Make PendSV and SysTick the same priority as the kernel. */
	*(portNVIC_SYSPRI2) |= portNVIC_PENDSV_PRI;
	*(portNVIC_SYSPRI2) |= portNVIC_SYSTICK_PRI;

	/* Start the timer that generates the tick ISR.  Interrupts are disabled
	here already. */
	prvSetupTimerInterrupt();

	/* Initialise the critical nesting count ready for the first task. */
	uxCriticalNesting = 0;

	/* Ensure the VFP is enabled - it should be anyway. */
	vPortEnableVFP();

	/* Lazy save always. */
	*( portFPCCR ) |= portASPEN_AND_LSPEN_BITS;

	/* Start the first task. */
	__asm volatile( "	dsb		\n"
					"	svc %0	\n"
					"	isb		\n"
					:: "i" (portSVC_START_SCHEDULER) );

	/* Should not get here! */
	return 0;
}
/*-----------------------------------------------------------*/

void vPortEndScheduler( void )
{
	/* Not implemented in ports where there is nothing to return to.
	Artificially force an assert. */
	configASSERT( uxCriticalNesting == 1000UL );
}
/*-----------------------------------------------------------*/

void vPortEnterCritical( void )
{
	portDISABLE_INTERRUPTS();
	uxCriticalNesting++;
}
/*-----------------------------------------------------------*/

void vPortExitCritical( void )
{
	configASSERT( uxCriticalNesting );
	uxCriticalNesting--;
	if( uxCriticalNesting == 0 )
	{
		portENABLE_INTERRUPTS();
	}
}
/*-----------------------------------------------------------*/

bool vPortInCritical( void )
{
	return (uxCriticalNesting > 0 && uxCriticalNesting != portCRITICAL_NESTING_INIT_VALUE);
}
/*-----------------------------------------------------------*/

void xPortPendSVHandler( void )
{
	/* This is a naked function. */

	__asm volatile
	(
		"	mrs r0, psp							\n"
		"	isb									\n"
		"										\n"
		"	ldr	r3, pxCurrentTCBConst			\n" /* Get the location of the current TCB. */
		"	ldr	r2, [r3]						\n"
		"										\n"
// Using real FPU
#if defined(__VFP_FP__) && !defined(__SOFTFP__)
		"	tst r14, #0x10						\n" /* Is the task using the FPU context?  If so, push high vfp registers. */
		"	it eq								\n"
		"	vstmdbeq r0!, {s16-s31}				\n"
#endif
		"										\n"
		"	mrs r1, control						\n"
		"	stmdb r0!, {r1, r4-r11, r14}		\n" /* Save the remaining registers. */
		"	str r0, [r2]						\n" /* Save the new top of stack into the first member of the TCB. */
		"										\n"
		"	stmdb sp!, {r3, r14}				\n"
		"	mov r0, %0							\n"
		"	msr basepri, r0						\n"
		"	dsb									\n"
		"	isb									\n"
		"	bl vTaskSwitchContext				\n"
		"	mov r0, #0							\n"
		"	msr basepri, r0						\n"
		"	ldmia sp!, {r3, r14}				\n"
		"										\n"	/* Restore the context. */
		"	ldr r1, [r3]						\n" /* r1 is a pointer to the TCB struct. */
		"	ldr r0, [r1]						\n" /* The first item in the TCB is the task top of stack. */
		"	add r1, r1, #4						\n" /* Move onto the second item in the TCB... */
		"										\n"
		"	ldr r2, =0xe000ed9c					\n" /* Region Base Address register. */
		"	ldmia r1!, {r4-r11}					\n" /* Read 4 sets of MPU registers. */
		"	stmia r2!, {r4-r11}					\n" /* Write 4 sets of MPU registers. */
		"										\n"
		"	ldmia r0!, {r3, r4-r11, r14}		\n" /* Pop the registers that are not automatically saved on exception entry. */
		"										\n"
		"	msr control, r3						\n"
		"										\n"
// Using real FPU
#if defined(__VFP_FP__) && !defined(__SOFTFP__)
		"	tst r14, #0x10						\n" /* Is the task using the FPU context?  If so, pop the high vfp registers too. */
		"	it eq								\n"
		"	vldmiaeq r0!, {s16-s31}				\n"
#endif
		"										\n"
		"	msr psp, r0							\n"
		"	isb									\n"
		"	bx r14								\n"
		"										\n"
		"	.align 2							\n"
		"pxCurrentTCBConst: .word pxCurrentTCB	\n"
		"	.ltorg							\n"
		::"i"(configMAX_SYSCALL_INTERRUPT_PRIORITY)
	);
}
/*-----------------------------------------------------------*/

void xPortSysTickHandler( void )
{
uint32_t ulDummy;

	ulDummy = portSET_INTERRUPT_MASK_FROM_ISR();
	{
		/* Increment the RTOS tick. */
		if( xTaskIncrementTick() != pdFALSE )
		{
			/* Pend a context switch. */
			portYIELD_WITHIN_API();
		}
#ifdef TARGET_QEMU
		/* When running under emulation, it might be necessary to correct ticks in case we
		   fell behind and missed some tick interrupts */
		if( vPortCorrectTicks() )
		{
			/* Pend a context switch. */
			portYIELD_WITHIN_API();
		}
#endif
	}
	portCLEAR_INTERRUPT_MASK_FROM_ISR( ulDummy );
}
/*-----------------------------------------------------------*/

/*
 * Setup the systick timer to generate the tick interrupts at the required
 * frequency.
 */
static void prvSetupTimerInterrupt( void )
{
	/* Configure SysTick to interrupt at the requested rate. */
	*(portNVIC_SYSTICK_LOAD) = ( configCPU_CLOCK_HZ / configTICK_RATE_HZ ) - 1UL;
	*(portNVIC_SYSTICK_CTRL) = portNVIC_SYSTICK_CLK | portNVIC_SYSTICK_INT | portNVIC_SYSTICK_ENABLE;
}
/*-----------------------------------------------------------*/

/* This is a naked function. */
static void vPortEnableVFP( void )
{
	__asm volatile
	(
		" ldr.w r0, =0xE000ED88			\n" /* The FPU enable bits are in the CPACR. */
		" ldr r1, [r0]					\n"
		"								\n"
		" orr r1, r1, #( 0xf << 20 )	\n" /* Enable CP10 and CP11 coprocessors, then save back. */
		" str r1, [r0]					\n"
		" bx r14						\n"
		" .ltorg						\n"
	);
}
/*-----------------------------------------------------------*/

void vPortStoreTaskMPUSettings( xMPU_SETTINGS *xMPUSettings, const struct xMEMORY_REGION * const xRegions, StackType_t *pxBottomOfStack, uint16_t usStackDepth )
{
unsigned long ul;
void *pvBaseAddress;
uint32_t ulParameters;

	for( ul = 0; ul < portNUM_CONFIGURABLE_REGIONS; ul++ )
	{
		if (xRegions != NULL)
		{
			pvBaseAddress = xRegions[ ul ].pvBaseAddress;
			ulParameters = xRegions[ ul ].ulParameters;
		}
		else
		{
			pvBaseAddress = 0;
			ulParameters = 0;
		}

		/* Set the configuration MPU regions. We're assuming that when the caller created they task, they setup the
		 * xRegions memory of the TaskParameters_t such that the ulParameters of each region contains what should be
		 * written into the MPU_RASR register */
		xMPUSettings->xRegion[ ul ].ulRegionBaseAddress = (uintptr_t)pvBaseAddress
										| ( portFIRST_CONFIGURABLE_REGION + ul ) | portMPU_REGION_VALID;
		xMPUSettings->xRegion[ ul ].ulRegionAttribute = ulParameters;
	}
}

/*-----------------------------------------------------------*/
void portSetupTCB(void) {
#if __DCACHE_PRESENT
	SCB_CleanDCache();
#endif
#if __ICACHE_PRESENT
	SCB_InvalidateICache();
#endif
}

/*-----------------------------------------------------------*/
// s16-s31 are stacked by the xPortPendSVHandler() (not the CPU)
#define portNUM_EXTRA_STACKED_FLOATING_POINT_REGS (16)

// s0-s15, fpscr, reserved are stacked by the CPU on exception entry
#define portNUM_BASIC_STACKED_FLOATING_POINT_REGS (18)

// if bit 4 is 0 it indicates floating point is in use
#define FLOATING_POINT_ACTIVE(exc_return) ((exc_return & 0x10) == 0)

uintptr_t ulPortGetStackedPC( StackType_t* pxTopOfStack )
{
	int offset = portTASK_REG_INDEX_PC;

	if (FLOATING_POINT_ACTIVE(pxTopOfStack[portTASK_REG_EXC_RETURN])) {
		offset += portNUM_EXTRA_STACKED_FLOATING_POINT_REGS;
	}
	return pxTopOfStack[offset];
}
/*-----------------------------------------------------------*/

uintptr_t ulPortGetStackedLR( StackType_t* pxTopOfStack )
{
	int offset = portTASK_REG_INDEX_LR;

	if (FLOATING_POINT_ACTIVE(pxTopOfStack[portTASK_REG_EXC_RETURN])) {
		offset += portNUM_EXTRA_STACKED_FLOATING_POINT_REGS;
	}
	return pxTopOfStack[offset];
}
/*-----------------------------------------------------------*/

void vPortGetTaskInfo( void *taskHandle, char const *pcTaskName, StackType_t *pxTopOfStack,
					  xPORT_TASK_INFO *pxTaskInfo)
{
	pxTaskInfo->taskHandle = taskHandle;
	pxTaskInfo->pcName = pcTaskName;

	// The contents at the current sp _only_ match the registers the thread was
	// using if the thread is _not_ currently running
	if (taskHandle == xTaskGetCurrentTaskHandle())
	{
	  return;
	}

	// Get the registers off the saved stack. See xPortPendSVHandler() for how the registers are stacked.
	// Registers are stored in task_info in canonical order defined in xCANONICAL_REG: [r0-r12, sp, lr, pc, sr]
	pxTopOfStack++;   // Skip control register
	for (int dstIdx = portCANONICAL_REG_INDEX_R4; dstIdx <=portCANONICAL_REG_INDEX_R11; dstIdx++)
	{
		pxTaskInfo->registers[dstIdx] = *pxTopOfStack++;
	}
	uint32_t exc_return = *pxTopOfStack++;

	// The xPortPendSVHandler() method saves these extra FP registers (s16-s31)
	if (FLOATING_POINT_ACTIVE(exc_return)) {
		pxTopOfStack += portNUM_EXTRA_STACKED_FLOATING_POINT_REGS;
	}

	// The basic registers
	for (int dstIdx = portCANONICAL_REG_INDEX_R0; dstIdx <=portCANONICAL_REG_INDEX_R3; dstIdx++)
	{
		pxTaskInfo->registers[dstIdx] = *pxTopOfStack++;
	}
	pxTaskInfo->registers[portCANONICAL_REG_INDEX_R12] = *pxTopOfStack++;
	pxTaskInfo->registers[portCANONICAL_REG_INDEX_LR] = *pxTopOfStack++;
	pxTaskInfo->registers[portCANONICAL_REG_INDEX_PC] = *pxTopOfStack++;
	pxTaskInfo->registers[portCANONICAL_REG_INDEX_XPSR] = *pxTopOfStack++;

	// When FP is active, the basic FP registers are saved by the CPU before it saves the
	//	basic registers (r0-r3, r12, lr, pc, xpsr)
	if (FLOATING_POINT_ACTIVE(exc_return)) {
		pxTopOfStack += portNUM_BASIC_STACKED_FLOATING_POINT_REGS;
	}

	pxTaskInfo->registers[portCANONICAL_REG_INDEX_SP] = (unsigned portLONG)pxTopOfStack;
}
/*-----------------------------------------------------------*/

extern uint32_t ulPortGetStackedControl( StackType_t* pxTopOfStack )
{
	return pxTopOfStack[portTASK_REG_INDEX_CONTROL];
}
