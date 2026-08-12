/*
 ## Cypress FX3 Core Library Header (cyu3tx.h)
 ## ===========================
 ##
 ##  Copyright Cypress Semiconductor Corporation, 2010-2023,
 ##  All Rights Reserved
 ##  UNPUBLISHED, LICENSED SOFTWARE.
 ##
 ##  CONFIDENTIAL AND PROPRIETARY INFORMATION
 ##  WHICH IS THE PROPERTY OF CYPRESS.
 ##
 ##  Use of this file is governed
 ##  by the license agreement included in the file
 ##
 ##     <install>/license/license.txt
 ##
 ##  where <install> is the Cypress software
 ##  installation root directory path.
 ##
 ## ===========================
 */

#ifndef _INCLUDED_CYU3P_TX_H_
#define _INCLUDED_CYU3P_TX_H_

#ifdef CYU3P_PROFILE_EN

/* These macros need to be defined before including tx_api.h to ensure that
   the ThreadX structures are created with the right amount of memory.
 */
#define TX_EVENT_FLAGS_ENABLE_PERFORMANCE_INFO
#define TX_MUTEX_ENABLE_PERFORMANCE_INFO
#define TX_QUEUE_ENABLE_PERFORMANCE_INFO
#define TX_THREAD_ENABLE_PERFORMANCE_INFO
#define TX_TIMER_ENABLE_PERFORMANCE_INFO

#endif /* CYU3P_PROFILE_EN */

#include "cyu3types.h"
#include "tx_api.h"
#include "cyu3externcstart.h"

/** \file cyu3tx.h
    \brief This file maps the various FX3 RTOS wrapper functions and data types
    to their base ThreadX implementations. Please refer to the ThreadX RTOS
    documentation (http://rtos.com/products/threadx) for more details of the
    base OS services.

    The FX3 SDK uses version 5.1 of the ThreadX OS with some minor updates to
    the OS start-up code. These changes were made in order to port the OS to the
    FX3 device and application framework.
 */

/** \cond THREADX_DOC
 */

/**************************************************************************
 ******************************* Macros ***********************************
 **************************************************************************/

#define CYU3P_NO_WAIT                      TX_NO_WAIT
#define CYU3P_WAIT_FOREVER                 TX_WAIT_FOREVER
#define CYU3P_EVENT_AND                    TX_AND
#define CYU3P_EVENT_AND_CLEAR              TX_AND_CLEAR
#define CYU3P_EVENT_OR                     TX_OR
#define CYU3P_EVENT_OR_CLEAR               TX_OR_CLEAR
#define CYU3P_NO_TIME_SLICE                TX_NO_TIME_SLICE
#define CYU3P_AUTO_START                   TX_AUTO_START
#define CYU3P_DONT_START                   TX_DONT_START
#define CYU3P_AUTO_ACTIVATE                TX_AUTO_ACTIVATE
#define CYU3P_NO_ACTIVATE                  TX_NO_ACTIVATE
#define CYU3P_LOOP_FOREVER                 TX_LOOP_FOREVER
#define CYU3P_INHERIT                      TX_INHERIT
#define CYU3P_NO_INHERIT                   TX_NO_INHERIT

/**************************************************************************
 ******************************* Data Types *******************************
 **************************************************************************/

#define CyU3PBytePool    TX_BYTE_POOL
#define CyU3PBlockPool   TX_BLOCK_POOL
#define CyU3PThread      TX_THREAD
#define CyU3PQueue       TX_QUEUE
#define CyU3PMutex       TX_MUTEX
#define CyU3PSemaphore   TX_SEMAPHORE
#define CyU3PEvent       TX_EVENT_FLAGS_GROUP
#define CyU3PTimer       TX_TIMER

/**************************************************************************
 *************************** Function prototypes **************************
 **************************************************************************/

/**************************** Kernel functions ****************************/

/* The following are the forward declarations as they are not defined in the
 * tx_api.h.
 */

/* Summary
   This function saves the thread context when entering the IRQ context.

   Description
   This shall be invoked by the IRQ handlers as the first call. This saves
   the previous thread state before handling the IRQ.
  
   Return Value
   None.
  
   See Also
   _tx_thread_vectored_context_save
   _tx_thread_context_restore
   _tx_fiq_context_save
   _tx_fiq_context_restore
 */
extern void
_tx_thread_context_save (
        void);

/* Summary
   This function saves the thread context when entering the IRQ context.
   This function should be used if the device uses a VIC and has vectored
   interrupt handlers.
  
   Description
   This shall be invoked by the IRQ handlers as the first call. This saves the 
   previous thread state before handling the IRQ in case of vectored interrupts.
  
   Return Value
   None.
  
   See Also
   _tx_thread_context_save
   _tx_thread_context_restore
   _tx_fiq_context_save
   _tx_fiq_context_restore
 */
extern void
_tx_thread_vectored_context_save (
        void);

/* Summary
   This function restores the thread context after handling an IRQ interrupt.
   This function shall be used for devices with and without VIC.
  
   Description
   This shall be invoked by the IRQ handlers as the last call. This is a non
   returning call. This restores new thread state according to the pre-emption
   sheduling mechanism of the RTOS.
   
   Return Value
   None.
  
   See Also
   _tx_thread_context_save
   _tx_thread_vectored_context_save
   _tx_fiq_context_save
   _tx_fiq_context_restore
 */
extern void
_tx_thread_context_restore (
        void);

/* Summary
   This function saves the thread context when entering the FIQ context.
   This function shall be used for devices with and without VIC.
  
   Description
   This shall be invoked by the FIQ handlers as the first call. This saves the 
   previous thread state before handling the FIQ.
   
   Return Value
   None.
  
   See Also
   _tx_thread_context_save
   _tx_thread_vectored_context_save
   _tx_thread_context_restore
   _tx_fiq_context_restore
 */
extern void
_tx_fiq_context_save (
        void);

/* Summary
   This function restores the thread context after handling an FIQ interrupt.
   This function shall be used for devices with and without VIC.
  
   Description
   This shall be invoked by the FIQ handlers as the last call.
   This is a non returning call. This restores new thread state according to the
   pre-emption sheduling mechanism of the RTOS.
   
   Return Value
   None.
  
   See Also
   _tx_thread_context_save
   _tx_thread_vectored_context_save
   _tx_thread_context_restore
   _tx_fiq_context_save
 */
extern void
_tx_fiq_context_restore (
        void);

/* Summary
   This function is the OS timer interrupt handler.
  
   Description
   This function does the thread scheduling based on the timer ticks.
   The function will be invoked during the RTOS timer interrupt. This function call
   shall be made from inside the API library.
   
   Return Value
   None.
  
   See Also
   _tx_thread_context_save
   _tx_thread_vectored_context_save
   _tx_thread_context_restore
   _tx_fiq_context_save
 */
extern void
_tx_timer_interrupt (
        void);

#define CyU3PKernelEntry                 tx_kernel_enter

/**************************** Memory functions ****************************/

/* 
 * The dynamic memory allocation needs to be down from a byte pool.
 * This shall be defined in the tx_api.a outside of the ThreadX kernel.
 */

#define CyU3PBytePoolCreate(pool_ptr,pool_start,pool_size) \
    tx_byte_pool_create(pool_ptr,0,pool_start,pool_size)
#define CyU3PBytePoolDestroy tx_byte_pool_delete
#define CyU3PByteAlloc tx_byte_allocate
#define CyU3PByteFree tx_byte_release

#define CyU3PBlockPoolCreate(pool_ptr,block_size,pool_start,pool_size) \
    tx_block_pool_create(pool_ptr,0,block_size,pool_start,pool_size)
#define CyU3PBlockPoolDestroy tx_block_pool_delete
#define CyU3PBlockAlloc tx_block_allocate
#define CyU3PBlockFree tx_block_release

/**************************** Thread functions ****************************/

#define CyU3PThreadCreate(ptr,name,fn,input,stack_start,stack_size,prio,thres,slice,start) \
    tx_thread_create(ptr,name,(VOID(*)(ULONG))(fn),input,stack_start,stack_size,prio,thres,slice,start)
#define CyU3PThreadDestroy tx_thread_delete
#define CyU3PThreadInfoGet(thread_ptr,name,priority,preemption_threshold,time_slice) \
    tx_thread_info_get(thread_ptr,(CHAR **)name,NULL,NULL,(UINT *)priority,\
            (UINT *)preemption_threshold,(ULONG *)time_slice,NULL,NULL)
#define CyU3PThreadIdentify tx_thread_identify
#define CyU3PThreadPriorityChange tx_thread_priority_change
#define CyU3PThreadRelinquish tx_thread_relinquish
#define CyU3PThreadResume tx_thread_resume 
#define CyU3PThreadSleep tx_thread_sleep
#define CyU3PThreadSuspend tx_thread_suspend
#define CyU3PThreadWaitAbort tx_thread_wait_abort
#define CyU3PThreadStackErrorNotify tx_thread_stack_error_notify

/***************************** Queue functions ****************************/

#define CyU3PQueueCreate(queue_ptr,message_size,queue_start,queue_size) \
    tx_queue_create(queue_ptr,0,message_size,queue_start,queue_size)
#define CyU3PQueueDestroy tx_queue_delete
#define CyU3PQueueFlush tx_queue_flush
#define CyU3PQueueSend tx_queue_send
#define CyU3PQueuePrioritySend tx_queue_front_send
#define CyU3PQueueReceive tx_queue_receive

/***************************** Mutex functions ****************************/

#define CyU3PMutexCreate(mutex_ptr,priority_inherit) \
    tx_mutex_create(mutex_ptr,0,priority_inherit)
#define CyU3PMutexDestroy tx_mutex_delete
#define CyU3PMutexGet tx_mutex_get
#define CyU3PMutexPut tx_mutex_put

/*************************** Semaphore functions **************************/

#define CyU3PSemaphoreCreate(semaphore_ptr,initial_count) \
    tx_semaphore_create(semaphore_ptr,0,initial_count)
#define CyU3PSemaphoreDestroy tx_semaphore_delete
#define CyU3PSemaphoreGet tx_semaphore_get
#define CyU3PSemaphorePut tx_semaphore_put

/***************************** Event functions ****************************/

#define CyU3PEventCreate(event_ptr) \
    tx_event_flags_create(event_ptr,0)
#define CyU3PEventDestroy tx_event_flags_delete
#define CyU3PEventGet(group_ptr,requested_flag,get_option,actual_flags_ptr,wait_option)\
    tx_event_flags_get(group_ptr,requested_flag,get_option,(ULONG *)actual_flags_ptr,wait_option)
#define CyU3PEventSet tx_event_flags_set

/*************************** Interrupt functions **************************/

#define CyU3PIrqContextSave             _tx_thread_context_save
#define CyU3PIrqVectoredContextSave     _tx_thread_vectored_context_save
#define CyU3PIrqContextRestore          _tx_thread_context_restore
#define CyU3PFiqContextSave             _tx_fiq_context_save
#define CyU3PFiqContextRestore          _tx_fiq_context_restore
#define CyU3PIrqNestingStart            _tx_thread_irq_nesting_start
#define CyU3PIrqNestingStop             _tx_thread_irq_nesting_end
#define CyU3POsTimerHandler             _tx_timer_interrupt

/***************************** Timer functions ****************************/

#define CyU3PTimerCreate(timer_ptr,expiration_function,expiration_input,\
        initial_tick,reschedule_ticks,timer_option) \
        tx_timer_create(timer_ptr,0,(VOID(*)(ULONG))(expiration_function),\
                expiration_input,initial_tick,reschedule_ticks,timer_option)
#define CyU3PTimerDestroy tx_timer_delete
#define CyU3PTimerStart tx_timer_activate
#define CyU3PTimerStop tx_timer_deactivate
#define CyU3PTimerModify tx_timer_change
#define CyU3PGetTime tx_time_get
#define CyU3PSetTime tx_time_set

/*************************** Profiling functions **************************/
#define CyU3PThreadSystemPerfGet(a,b,c,d,e,f,g,h,i)     \
    _tx_thread_performance_system_info_get((a), (b), (c), (d), (e), 0, (f), (g), 0, (h), (i))

#define CyU3PThreadPerfGet(t,a,b,c,d,e,f,g)             \
    _tx_thread_performance_info_get ((t), (a), (b), (c), (d), (e), 0, (f), (g), 0, 0)

#define CyU3PTimerSystemPerfGet(a,b,c,d)                \
    _tx_timer_performance_system_info_get((a), (b), (c), (d), 0)

#define CyU3PTimerPerfGet(t,a,b,c,d)                    \
    _tx_timer_performance_info_get((t), (a), (b), (c), (d), 0)

#define CyU3PMutexSystemPerfGet(a,b,c,d,e,f)            \
    _tx_mutex_performance_system_info_get((a), (b), (c), (d), (e), (f))

#define CyU3PMutexPerfGet(m,a,b,c,d,e,f)                \
    _tx_mutex_performance_info_get((m), (a), (b), (c), (d), (e), (f))

#define CyU3PEventSystemPerfGet(a,b,c,d)                \
    _tx_event_flags_performance_system_info_get((a), (b), (c), (d))

#define CyU3PEventPerfGet(e,a,b,c,d)                    \
    _tx_event_flags_performance_info_get((e), (a), (b), (c), (d))

#define CyU3PQueueSystemPerfGet(a,b,c,d,e,f)            \
    _tx_queue_performance_system_info_get((a), (b), (c), (d), (e), (f))

#define CyU3PQueuePerfGet(q,a,b,c,d,e,f)                \
    _tx_queue_performance_info_get((q), (a), (b), (c), (d), (e), (f))

#define CyU3PThreadSetActivityGpio      tx_thread_set_profile_gpio
#define CyU3PMutexSetActivityGpio       tx_mutex_set_profile_gpio
#define CyU3PSemaphoreSetActivityGpio   tx_semaphore_set_profile_gpio
#define CyU3PEventSetActivityGpio       tx_event_flags_set_profile_gpio

/** \endcond
 */

/******************** DMA Buffer Management Functions *********************/

/** \brief DMA buffer manager for the FX3 devices.

    **Description**\n
    Firmware applications for the FX3 devices require a number of buffers
    that will be used for ingress/egress DMA data transfers. The DMA buffer
    manager is an object that allows the application to dynamically allocate
    and free DMA buffers as per need. The buffers are allocated from a region
    of memory that is specified during start-up.
    
    To ensure cache coherency when the data cache is in use, it is required
    that all buffers used for DMA operations be 32 byte aligned. This
    implementation of the DMA manager ensures this by making rounding up
    all allocations to multiples of 32 bytes.
 
    This is a simple reference implementation of a buffer manager and can be
    adapted as needed. This implementation tracks the used status of each
    32 byte memory chunk in the specified region, and uses a first-fit algorithm
    to service allocation requests. If the manager is unable to find the required
    memory, it returns an error and there is no provision for waiting until
    memory is available.
 
    Only one instance of this buffer manager can be created and used.
 */
typedef struct CyU3PDmaBufMgr_t
{
    CyU3PMutex  lock;                   /**< Mutex used for thread safe allocation. */
    uint32_t    startAddr;              /**< Start address of memory region available for allocation. */
    uint32_t    regionSize;             /**< Size of memory region available for allocation. */
    uint32_t   *usedStatus;             /**< Bit-map that stores the status of memory blocks. */
    uint32_t    statusSize;             /**< Size of the status array in 32 bit words. */
    uint32_t    searchPos;              /**< Word address from which to start searching for memory. */
} CyU3PDmaBufMgr_t;


/**************************************************************************/

#include "cyu3externcend.h"

#endif /* _INCLUDED_CYU3P_TX_H_ */

/*[]*/


