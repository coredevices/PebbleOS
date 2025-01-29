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


#include <stdlib.h>

/* Defining MPU_WRAPPERS_INCLUDED_FROM_API_FILE prevents task.h from redefining
all the API functions to use the MPU wrappers.  That should only be done when
task.h is included from an application file. */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#include "FreeRTOS.h"
#include "task.h"
#include "light_mutex.h"

/* Lint e961 and e750 are suppressed as a MISRA exception justified because the
MPU ports require MPU_WRAPPERS_INCLUDED_FROM_API_FILE to be defined for the
header files above, but not in this file, in order to generate the correct
privileged Vs unprivileged linkage and placement. */
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE /*lint !e961 !e750. */

/* Remove the whole file if light mutexes not being used. */
#if( configUSE_LIGHT_MUTEXES != 0 )

#if( configUSE_PREEMPTION == 0 )
	/* If the cooperative scheduler is being used then a yield should not be
	performed just because a higher priority task has been woken. */
	#define mutexYIELD_IF_USING_PREEMPTION()
#else
	#define mutexYIELD_IF_USING_PREEMPTION() portYIELD_WITHIN_API()
#endif

typedef struct LightMutexDefinition
{
	TaskHandle_t pxMutexHolder;

	int16_t uxRecursiveCallCount;	/*< Maintains a count of the number of times a recursive mutex has been recursively 'taken'. */
	int8_t uxLocked;

	List_t xTasksWaitingToLock;		/*< List of tasks that are blocked waiting to lock this mutex.  Stored in priority order. */
} LightMutex_t;

/*-----------------------------------------------------------*/

/*
 * Uses a critical section to determine whether the mutex is locked.
 */
static BaseType_t prvIsMutexLocked( const LightMutex_t *pxMutex ) PRIVILEGED_FUNCTION;

/*
 * Unlocks the mutex, clearing the mutex holder.
 */
static BaseType_t prvMutexSetUnlocked( LightMutex_t * const pxMutex ) PRIVILEGED_FUNCTION;

/*-----------------------------------------------------------*/

LightMutexHandle_t xLightMutexCreate( void ) {
	LightMutex_t *pxNewMutex;
	pxNewMutex = ( LightMutex_t * ) pvPortMalloc( sizeof( LightMutex_t ) );
	if( pxNewMutex != NULL )
	{
		/* Information required for priority inheritance. */
		pxNewMutex->pxMutexHolder = NULL;
		pxNewMutex->uxRecursiveCallCount = 0;

		pxNewMutex->uxLocked = pdFALSE;

		/* Ensure the event queues start with the correct state. */
		vListInitialise( &( pxNewMutex->xTasksWaitingToLock ) );
	}
	else
	{
		traceCREATE_LIGHT_MUTEX_FAILED();
	}

	configASSERT( pxNewMutex );
	return pxNewMutex;
}


BaseType_t xLightMutexUnlock( LightMutexHandle_t xMutex ) {
	BaseType_t xReturn, xYieldRequired;

	LightMutex_t * const pxMutex = ( LightMutex_t * ) xMutex;

	configASSERT_SAFE_TO_CALL_FREERTOS_API();
	configASSERT( pxMutex );

	taskENTER_CRITICAL();
	{
		/* Check if the mutex is ready to be unlocked */
		if( pxMutex->uxLocked == pdTRUE )
		{
			traceUNLOCK_LIGHT_MUTEX( pxMutex );
			xYieldRequired = prvMutexSetUnlocked( pxMutex );

			/* If there was a task waiting to lock the mutex then unblock it now. */
			if( listLIST_IS_EMPTY( &( pxMutex->xTasksWaitingToLock ) ) == pdFALSE )
			{
				if( xTaskRemoveFromEventList( &( pxMutex->xTasksWaitingToLock ) ) == pdTRUE )
				{
					/* The unblocked task has a priority higher than
					our own so yield immediately.  Yes it is ok to do
					this from within the critical section - the kernel
					takes care of that. */
					mutexYIELD_IF_USING_PREEMPTION();
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else if( xYieldRequired != pdFALSE )
			{
				/* This path is a special case that will only get
				executed if the task was holding multiple mutexes and
				the mutexes were given back in an order that is
				different to that in which they were taken. */
				mutexYIELD_IF_USING_PREEMPTION();
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}

			xReturn = pdPASS;
		}
		else
		{
			/* The mutex was already unlocked?. */
			traceUNLOCK_LIGHT_MUTEX_FAILED( pxQueue );

			xReturn = pdFAIL;
		}
	}
	taskEXIT_CRITICAL();

	return xReturn;
}

BaseType_t xLightMutexLock( LightMutexHandle_t xMutex, TickType_t xTicksToWait )
{
	BaseType_t xEntryTimeSet = pdFALSE;
	TimeOut_t xTimeOut;
	LightMutex_t * const pxMutex = ( LightMutex_t * ) xMutex;

	configASSERT_SAFE_TO_CALL_FREERTOS_API();
	configASSERT( pxMutex );
	#if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
	{
		configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
	}
	#endif

	/* This function relaxes the coding standard somewhat to allow return
	statements within the function itself.  This is done in the interest
	of execution time efficiency. */

	for( ;; )
	{
		taskENTER_CRITICAL();
		{
			/* Is the mutex unlocked yet? To be running the calling task
			must be the highest priority task wanting to acquire the mutex. */
			if( pxMutex->uxLocked == pdFALSE )
			{
				traceUNLOCK_LIGHT_MUTEX( pxMutex )
				pxMutex->uxLocked = pdTRUE;

				/* Record the information required to implement
				priority inheritance should it become necessary. */
				pxMutex->pxMutexHolder = pvTaskIncrementMutexHeldCount();

				taskEXIT_CRITICAL();
				return pdPASS;
			}
			else
			{
				if( xTicksToWait == ( TickType_t ) 0 )
				{
					/* The mutex was locked and no block time is specified (or
					the block time has expired) so leave now. */
					taskEXIT_CRITICAL();
					traceLOCK_LIGHT_MUTEX_FAILED( pxMutex );
					return errQUEUE_EMPTY;
				}
				else if( xEntryTimeSet == pdFALSE )
				{
					/* The mutex was locked and a block time was specified so
					configure the timeout structure. */
					vTaskSetTimeOutState( &xTimeOut );
					xEntryTimeSet = pdTRUE;
				}
				else
				{
					/* Entry time was already set. */
					mtCOVERAGE_TEST_MARKER();
				}
			}
		}
		taskEXIT_CRITICAL();

		/* Interrupts and other tasks can send to and receive from the queue
		now the critical section has been exited. */

		vTaskSuspendAll();

		/* Update the timeout state to see if it has expired yet. */
		if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
		{
			if( prvIsMutexLocked( pxMutex ) == pdTRUE )
			{
				traceLOCK_LIGHT_MUTEX_BLOCKED( pxMutex );
				taskENTER_CRITICAL();
				{
					vTaskPriorityInherit( pxMutex->pxMutexHolder );
				}
				taskEXIT_CRITICAL();

				vTaskPlaceOnEventList( &( pxMutex->xTasksWaitingToLock ), xTicksToWait );
				if( xTaskResumeAll() == pdFALSE )
				{
					portYIELD_WITHIN_API();
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				/* Try again. */
				( void ) xTaskResumeAll();
			}
		}
		else
		{
			( void ) xTaskResumeAll();
			traceLOCK_LIGHT_MUTEX_FAILED( pxMutex );
			return errQUEUE_EMPTY;
		}
	}
}
/*-----------------------------------------------------------*/

UBaseType_t xLightMutexIsTaskWaiting( LightMutexHandle_t xMutex, void *task_handle)
{
  LightMutex_t *pxMutex = ( LightMutex_t * ) xMutex;

  taskENTER_CRITICAL();

  ListItem_t const *pxListItem, *pxListEnd;
  List_t *pxList;
  pxList = &pxMutex->xTasksWaitingToLock;
  UBaseType_t is_waiting = 0;

  pxListItem = listGET_HEAD_ENTRY( pxList );
  pxListEnd = listGET_END_MARKER( pxList );

  while (pxListItem != pxListEnd) {
    if (listGET_LIST_ITEM_OWNER(pxListItem) == (TaskHandle_t)task_handle) {
      is_waiting = 1;
      break;
    }
    pxListItem = listGET_NEXT( pxListItem );
  }

  taskEXIT_CRITICAL();
  return (is_waiting);
}

/*-----------------------------------------------------------*/

#if ( INCLUDE_xSemaphoreGetMutexHolder == 1 )

void* xLightMutexGetHolder( LightMutexHandle_t xMutex )
{
	void *pxReturn;

	/* Note:  This is a good way of determining if the
	calling task is the mutex holder, but not a good way of determining the
	identity of the mutex holder, as the holder may change between the
	following critical section exiting and the function returning. */
	taskENTER_CRITICAL();
	{
		pxReturn = ( void * ) ( ( LightMutex_t * ) xMutex )->pxMutexHolder;
	}
	taskEXIT_CRITICAL();

	return pxReturn;
}

#endif
/*-----------------------------------------------------------*/

#if ( configUSE_RECURSIVE_LIGHT_MUTEXES == 1 )

UBaseType_t uxLightMutexGetRecursiveCallCount( LightMutexHandle_t xMutex ) {
	LightMutex_t * const pxMutex = ( LightMutex_t * ) xMutex;

    configASSERT_SAFE_TO_CALL_FREERTOS_API();
    configASSERT( pxMutex );

    // only the thread which owns the lock will call this routine so we
    // don't need a critical section
    const UBaseType_t uxCount = pxMutex->uxRecursiveCallCount;

    return uxCount;
  }

BaseType_t xLightMutexUnlockRecursive( LightMutexHandle_t xMutex ) {
	BaseType_t xReturn;
	LightMutex_t * const pxMutex = ( LightMutex_t * ) xMutex;

	configASSERT_SAFE_TO_CALL_FREERTOS_API();
	configASSERT( pxMutex );

	/* If this is the task that holds the mutex then pxMutexHolder will not
	change outside of this task.  If this task does not hold the mutex then
	pxMutexHolder can never coincidentally equal the tasks handle, and as
	this is the only condition we are interested in it does not matter if
	pxMutexHolder is accessed simultaneously by another task.  Therefore no
	mutual exclusion is required to test the pxMutexHolder variable. */
	if( pxMutex->pxMutexHolder == xTaskGetCurrentTaskHandle() )
	{
		traceUNLOCK_LIGHT_MUTEX_RECURSIVE( pxMutex );

		/* uxRecursiveCallCount cannot be zero if pxMutexHolder is equal to
		the task handle, therefore no underflow check is required.  Also,
		uxRecursiveCallCount is only modified by the mutex holder, and as
		there can only be one, no mutual exclusion is required to modify the
		uxRecursiveCallCount member. */
		( pxMutex->uxRecursiveCallCount )--;

		/* Have we unwound the call count? */
		if( pxMutex->uxRecursiveCallCount == ( UBaseType_t ) 0 )
		{
			/* Return the mutex.  This will automatically unblock any other
			task that might be waiting to access the mutex. */
			( void ) xLightMutexUnlock( pxMutex );
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		xReturn = pdPASS;
	}
	else
	{
		/* The mutex cannot be given because the calling task is not the holder. */
		xReturn = pdFAIL;

		traceUNLOCK_LIGHT_MUTEX_RECURSIVE_FAILED( pxMutex );
	}

	return xReturn;
}

BaseType_t xLightMutexLockRecursive( LightMutexHandle_t xMutex, TickType_t xTicksToWait ) {
	BaseType_t xReturn;
	LightMutex_t * const pxMutex = ( LightMutex_t * ) xMutex;

		configASSERT_SAFE_TO_CALL_FREERTOS_API();
		configASSERT( pxMutex );

		/* Comments regarding mutual exclusion as per those within
		xMutexUnlockRecursive(). */

		traceLOCK_LIGHT_MUTEX_RECURSIVE( pxMutex );

		if( pxMutex->pxMutexHolder == xTaskGetCurrentTaskHandle() )
		{
			( pxMutex->uxRecursiveCallCount )++;
			xReturn = pdPASS;
		}
		else
		{
			xReturn = xLightMutexLock( pxMutex, xTicksToWait );

			/* pdPASS will only be returned if the mutex was successfully
			obtained. The calling task may have entered the Blocked state
			before reaching here. */
			if( xReturn == pdPASS )
			{
				( pxMutex->uxRecursiveCallCount )++;
			}
			else
			{
				traceLOCK_LIGHT_MUTEX_RECURSIVE_FAILED( pxMutex );
			}
		}

		return xReturn;
	}

#endif /* configUSE_RECURSIVE_LIGHT_MUTEXES */
/*-----------------------------------------------------------*/

void vLightMutexDelete( LightMutexHandle_t xMutex )
{
	LightMutex_t * const pxMutex = ( LightMutex_t * ) xMutex;

	configASSERT( pxMutex );
	vPortFree( pxMutex );
}
/*-----------------------------------------------------------*/

static BaseType_t prvMutexSetUnlocked( LightMutex_t * const pxMutex )
{
	BaseType_t xReturn = pdFALSE;

	/* The mutex is no longer being held. Reset the priority of the mutex holder. */
	xReturn = xTaskPriorityDisinherit( pxMutex->pxMutexHolder );
	pxMutex->pxMutexHolder = NULL;

	pxMutex->uxLocked = pdFALSE;

	return xReturn;
}

static BaseType_t prvIsMutexLocked( const LightMutex_t *pxMutex ) {
	BaseType_t xReturn;

	taskENTER_CRITICAL();
	{
		if( pxMutex->uxLocked == pdTRUE )
		{
			xReturn = pdTRUE;
		}
		else
		{
			xReturn = pdFALSE;
		}
	}
	taskEXIT_CRITICAL();

	return xReturn;
}

#endif /* configUSE_LIGHT_MUTEXES == 0 */
