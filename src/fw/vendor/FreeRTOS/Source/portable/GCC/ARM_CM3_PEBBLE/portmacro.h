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


#ifndef PORTMACRO_H
#define PORTMACRO_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------
 * Port specific definitions.
 *
 * The settings in this file configure FreeRTOS correctly for the
 * given hardware and compiler.
 *
 * These settings should not be altered.
 *-----------------------------------------------------------
 */

/* Type definitions. */
#define portCHAR		char
#define portFLOAT		float
#define portDOUBLE		double
#define portLONG		long
#define portSHORT		short
#define portSTACK_TYPE	uint32_t
#define portBASE_TYPE	long

typedef portSTACK_TYPE StackType_t;
typedef long BaseType_t;
typedef unsigned long UBaseType_t;

#if( configUSE_16_BIT_TICKS == 1 )
	typedef uint16_t TickType_t;
	#define portMAX_DELAY ( TickType_t ) 0xffff
#else
	typedef uint32_t TickType_t;
	#define portMAX_DELAY ( TickType_t ) 0xffffffffUL

	/* 32-bit tick type on a 32-bit architecture, so reads of the tick count do
	not need to be guarded with a critical section. */
	#define portTICK_TYPE_IS_ATOMIC 1
#endif
/*-----------------------------------------------------------*/

#define portMPU_REGION_CACHEABLE_BUFFERABLE		( 0x07UL << 16UL )

#define portFIRST_CONFIGURABLE_REGION	    ( 4UL )
#define portLAST_CONFIGURABLE_REGION		( 7UL )
#define portNUM_CONFIGURABLE_REGIONS		( ( portLAST_CONFIGURABLE_REGION - portFIRST_CONFIGURABLE_REGION ) + 1 )

#define portSWITCH_TO_USER_MODE() __asm volatile ( " mrs r0, control \n orr r0, #1 \n msr control, r0 " :::"r0" )

#define portUSING_MPU                       1

typedef struct MPU_REGION_REGISTERS
{
	unsigned portLONG ulRegionBaseAddress;
	unsigned portLONG ulRegionAttribute;
} xMPU_REGION_REGISTERS;

/* Plus 1 to create space for the stack region. */
typedef struct MPU_SETTINGS
{
	xMPU_REGION_REGISTERS xRegion[ portNUM_CONFIGURABLE_REGIONS ];
} xMPU_SETTINGS;

/* Architecture specifics. */
#define portSTACK_GROWTH			( -1 )
#define portTICK_PERIOD_MS			( ( TickType_t ) 1000 / configTICK_RATE_HZ )
#define portBYTE_ALIGNMENT			8
/*-----------------------------------------------------------*/

/* SVC numbers for various services. */
#define portSVC_START_SCHEDULER				0
#define portSVC_YIELD						1
#define portSVC_RAISE_PRIVILEGE				2

/* Scheduler utilities. */

#define portYIELD()				__asm volatile ( "	SVC	%0	\n" :: "i" (portSVC_YIELD) )
#define portYIELD_WITHIN_API()	*(portNVIC_INT_CTRL) = portNVIC_PENDSVSET

#define portNVIC_INT_CTRL			( ( volatile uint32_t *) 0xe000ed04 )
#define portNVIC_PENDSVSET			0x10000000
#define portEND_SWITCHING_ISR( xSwitchRequired ) if( xSwitchRequired ) *(portNVIC_INT_CTRL) = portNVIC_PENDSVSET
#define portYIELD_FROM_ISR( x ) portEND_SWITCHING_ISR( x )
/*-----------------------------------------------------------*/


/* Critical section management. */

/*
 * Set basepri to portMAX_SYSCALL_INTERRUPT_PRIORITY without effecting other
 * registers.  r0 is clobbered.
 */
#define portSET_INTERRUPT_MASK()						\
	__asm volatile										\
	(													\
		"	mov r0, %0								\n"	\
		"	msr basepri, r0							\n" \
		::"i"(configMAX_SYSCALL_INTERRUPT_PRIORITY):"r0"	\
	)

/*
 * Set basepri back to 0 without effective other registers.
 * r0 is clobbered.  FAQ:  Setting BASEPRI to 0 is not a bug.  Please see
 * http://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html before disagreeing.
 */
#define portCLEAR_INTERRUPT_MASK()			\
	__asm volatile							\
	(										\
		"	mov r0, #0					\n"	\
		"	msr basepri, r0				\n"	\
		:::"r0"								\
	)

/* FAQ:  Setting BASEPRI to 0 is not a bug.  Please see
http://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html before disagreeing. */
#define portSET_INTERRUPT_MASK_FROM_ISR()		0;portSET_INTERRUPT_MASK()
#define portCLEAR_INTERRUPT_MASK_FROM_ISR(x)	portCLEAR_INTERRUPT_MASK();(void)x


extern void vPortEnterCritical( void );
extern void vPortExitCritical( void );
extern bool vPortInCritical( void );

#define portDISABLE_INTERRUPTS()	portSET_INTERRUPT_MASK()
#define portENABLE_INTERRUPTS()		portCLEAR_INTERRUPT_MASK()
#define portENTER_CRITICAL()		vPortEnterCritical()
#define portEXIT_CRITICAL()			vPortExitCritical()
#define portIN_CRITICAL()           vPortInCritical()
/*-----------------------------------------------------------*/

/* Task function macros as described on the FreeRTOS.org WEB site. */
#define portTASK_FUNCTION_PROTO( vFunction, pvParameters ) void vFunction( void *pvParameters )
#define portTASK_FUNCTION( vFunction, pvParameters ) void vFunction( void *pvParameters )

/* Tickless idle/low power functionality. */
#ifndef portSUPPRESS_TICKS_AND_SLEEP
	extern void vPortSuppressTicksAndSleep( TickType_t xExpectedIdleTime );
	#define portSUPPRESS_TICKS_AND_SLEEP( xExpectedIdleTime ) vPortSuppressTicksAndSleep( xExpectedIdleTime )
#endif

extern uintptr_t ulPortGetStackedPC( StackType_t *pxTopOfStack );
#define portGET_STACKED_PC( pxTopOfStack ) ulPortGetStackedPC( pxTopOfStack )

extern uintptr_t ulPortGetStackedLR( StackType_t *pxTopOfStack );
#define portGET_STACKED_LR( pxTopOfStack ) ulPortGetStackedLR( pxTopOfStack )

// Call from the tick handler to bump up the tick count in case the system
// fell behind and missed some tick interrupts (i.e. while running in an emulator).
extern bool vPortCorrectTicks(void);

//! The indexes of the registers as stored on a task's stack by FreeRTOS
typedef enum {
	portTASK_REG_INDEX_CONTROL = 0,
	portTASK_REG_INDEX_R4,
	portTASK_REG_INDEX_R5,
	portTASK_REG_INDEX_R6,
	portTASK_REG_INDEX_R7,
	portTASK_REG_INDEX_R8,
	portTASK_REG_INDEX_R9,
	portTASK_REG_INDEX_R10,
	portTASK_REG_INDEX_R11,
	portTASK_REG_INDEX_R0,
	portTASK_REG_INDEX_R1,
	portTASK_REG_INDEX_R2,
	portTASK_REG_INDEX_R3,
	portTASK_REG_INDEX_R12,
	portTASK_REG_INDEX_LR,
	portTASK_REG_INDEX_PC,
	portTASK_REG_INDEX_XPSR,
} xTASK_REG;

//! The indexes of the registers when stored in canonical form as stored in xPORT_TASK_INFO.registers
typedef enum {
	portCANONICAL_REG_INDEX_R0 = 0,
	portCANONICAL_REG_INDEX_R1,
	portCANONICAL_REG_INDEX_R2,
	portCANONICAL_REG_INDEX_R3,
	portCANONICAL_REG_INDEX_R4,
	portCANONICAL_REG_INDEX_R5,
	portCANONICAL_REG_INDEX_R6,
	portCANONICAL_REG_INDEX_R7,
	portCANONICAL_REG_INDEX_R8,
	portCANONICAL_REG_INDEX_R9,
	portCANONICAL_REG_INDEX_R10,
	portCANONICAL_REG_INDEX_R11,
	portCANONICAL_REG_INDEX_R12,
	portCANONICAL_REG_INDEX_SP,
	portCANONICAL_REG_INDEX_LR,
	portCANONICAL_REG_INDEX_PC,
	portCANONICAL_REG_INDEX_XPSR,
	portCANONICAL_REG_COUNT,
} xCANONICAL_REG;

typedef struct PORT_TASK_INFO
{
	portCHAR const		*pcName;
	void				*taskHandle; // Can be compared to xTaskGetCurrentTaskHandle()
	unsigned portLONG	registers[portCANONICAL_REG_COUNT];	// task registers
} xPORT_TASK_INFO;

extern void vPortGetTaskInfo( void *xTaskHandle, char const * pcTaskName, StackType_t *pxTopOfStack,
							 xPORT_TASK_INFO *pxTaskInfo );

extern uint32_t ulPortGetStackedControl( StackType_t* pxTopOfStack );
#define portGET_STACKED_CONTROL( pxTopOfStack ) ulPortGetStackedControl( pxTopOfStack )

#ifdef __cplusplus
}
#endif

#endif /* PORTMACRO_H */

