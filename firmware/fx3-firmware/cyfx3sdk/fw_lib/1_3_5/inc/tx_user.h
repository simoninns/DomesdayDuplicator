/**************************************************************************/ 
/*                                                                        */ 
/*            Copyright (c) 1996-2007 by Express Logic Inc.               */ 
/*                                                                        */ 
/*  This software is copyrighted by and is the sole property of Express   */ 
/*  Logic, Inc.  All rights, title, ownership, or other interests         */ 
/*  in the software remain the property of Express Logic, Inc.  This      */ 
/*  software may only be used in accordance with the corresponding        */ 
/*  license agreement.  Any unauthorized use, duplication, transmission,  */ 
/*  distribution, or disclosure of this software is expressly forbidden.  */ 
/*                                                                        */
/*  This Copyright notice may not be removed or modified without prior    */ 
/*  written consent of Express Logic, Inc.                                */ 
/*                                                                        */ 
/*  Express Logic, Inc. reserves the right to modify this software        */ 
/*  without notice.                                                       */ 
/*                                                                        */ 
/*  Express Logic, Inc.                     info@expresslogic.com         */
/*  11423 West Bernardo Court               http://www.expresslogic.com   */
/*  San Diego, CA  92127                                                  */
/*                                                                        */
/**************************************************************************/


/**************************************************************************/
/**************************************************************************/
/**                                                                       */ 
/** ThreadX Component                                                     */
/**                                                                       */
/**   User Specific                                                       */
/**                                                                       */
/**************************************************************************/
/**************************************************************************/


/**************************************************************************/ 
/*                                                                        */ 
/*  PORT SPECIFIC C INFORMATION                            RELEASE        */ 
/*                                                                        */ 
/*    tx_user.h                                           PORTABLE C      */ 
/*                                                           5.1          */ 
/*                                                                        */
/*  AUTHOR                                                                */ 
/*                                                                        */ 
/*    William E. Lamie, Express Logic, Inc.                               */ 
/*                                                                        */ 
/*  DESCRIPTION                                                           */ 
/*                                                                        */ 
/*    This file contains user defines for configuring ThreadX in specific */ 
/*    ways. This file will have an effect only if the application and     */ 
/*    ThreadX library are built with TX_INCLUDE_USER_DEFINE_FILE defined. */ 
/*    Note that all the defines in this file may also be made on the      */ 
/*    command line when building ThreadX library and application objects. */ 
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  12-12-2005     William E. Lamie         Initial Version 5.0           */ 
/*  04-02-2007     William E. Lamie         Modified comment(s), and      */ 
/*                                            added two new conditional   */ 
/*                                            build options, namely:      */ 
/*                                                                        */ 
/*                                                 TX_NO_TIMER  and       */ 
/*                                                 TX_USE_PRESET_DATA     */ 
/*                                                                        */ 
/*                                            resulting in version 5.1    */ 
/*                                                                        */ 
/**************************************************************************/ 

#ifndef TX_USER_H
#define TX_USER_H


/* Define various build options for the ThreadX port.  The application should either make changes
   here by commenting or un-commenting the conditional compilation defined OR supply the defines 
   though the compiler's equivalent of the -D option.  */


/* Override various options with default values already assigned in tx_port.h. Please also refer
   to tx_port.h for descriptions on each of these options.  */

/*
#define TX_MAX_PRIORITIES                       32  
#define TX_MINIMUM_STACK                        ????         
#define TX_THREAD_USER_EXTENSION                ????
#define TX_TIMER_THREAD_STACK_SIZE              ????
#define TX_TIMER_THREAD_PRIORITY                ????
*/

/* Determine if timer expirations (application timers, timeouts, and tx_thread_sleep calls 
   should be processed within the a system timer thread or directly in the timer ISR. 
   By default, the timer thread is used. When the following is defined, the timer expiration 
   processing is done directly from the timer ISR, thereby eliminating the timer thread control
   block, stack, and context switching to activate it.  */

/*
#define TX_TIMER_PROCESS_IN_ISR
*/

/* Determine if in-line timer reactivation should be used within the timer expiration processing.
   By default, this is disabled and a function call is used. When the following is defined,
   reactivating is performed in-line resulting in faster timer processing but slightly larger
   code size.  */ 

/*
#define TX_REACTIVATE_INLINE 
*/

/* Determine is stack filling is enabled. By default, ThreadX stack filling is enabled,
   which places an 0xEF pattern in each byte of each thread's stack.  This is used by
   debuggers with ThreadX-awareness and by the ThreadX run-time stack checking feature.  */

/*
#define TX_DISABLE_STACK_FILLING 
*/

/* Determine whether or not stack checking is enabled. By default, ThreadX stack checking is 
   disabled. When the following is defined, ThreadX thread stack checking is enabled.  If stack
   checking is enabled (TX_ENABLE_STACK_CHECKING is defined), the TX_DISABLE_STACK_FILLING
   define is negated, thereby forcing the stack fill which is necessary for the stack checking
   logic.  */

/*
#define TX_ENABLE_STACK_CHECKING
*/

/* Determine if preemption-threshold should be disabled. By default, preemption-threshold is 
   enabled. If the application does not use preemption-threshold, it may be disabled to reduce
   code size and improve performance.  */

/*
#define TX_DISABLE_PREEMPTION_THRESHOLD
*/

/* Determine if global ThreadX variables should be cleared. If the compiler startup code clears 
   the .bss section prior to ThreadX running, the define can be used to eliminate unnecessary
   clearing of ThreadX global variables.  */

/*
#define TX_DISABLE_REDUNDANT_CLEARING
*/

/* Determine if no timer processing is required. This option will help eliminate the timer 
   processing when not needed. The user will also have to comment out the call to 
   tx_timer_interrupt, which is typically made from assembly language in 
   tx_initialize_low_level.  */

/* 
#define TX_NO_TIMER
*/

/* Determine if preset data can be used for the thread priority bit map instead of the
   generated bit map that is created during initialization. If defined, the time required
   to initialize ThreadX is significantly reduced.  */

/* 
#define TX_USE_PRESET_DATA
*/

/* Determine if the notify callback option should be disabled. By default, notify callbacks are
   enabled. If the application does not use notify callbacks, they may be disabled to reduce
   code size and improve performance.  */

/*
#define TX_DISABLE_NOTIFY_CALLBACKS
*/

/* Determine if block pool performance gathering is required by the application. When the following is
   defined, ThreadX gathers various block pool performance information. */

/*
#define TX_BLOCK_POOL_ENABLE_PERFORMANCE_INFO
*/

/* Performance profiling is enabled when this header file is included. */

/* Determine if byte pool performance gathering is required by the application. When the following is
   defined, ThreadX gathers various byte pool performance information. */
/*
#define TX_BYTE_POOL_ENABLE_PERFORMANCE_INFO
 */

/* Determine if event flags performance gathering is required by the application. When the following is
   defined, ThreadX gathers various event flags performance information. */
#define TX_EVENT_FLAGS_ENABLE_PERFORMANCE_INFO

/* Determine if mutex performance gathering is required by the application. When the following is
   defined, ThreadX gathers various mutex performance information. */
#define TX_MUTEX_ENABLE_PERFORMANCE_INFO

/* Determine if queue performance gathering is required by the application. When the following is
   defined, ThreadX gathers various queue performance information. */
#define TX_QUEUE_ENABLE_PERFORMANCE_INFO

/* Determine if semaphore performance gathering is required by the application. When the following is
   defined, ThreadX gathers various semaphore performance information. */
/*
#define TX_SEMAPHORE_ENABLE_PERFORMANCE_INFO
 */

/* Determine if thread performance gathering is required by the application. When the following is
   defined, ThreadX gathers various thread performance information. */
#define TX_THREAD_ENABLE_PERFORMANCE_INFO

/* Determine if timer performance gathering is required by the application. When the following is
   defined, ThreadX gathers various timer performance information. */
#define TX_TIMER_ENABLE_PERFORMANCE_INFO

#endif

