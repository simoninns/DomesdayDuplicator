/*
 ## Cypress FX3 Core Library Header (cyu3os.h)
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

#ifndef _INCLUDED_CYU3P_OS_H_
#define _INCLUDED_CYU3P_OS_H_

#include "cyu3types.h"
#include "cyu3externcstart.h"

/** \file cyu3os.h
    \brief This file defines the RTOS services and wrappers that are provided
    as part of the FX3 firmware solution. Most of these services are used by
    the drivers in the FX3 library and need to be provided when porting to a
    new OS environment.

    \section RTOSConst RTOS Constants
    \brief The RTOS wrappers used by the FX3 firmware library define and make
    use of the following constants.

    <B>Description</B>\n
    The following constants are special parameter values passed to some OS
    functions to specify the desired behaviour.

    <B>CYU3P_NO_WAIT</B> : This is a special value for waitOption which indicates
    that the function should not wait / block the thread and should immediately return.
    This is used for OS functions like CyU3PMutexGet and also for various firmware
    API functions which internally wait for mutexes or events.

    <B>CYU3P_WAIT_FOREVER</B> : This is a special value for waitOption which indicates
    that the function should wait until the particular mutex or event has been flagged.
    This is used for OS functions like CyU3PMutexGet and also for various firmware
    API functions which internally wait for mutexes and events.

    <B>CYU3P_EVENT_AND</B> : This is a special value for getOption / setOption in
    the CyU3PEventGet and CyU3PEventSet functions which specifies the bit-wise operation
    to be performed on the event flag. This variable is used when the mask and the flag
    have to be ANDed without modification to the actual flags in the case of CyU3PEventGet.

    <B>CYU3P_EVENT_AND_CLEAR</B> : This is a special value for getOption in the
    CyU3PEventGet function which specifies the bit operation to be performed on the event
    flag. This option is used when the mask and the flag have to be ANDed and the flag
    cleared.

    <B>CYU3P_EVENT_OR</B> : This is a special value for the getOption / setOption in
    CyU3PEventGet and CyU3PEventSet functions which specifies the bit operation to be
    performed on the event flag. This option is used when the mask and the flag have
    to be ORed without modification to the flags.

    <B>CYU3P_EVENT_OR_CLEAR</B> : This is a special value for the getOption in CyU3PEventGet
    function which specifies the bit operation to be performed on the event flag.
    This option is used when the mask and the flag have to be ORed and the flag
    cleared.

    <B>CYU3P_NO_TIME_SLICE</B> : This is a special value for the timeSlice option in
    CyU3PThreadCreate function. The value specifies that the thread shall not be
    pre-empted based on the timeSlice value provided.

    <B>CYU3P_AUTO_START</B> : This is a special value for the autoStart option in
    CyU3PThreadCreate function. The value specifies that the thread should be
    started immediately without waiting for a CyU3PThreadResume call.

    <B>CYU3P_DONT_START</B> : This is a special value for the autoStart option in
    CyU3PThreadCreate function. The value specifies that the thread should be suspended
    on create and shall be started only after a subsequent CyU3PThreadResume call.

    <B>CYU3P_AUTO_ACTIVATE</B> : This is a special value for the timerOption parameter in
    CyU3PTimerCreate function. This value specifies that the timer shall be automatically
    started after create.

    <B>CYU3P_NO_ACTIVATE</B> : This is a special value for the timerOption parameter in
    CyU3PTimerCreate function. This value specifies that the timer shall not be activated
    on create and needs to be explicitly activated.

    <B>CYU3P_INHERIT</B> : This is a special value for the priorityInherit parameter of the
    CyU3PMutexCreate function. This value specifies that the mutex supports priority
    inheritance.

    <B>CYU3P_NO_INHERIT</B> : This is a special value for the priorityInherit parameter of the
    CyU3PMutexCreate function. This value specifies that the mutex does not support
    priority inheritance.

    \section FX3Exceptions Exceptions and Interrupts
    \brief This section describes the operating modes, exceptions and interrupts
    in the FX3 firmware.

    <B>Description</B>\n
    The FX3 firmware generally executes in the Supervisor (SVC) mode of ARM
    processors, which is a privileged mode. The User and FIQ modes are
    currently unused by the FX3 firmware. The IRQ mode is used for the initial
    part of interrupt handler execution and the System mode is used for handling
    the second halves of long interrupts in a nested manner.

    The Abort and Undefined modes are only executed when the ARM CPU encounters
    an execution error such as an undefined instruction, or a instruction/data
    access abort.

    The FX3 device has an internal PL192 Vectored Interrupt Controller which is
    used for managing all interrupts raised by the device. The drivers for various
    hardware blocks in the FX3 firmware library register interrupt handlers for
    all interrupt sources. Event callbacks are raised to the user firmware for all
    relevant interrupt conditions.

    Each interrupt source has a 4 bit priority value associated with it. These
    priority settings are unused as of now; and interrupt priority is enforced
    through nesting and pre-emption.

    The RTOS used by the FX3 firmware allows interrupts to be nested, and this
    mechanism is used to allow higher priority interrupts to pre-empt lower
    priority ones. Thus the interrupts are classified into two groups: high
    priority ones that cannot be pre-empted and low priority ones that can be
    pre-empted.
   
    The high priority (non pre-emptable interrupts) are:\n
    * USB interrupt\n
    * DMA interrupt for USB sockets\n
    * DMA interrupt for GPIF sockets\n
    * DMA interrupt for Serial peripheral sockets\n
   
    The low priority (pre-emptable interrupts) are:
    * System control interrupt (used for suspend/wakeup)\n
    * OS scheduler interrupt (timer based)\n
    * GPIF interrupt\n
    * SPI interrupt\n
    * I2C interrupt\n
    * I2S interrupt\n
    * UART interrupt\n
    * GPIO interrupt\n

    The respective interrupt handlers in the drivers are responsible for
    enabling/disabling these interrupt sources at the appropriate time.
    Since disabling one or more of these interrupts at arbitrary times
    can cause system errors and crashes; user accessible functions to
    control these interrupts individually are not provided.

    The FX3 SDK only provides APIs that can disable and re-enable all
    interrupt sources so as to ensure atomicity of critical code sections.

    \subsection Fx3ExceptionHandlers Exception Handler Functions
    \brief This section provides information on ARM Exception handlers in
    the FX3 firmware.

    <B>Description</B>\n
    Exceptions such as data or instruction abort, and undefined instruction
    may happen if the firmware image is corrupted during loading or at runtime.
    Since these are unexpected conditions, the FX3 firmware library does not
    provide any specific code to handle them. Default handlers for these
    conditions are provided in the cyfxtx.c file, and these can be modified
    by the users to match their requirements (for example, reset the FX3 device
    and restart operation).

    \section RTOSMemFunc RTOS Memory Functions
    \brief This section documents the functions that the FX3 firmware provides
    for dynamic memory management. These are implemented as wrappers on top of
    the corresponding RTOS services.

    <B>Description</B>\n
    The FX3 firmware makes use of a set of memory management services
    that are provided by the RTOS used. The firmware library also provides a set
    of high level dynamic memory management functions that are wrappers on top
    of the corresponding RTOS services.

    Two flavors of memory allocation services are provided:\n
    * Byte Pool: This is a generic memory pool similar to a fixed sized heap.
      Memory buffers of any size can be allocated from a byte pool.\n
    * Block pool: This is a memory pool that is optimized for handling buffers
      if a fixed size only. The block pool has lesser memory overhead per buffer
      allocated and can be used in cases where the application requires a large
      number of buffers of a specific size.\n

    The firmware library or application is not expected to use the compiler
    provided heap for memory allocation to avoid portability issues across
    different compilers and tool chains. The library provides general purpose
    memory allocation functions that can be used to replace the standard C
    library memory allocation calls.

    These functions can be implemented using the above mentioned byte pool and
    block pool functions. Implementations of these functions in source form are
    provided for reference, and can be modified as required by the application.

    \section RTOSThreadFunc Thread Functions
    \brief This section documents the thread functions that are provided as
    part of the FX3 firmware.

    <B>Description</B>\n
    The FX3 firmware provides a set of thread functions that can be used
    by the application to create and manage threads. These functions are
    wrappers around the corresponding RTOS services.

    The firmware framework itself consists of a number of threads that run
    the  drivers for various peripheral blocks on the device. It is expected
    that these driver threads will have higher priorities than any threads
    created by the firmware application.

    Co-operative scheduling is typically used in the firmware, and time slices
    are not used. This means that thread switches will only happen when the
    active thread is blocked.

    \section RTOSQueueFunc Message Queue Functions
    \brief This section documents the message queue functions that are provided
    as part of the FX3 firmware.

    <B>Description</B>\n
    The FX3 firmware makes use of message queues for inter-thread data communication.
    A set of wrapper functions on top of the corresponding OS services are provided
    to allow the use of message queues from application threads.

    \section RTOSMutexFunc Mutex functions
    \brief This section documents the mutex functions that are provided as part of
    the FX3 firmware.

    <B>Description</B>\n
    The FX3 firmware provides a set of mutex functions that can be used
    for protection of global data structures in a multi-thread environment. These
    functions are all wrappers around the corresponding OS services.

    \section RTOSSemaFunc Semaphore Functions
    \brief This section documents the semaphore functions supported by the FX3
    firmware.

    <B>Description</B>\n
    In addition to mutexes used for ownership control of shared data and mutual
    exclusion, the FX3 firmware also provides counting semaphores that can be
    used for synchronization and signaling. Each semaphore is created with an
    initial count, and the count is decremented on each successful get operation.
    The get operation is failed when the count reaches zero. Each put operation
    on the semaphore increments the associated count.

    \section RTOSEventFunc Event Flag Functions
    \brief This section documents the event flag functions provided by the
    FX3 firmware.

    <B>Description</B>\n
    Event flags are an advanced means for inter-thread synchronization that
    is provided as part of the FX3 firmware. These functions are
    wrappers around the corresponding functionality provided by the RTOS.

    Event flag groups are groups of single bit flags or signals that can
    be used for inter-thread communication. Each flag in a event group can
    be individually signaled or waited upon by any given thread. It is
    possible for multiple threads to wait on different flags that are
    implemented by a single event group. This facility makes event flags
    a memory efficient means for inter-thread synchronization.

    Event flags are frequently used for synchronization between various
    driver threads in the FX3 device.

    \section RTOSTimerFunc Timer Functions
    \brief This section documents the application timer functions that are provided
    by the FX3 firmware.

    <B>Description</B>\n
    Application timers are OS services that support the implementation of
    periodic tasks in the firmware application. Any number of application timers
    (subject to memory constraints) can be created by the application and the
    time intervals specified should be multiples of the OS timer ticks.

    The OS keeps track of the registered application timers and calls the
    application provided callback functions every time the corresponding
    interval has elapsed.
 */

/**************************************************************************
 ******************************* Macros ***********************************
 **************************************************************************/

/**************************************************************************
 ******************************* Data Types *******************************
 **************************************************************************/

/** \brief Byte pool structure.

    <B>Description</B>\n
    A byte pool is a form of heap that supports allocation of arbitrarily sized
    memory blocks from a memory region specified at creation time. The total memory
    used by the byte pool is fixed at creation time, and allocation will fail
    once all of the available memory is used up.

    **\see
    *\see CyU3PBytePoolCreate
    *\see CyU3PBytePoolDestroy
    *\see CyU3PByteAlloc
    *\see CyU3PByteFree
 */
typedef struct CyU3PBytePool     CyU3PBytePool;

/** \brief Block pool structure.

    <B>Description</B>\n
    A block pool is a form of heap (memory allocator) which allocated memory blocks
    of a fixed size selected during pool creation. This structure is more memory
    efficient than the more generic byte pool.

    Though the block pool definition and services are part of the API library,
    the FX3 driver code does not make internal references to these services.

    **\see
    *\see CyU3PBlockPoolCreate
    *\see CyU3PBlockPoolDestroy
    *\see CyU3PBlockAlloc
    *\see CyU3PBlockFree
 */
typedef struct CyU3PBlockPool    CyU3PBlockPool;

/** \brief Thread structure.

    <B>Description</B>\n
    This data structure is associated with each RTOS thread that is created in
    the FX3 firmware. The driver modules for various FX3 blocks make use of some
    threads internally as well.

    **\see
    *\see CyU3PThreadCreate
    *\see CyU3PThreadDestroy
    *\see CyU3PThreadIdentify
    *\see CyU3PThreadInfoGet
    *\see CyU3PThreadSleep
    *\see CyU3PThreadSuspend
    *\see CyU3PThreadResume
    *\see CyU3PThreadWaitAbort
    *\see CyU3PThreadRelinquish
    *\see CyU3PThreadPriorityChange
    *\see CyU3PThreadStackErrorNotify
 */
typedef struct CyU3PThread       CyU3PThread;

/** \brief Thread entry function type.

    <B>Description</B>\n
    This type represents the entry function for an RTOS thread created by a
    FX3 firmware application. The threadArg argument is a context input that
    is specified by the user when creating the thread.

    **\see
    *\see CyU3PThread
    *\see CyU3PThreadCreate
 */
typedef void (*CyU3PThreadEntry_t) (
        uint32_t threadArg              /**< User specified context argument. */
        );

/** \brief Message queue structure.

    <B>Description</B>\n
    Message queues are used for inter-thread communication in FX3 firmware. They
    are also used by the interrupt handlers for various FX3 blocks to schedule work
    for the driver threads to handle. This structure is associated with each message
    queue that is used in the firmware application.

    **\see
    *\see CyU3PQueueCreate
    *\see CyU3PQueueDestroy
    *\see CyU3PQueueSend
    *\see CyU3PQueuePrioritySend
    *\see CyU3PQueueReceive
    *\see CyU3PQueueFlush
 */
typedef struct CyU3PQueue        CyU3PQueue;

/** \brief Mutex structure.

    <B>Description</B>\n
    This structure is associated with each Mutex object that is used in the FX3
    firmware application.

    **\see
    *\see CyU3PMutexCreate
    *\see CyU3PMutexDestroy
    *\see CyU3PMutexPut
    *\see CyU3PMutexGet
 */
typedef struct CyU3PMutex        CyU3PMutex;

/** \brief Semaphore structure.

    <B>Description</B>\n
    The semaphore service in the FX3 OS Library is a typical counting semaphore
    implementation. This structure is associated with each semaphore object that
    is used in the firmware application.

    **\see
    *\see CyU3PSemaphoreCreate
    *\see CyU3PSemaphoreDestroy
    *\see CyU3PSemaphoreGet
    *\see CyU3PSemaphorePut
 */
typedef struct CyU3PSemaphore    CyU3PSemaphore;

/** \brief Event group structure.

    <B>Description</B>\n
    An event group is a set of event flags that an OS thread can wait for, and other
    threads or interrupts can notify. Each event group provides a maximum of 32 separate
    event flags that can be used independently. This structure is associated with each
    event group used in the FX3 firmware application.

    **\see
    *\see CyU3PEventCreate
    *\see CyU3PEventDestroy
    *\see CyU3PEventSet
    *\see CyU3PEventGet
 */
typedef struct CyU3PEvent        CyU3PEvent;

/** \brief Timer structure.

    <B>Description</B>\n
    OS timers provide a mechanism for setting up periodic tasks which will be triggered
    once every period number of timer ticks. This structure is associated with each
    OS timer object used in the FX3 firmware application.

    **\see
    *\see CyU3PTimerCreate
    *\see CyU3PTimerDestroy
    *\see CyU3PTimerStart
    *\see CyU3PTimerStop
    *\see CyU3PTimerModify
 */
typedef struct CyU3PTimer        CyU3PTimer;

/** \brief Type of callback function called on timer expiration.

    <B>Description</B>\n
    This type defines the signature of the handler callback for an OS timer object.
    A function of this type is associated with each timer at the time of creation
    and is called by the OS scheduler on a periodic basis. The timerArg parameter
    passed to this function is also specified at the time of creating the timer
    object.

    **\see
    *\see CyU3PTimer
    *\see CyU3PTimerCreate
 */
typedef void (*CyU3PTimerCb_t) (
        uint32_t timerArg               /**< User specified context argument. */
        );

/**************************************************************************
 *************************** Function prototypes **************************
 **************************************************************************/

/*************************** Exception vectors ****************************/

/** \brief The undefined instruction exception handler.

    <B>Description</B>\n
    This function gets invoked when the ARM CPU encounters an undefined
    instruction. This happens when the firmware loses control and jumps to
    unknown locations. This should not occur as a part of normal operation
    sequence, and may be seen in case of memory corruption.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PPrefetchHandler
    *\see CyU3PAbortHandler
 */
extern void
CyU3PUndefinedHandler (
        void);

/** \brief The pre-fetch error exception handler.

    <B>Description</B>\n
    This function gets invoked when the ARM CPU encounters an instruction pre-fetch
    error. Since virtual memory is not used in the system, this can only happen if
    the device is trying to fetch instructions from non-existent memory.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PUndefinedHandler
    *\see CyU3PAbortHandler
 */
extern void
CyU3PPrefetchHandler (
        void);

/** \brief The abort error exception handler.

    <B>Description</B>\n
    This function gets invoked when the ARM CPU encounters an data pre-fetch
    abort error.  As virtual memory is not used in the system, this can only
    happen if data is being read from a non-existent memory location.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PUndefinedHandler
    *\see CyU3PPrefetchHandler
 */
extern void
CyU3PAbortHandler (
        void);

/**************************** Kernel functions ****************************/

/** \brief RTOS kernel entry function.

    <B>Description</B>\n
    The function call is expected to initialize the RTOS. This function
    needs to be invoked as the last function from the main () function.
    It is expected that the firmware shall always function in the SVC mode.

    <B>Return value</B>\n
    * None - The function does not return at all.

    **\see
    *\see CyU3PApplicationDefine
 */
extern void
CyU3PKernelEntry (
        void);

/** \brief The driver specific OS primitives and threads shall be created in
    this function.

    <B>Description</B>\n
    This function shall be implemented by the firmware library and needs to
    be invoked from the kernel during initialization. This is where all the
    threads and OS primitives for all module drivers shall be created. Though
    some OS calls can be safely made at this point; scheduling, wait options
    and sleep functions are not expected to be functional at this point.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PKernelEntry
 */
extern void
CyU3PApplicationDefine (
        void);

/** \brief Initialize the OS scheduler timer.

    <B>Description</B>\n
    This function is implemented by the firmware library and is invoked
    from the RTOS kernel during initialization.

    The OS_TIMER_TICK shall be defined by this function and all wait
    durations are calculated based on this. By default, the OS timer
    tick is configured to be 1ms. It is recommended that this default tick
    duration be retained to guarantee fast and accurate system response.

    If the timer tick needs to be modified, this API can be invoked only after
    the RTOS has been initialized. This function can support a timer tick range
    from 1ms to 1000ms. Any other value of the timer tick will be discarded and
    the timer tick will be set to 1ms.
 
    The clock for this timer is derived from the 32KHz standby clock. The actual
    tick interval can have an error of upto +/- 4%. The time interval will be
    accurate only as long as the interrupts are running.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3POsTimerInit (
        uint16_t intervalMs     /**< OS Timer tick interval in millisecond. */
        );

/** \brief The OS scheduler.

    <B>Description</B>\n
    This function is implemented by the RTOS kernel and is called from the OS timer
    interrupt. This function call notifies the kernel scheduler that another time
    tick has elapsed.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3POsTimerHandler (
        void);

/** \brief This function saves the active thread context when entering the IRQ
    context.

    <B>Description</B>\n
    This function is provided as part of the OS to allow any thread state to be
    backed up on entry to a IRQ handler routine. This function is only applicable
    when the VIC is not being used, and is generally not used in FX3 firmware.

    See the CyU3PIrqVectoredContextSave function for the equivalent function used
    in FX3 firmware.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PIrqContextRestore
    *\see CyU3PIrqVectoredContextSave
    *\see CyU3PFiqContextSave
 */
extern void
CyU3PIrqContextSave (
        void);

/** \brief This function saves the active thread context when entering the IRQ
    context.

    <B>Description</B>\n
    This function is provided as part of the OS to allow any thread state to be
    backed up on entry to a IRQ handler routine. This function is applicable when
    the device in use has a Vectored Interrupt Controller (VIC) and is used by
    all interrupt handlers in the FX3 firmware.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PIrqContextRestore
    *\see CyU3PIrqContextSave
    *\see CyU3PFiqContextSave
 */
extern void
CyU3PIrqVectoredContextSave (
        void);

/** \brief This function restores the thread context after handling an interrupt.

    <B>Description</B>\n
    This function is provided as part of the OS and allows the previously backed
    up thread state to be restored at the end of a IRQ handler call. The same
    function is used to restore thread state saved by the CyU3PIrqContextSave and
    CyU3PIrqVectoredContextSave functions.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PIrqContextSave
    *\see CyU3PIrqVectoredContextSave
 */
extern void
CyU3PIrqContextRestore (
        void);

/** \brief This function saves the active thread context when entering the FIQ context.

    <B>Description</B>\n
    This function is provided as part of the OS and saves thread state when entering
    a Fast Interrupt (FIQ) handler routine. As the FX3 firmware library currently
    does not use a FIQ, this function is not used.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PFiqContextRestore
 */
extern void
CyU3PFiqContextSave (
        void);

/** \brief This function restores the thread context after handling an FIQ interrupt.

    <B>Description</B>\n
    This function is provided as part of the OS and restores thread state saved at the
    beginning of the FIQ handler using the CyU3PFiqContextSave API.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PFiqContextSave
 */
extern void
CyU3PFiqContextRestore (
        void);

/** \brief This function switches the ARM mode from IRQ to SYS to allow nesting
    of interrupts.

    <B>Description</B>\n
    Nesting of interrupts on an ARMv5 controller requires that the current
    interrupt handler switch the ARM execution mode to SYS mode. This function
    is used to do this switching. In case interrupt nesting is not required
    for this platform, this function can be a No-Operation.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PIrqNestingStop
 */
extern void
CyU3PIrqNestingStart (
        void);

/** \brief This function switches the ARM mode from SYS to IRQ at the end of a
    interrupt handler.

    <B>Description</B>\n
    Nesting of interrupts on an ARMv5 controller requires that the ARM execution
    mode be switched to SYS mode at the beginning of the handler and back to IRQ
    mode at the end. This function is used to do the switch the mode back to IRQ
    at the end of an interrupt handler. In case interrupt nesting is not required
    for this platform, this function can be a No-operation.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PIrqNestingStart
 */
extern void
CyU3PIrqNestingStop (
        void);

/**************************** Memory functions ****************************/

/** \brief Initialize the custom heap manager for OS specific allocation calls

    <B>Description</B>\n
    This function creates a memory pool that can be used in place of the system
    heap. All dynamic memory allocation for OS data structures, thread stacks, and
    firmware data buffers should be allocated out of this memory pool. The size and
    location of this memory pool needs to be adjusted as per the user requirements
    by modifying this function. This function is called internally by the FX3
    library, and should not be called directly by the user code.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PMemAlloc
    *\see CyU3PMemFree
    *\see CyU3PFreeHeaps
 */
extern void
CyU3PMemInit (
        void);

/** \brief Allocate memory from the dynamic memory pool.

    <B>Description</B>\n
    This function is the malloc equivalent for allocating memory from the pool
    created by the CyU3PMemInit function. This function needs to be implemented
    as part of the RTOS porting to the FX3 device. This function needs to be
    capable of allocating memory even if called from an interrupt handler.

    <B>Return value</B>\n
    * Pointer to the allocated buffer, or NULL in case of failure.

    **\see
    *\see CyU3PMemInit
    *\see CyU3PMemFree
 */
extern void *
CyU3PMemAlloc (
        uint32_t size                   /**< The size in bytes of the buffer to be allocated. */
        );

/** \brief Free memory allocated from the dynamic memory pool.

    <B>Description</B>\n
    This function is the free equivalent that frees memory allocated through
    the CyU3PMemAlloc function. This function is also expected to be able to
    free memory in an interrupt context.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PMemInit
    *\see CyU3PMemAlloc
 */
extern void
CyU3PMemFree (
         void *mem_p                    /**< Pointer to memory buffer to be freed. */
         );

/** \brief Free up the custom heap manager and the buffer allocator.

    <B>Description</B>\n
    This function is called in preparation to a reset of the FX3 device and is
    intended to allow the user to free up the custom heap manager and the buffer
    allocator that are created in the CyU3PMemInit and the CyU3PDmaBufferInit
    functions respectively.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PMemInit
    *\see CyU3PDmaBufferInit
 */
extern void
CyU3PFreeHeaps (
        void);

/** \brief Initialize the DMA buffer allocator.

    <B>Description</B>\n
    This function creates an allocator that provides the DMA buffers required
    for data transfers through the FX3 device. The DMA buffers are also allocated
    from a pre-assigned memory region within the FX3 RAM.
   
    This allocator needs to ensure that all buffers managed by it are aligned to data
    cache lines. i.e., That all buffers start at addresses that are multiples of 32
    bytes and span a multiple of 32 bytes.

    This function is called internally by the FX3 firmware library and should not be
    called directly by the user application.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PDmaBufferDeInit
    *\see CyU3PDmaBufferAlloc
    *\see CyU3PDmaBufferFree
 */
extern void
CyU3PDmaBufferInit (
        void);

/** \brief De-initialize the buffer allocator.

    <B>Description</B>\n
    This function de-initializes the DMA buffer manager. The function shall be 
    invoked on DMA module de-init. This is provided in source format so that it
    can be modified as per user requirement. This function must not be called
    directly by the user application.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PDmaBufferInit
 */
extern void
CyU3PDmaBufferDeInit (
        void);

/** \brief Allocate the required amount of buffer memory.

    <B>Description</B>\n
    This function allocates buffers of the required size from the memory region
    reserved through the CyU3PDmaBufferInit function. It is called by the DMA
    APIs when creating DMA channels, and can also be used directly by the user
    application code.

    This implementation is expected to ensure that the buffers allocated are
    aligned to data cache lines.

    <B>Return value</B>\n
    * Pointer to the allocated buffer, or NULL in case of error.

    **\see
    *\see CyU3PDmaBufferInit
    *\see CyU3PDmaBufferDeInit
    *\see CyU3PDmaBufferFree
 */
extern void *
CyU3PDmaBufferAlloc (
        uint16_t size                   /**< The size in bytes of the buffer required. */
        );

/** \brief Free the previously allocated buffer area.

    <B>Description</B>\n
    Frees up a buffer previously allocated through the CyU3PDmaBufferAlloc
    function. This function is used internally by the DMA API when freeing up
    DMA channels, and can also be used directly by the user application code.

    <B>Return value</B>\n
    * 0 if the buffer is freed successfully.\n
    * Non-zero value otherwise.

    **\see
    *\see CyU3PDmaBufferInit
    *\see CyU3PDmaBufferDeInit
    *\see CyU3PDmaBufferAlloc
 */
extern int
CyU3PDmaBufferFree (
        void *buffer                    /**< Address of buffer to be freed. */
        );

/** \brief Structure representing a block of memory that has been dynamically allocated and is in use.

    <B>Description</B>\n
    This structure represents the header associated with a memory block allocated through
    the CyU3PMemAlloc and CyU3PDmaBufferAlloc functions. This header structure is used to check
    for memory leak and corruption occurences.
 */
typedef struct MemBlockInfo {
    uint32_t             alloc_id;              /**< Allocation id. */
    uint32_t             alloc_size;            /**< Actual memory size requested. */
    struct MemBlockInfo *prev_blk;              /**< Previous memory block. */
    struct MemBlockInfo *next_blk;              /**< Next memory block. */
    uint32_t             start_sig;             /**< Block start signature. */
    uint32_t             pad[3];                /**< Padding used to extend the header to 32 bytes. */
} MemBlockInfo;

/** \brief Type of callback function used for notifying user about memory corruption.

     <B>Description</B>\n
     This type defines a callback function that will be used to notify the user when
     the allocator detects memory corruption around the boundaries of a memory block
     allocated through the CyU3PMemAlloc or CyU3PDmaBufferAlloc calls.

     **\see
     *\see CyU3PMemEnableChecks
     *\see CyU3PBufEnableChecks
     *\see CyU3PMemCorruptionCheck
     *\see CyU3PBufCorruptionCheck
 */
typedef void (*CyU3PMemCorruptCallback) (
        void *mem_p                             /**< The memory buffer around which corruption is detected. */
        );

/** \brief Control memory leak and corruption checks in the CyU3PMemAlloc memory manager.

    <B>Description</B>\n
    This function enables/disables memory leak and corruption checks in the CyU3PMemAlloc memory
    manager. This has to be done before the CyU3PMemInit() function is called (in the main
    function before CyU3PKernelEntry() is called.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the checks were correctly enabled/disabled.
    * CY_U3P_ERROR_ALREADY_STARTED if the CyU3PMemInit function has already been called.

    **\see
    *\see CyU3PMemCorruptCallback
    *\see CyU3PMemGetActiveList
    *\see CyU3PMemCorruptionCheck
 */
extern CyU3PReturnStatus_t
CyU3PMemEnableChecks (
        CyBool_t                enable,         /**< Enable leak and corruption checks. */
        CyU3PMemCorruptCallback cb              /**< Callback to be used for memory corruption notification. */
        );

/** \brief Control memory leak and corruption checks in the CyU3PDmaBufferAlloc memory manager.

    <B>Description</B>\n
    This function enables/disables memory leak and corruption checks in the CyU3PDmaBufferAlloc
    memory manager. This has to be done before the CyU3PDmaBufferInit() function is called (in
    the main function before CyU3PKernelEntry() is called.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the checks were correctly enabled/disabled.
    * CY_U3P_ERROR_ALREADY_STARTED if the CyU3PDmaBufferInit function has already been called.

    **\see
    *\see CyU3PMemCorruptCallback
    *\see CyU3PBufGetActiveList
    *\see CyU3PBufCorruptionCheck
 */
extern CyU3PReturnStatus_t
CyU3PBufEnableChecks (
        CyBool_t                enable,         /**< Enable leak and corruption checks. */
        CyU3PMemCorruptCallback cb              /**< Callback to be used for memory corruption notification. */
        );

/** \brief Get memory allocation statistics.

    <B>Description</B>\n
    This function returns the total memory allocation and free counts using the
    CyU3PMemAlloc and CyU3PMemFree calls.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3PMemGetCounts (
        uint32_t *allocCnt_p,                   /**< Return pointer to be filled with allocation count. */
        uint32_t *freeCnt_p                     /**< Return pointer to be filled with free count. */
        );

/** \brief Get list of all unfreed allocations using CyU3PMemAlloc.

    <B>Description</B>\n
    Get list of all unfreed allocations that have been made using CyU3PMemAlloc.

    <B>Return value</B>\n
    * Start of the allocated memory list.
 */
extern MemBlockInfo *
CyU3PMemGetActiveList (
        void);

/** \brief Get memory allocation statistics.

    <B>Description</B>\n
    This function returns the total memory allocation and free counts using the
    CyU3PDmaBufferAlloc and CyU3PDmaBufferFree calls.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3PBufGetCounts (
        uint32_t *allocCnt_p,                   /**< Return pointer to be filled with allocation count. */
        uint32_t *freeCnt_p                     /**< Return pointer to be filled with free count. */
        );

/** \brief Get list of all unfreed allocations using CyU3PDmaBufferAlloc.

    <B>Description</B>\n
    Get list of all unfreed allocations that have been made using CyU3PDmaBufferAlloc.

    <B>Return value</B>\n
    * Start of the allocated memory list.
 */
extern MemBlockInfo *
CyU3PBufGetActiveList (
        void);

/** \brief Check for memory corruption around all buffers allocated using CyU3PMemAlloc.

    <B>Description</B>\n
    Check for memory corruption around all buffers allocated using CyU3PMemAlloc. The
    callback registered using the CyU3PMemEnableChecks() API will be called for each instance
    of memory corruption detected during the checks.

    <B>Return value</B>\n
    * CY_U3P_ERROR_FAILURE if any memory corruption is found.
    * CY_U3P_SUCCESS if no memory corruption is found.
 */
extern CyU3PReturnStatus_t
CyU3PMemCorruptionCheck (
        void);

/** \brief Check for memory corruption around all buffers allocated using CyU3PDmaBufferAlloc.

    <B>Description</B>\n
    Check for memory corruption around all buffers allocated using CyU3PDmaBufferAlloc. The
    callback registered using the CyU3PBufEnableChecks() API will be called for each instance
    of memory corruption detected during the checks.

    <B>Return value</B>\n
    * CY_U3P_ERROR_FAILURE if any memory corruption is found.
    * CY_U3P_SUCCESS if no memory corruption is found.
 */
extern CyU3PReturnStatus_t
CyU3PBufCorruptionCheck (
        void);

/** \brief Fill a region of memory with a specified value.

    <B>Description</B>\n
    This function is a memset equivalent and is used by the firmware library code.
    It can also be used by the application firmware.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3PMemSet (
        uint8_t *ptr,                   /**< Pointer to the buffer to be filled. */
        uint8_t data,                   /**< Value to fill the buffer with. */
        uint32_t count                  /**< Size of the buffer in bytes. */
        );

/** \brief Copy data from one memory location to another.

    <B>Description</B>\n
    This is a memcpy equivalent function that is provided and used by the
    firmware library. The implementation does not handle the case of
    overlapping buffers.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3PMemCopy (
        uint8_t *dest,                  /**< Pointer to destination buffer. */
        uint8_t *src,                   /**< Pointer to source buffer. */
        uint32_t count                  /**< Size of the buffer to be copied. */
        );

/** \brief Compare the contents of two memory buffers.

    <B>Description</B>\n
    This is a memcmp equivalent function that does a byte by byte
    comparison of two memory buffers.

    <B>Return value</B>\n
    * 0 if the two buffers hold the same data.\n
    * Negative if the data in s1 has a lower ASCII value than that in s2.\n
    * Positive if the data in s1 has a higher ASCII value that that in s2.
 */
extern int32_t
CyU3PMemCmp (
        const void* s1,                 /**< Pointer to first memory buffer. */
        const void* s2,                 /**< Pointer to second memory buffer. */
        uint32_t n                      /**< Size in bytes of the buffers to compare. */
        );

/** \brief This function creates and initializes a memory byte pool.

    <B>Description</B>\n
    This function is a wrapper around the byte pool service provided by the
    RTOS. It creates a fixed size general purpose heap structure that can
    be used for dynamic memory allocation.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if pool was successfully initialized.\n
    * Other RTOS specific error codes in case of failure.

    **\see
    *\see CyU3PBytePoolDestroy
    *\see CyU3PByteAlloc
    *\see CyU3PBlockFree
 */
extern uint32_t
CyU3PBytePoolCreate (
        CyU3PBytePool *pool_p,          /**< Handle to the byte pool structure. The memory for the
                                           structure is to be allocated by the caller. This function
                                           only initializes the structure with the pool information. */
        void *poolStart,                /**< Pointer to memory region provided for the byte pool. This
                                           address needs to be 16 byte aligned. */
        uint32_t poolSize               /**< Size of the memory region provided for the byte pool. This
                                           size needs to be a multiple of 16 bytes. */
        );

/** \brief De-initialize and free up a memory byte pool.

    <B>Description</B>\n
    This function cleans up the previously created byte pool pointed to by
    the pool_p structure. Memory allocated for the pool can be re-used after
    this call returns.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS on success.\n
    * Other RTOS error codes on failure.

    **\see
    *\see CyU3PBytePoolCreate
 */
extern uint32_t
CyU3PBytePoolDestroy (
        CyU3PBytePool *pool_p           /**< Handle to the byte pool to be freed up */
        );

/** \brief Allocate memory from a byte pool.

    <B>Description</B>\n
    This function is used to allocate memory buffers from a previously
    created memory byte pool. The waitOption specifies the timeout duration
    after which the memory allocation should be failed. It is permitted to use
    this function from an interrupt context as long as the waitOption is set to
    zero or CYU3P_NO_WAIT.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PBytePoolCreate
    *\see CyU3PByteFree
 */
extern uint32_t
CyU3PByteAlloc (
        CyU3PBytePool *pool_p,          /**< Handle to the byte pool to allocate memory from. */
        void **mem_p,                   /**< Output variable that points to the allocated memory buffer. */
        uint32_t memSize,               /**< Size of memory buffer required in bytes. */
        uint32_t waitOption             /**< Timeout for memory allocation operation in terms of
                                           OS timer ticks. Can be set to CYU3P_NO_WAIT or
                                           CYU3P_WAIT_FOREVER. */
        );

/** \brief Frees memory allocated by the CyU3PByteAlloc function.

    <B>Description</B>\n
    This function frees memory allocated from a byte pool using the
    CyU3PByteAlloc function. This function can also be invoked from an
    interrupt context.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PBytePoolCreate
    *\see CyU3PByteAlloc
 */
extern uint32_t
CyU3PByteFree (
        void *mem_p                     /**< Pointer to memory buffer to be freed */
        );

/** \brief Creates and initializes a memory block pool.

    <B>Description</B>\n
    This function initializes a memory block pool from which buffers of a fixed
    size can be dynamically allocated. The size of the buffer is specified as a
    parameter to this pool create function.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PBlockPoolDestroy
    *\see CyU3PBlockAlloc
    *\see CyU3PBlockFree
 */
extern uint32_t
CyU3PBlockPoolCreate (
        CyU3PBlockPool *pool_p,         /**< Handle to block pool structure. The caller needs to
                                           allocate the structure, and this function only initializes
                                           the fields of the structure. */
        uint32_t blockSize,             /**< Size of memory blocks that this pool will manage. The block
                                           size needs to be a multiple of 16 bytes. */
        void *poolStart,                /**< Pointer to memory region provided for the block pool. */
        uint32_t poolSize               /**< Size of memory region provided for the block pool. */
        );

/** \brief De-initialize a block memory pool.

    <B>Description</B>\n
    This function de-initializes a memory block pool. The memory region used
    by the block pool can be re-used after this function returns.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PBlockPoolCreate
 */
extern uint32_t
CyU3PBlockPoolDestroy (
        CyU3PBlockPool *pool_p          /**< Handle to the block pool to be destroyed. */
        );

/** \brief Allocate a memory buffer from a block pool.

    <B>Description</B>\n
    This function allocates a memory buffer from a block pool. The size of the
    buffer is fixed during the pool creation. The waitOption parameter specifies
    the timeout duration before the alloc call is failed.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PBlockPoolCreate
    *\see CyU3PBlockFree
 */
extern uint32_t
CyU3PBlockAlloc (
        CyU3PBlockPool *pool_p,         /**< Handle to the memory block pool. */
        void **block_p,                 /**< Output variable that will be filled with a pointer
                                           to the allocated buffer. */
        uint32_t waitOption             /**< Timeout duration in timer ticks. */
        );

/** \brief Frees a memory buffer allocated from a block pool.

    <B>Description</B>\n
    This function frees a memory buffer allocated through the CyU3PBlockAlloc call.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PBlockPoolCreate
    *\see CyU3PBlockAlloc
 */
extern uint32_t
CyU3PBlockFree (
        void *block_p                   /**< Pointer to buffer to be freed. */
        );

/**************************** Thread functions ****************************/

/** \brief This function creates a new thread.

    <B>Description</B>\n
    This function creates a new application thread with the specified parameters.
    This function call must be made only after the RTOS kernel has been started.

    The application threads should take only priority values from 7 to 15. The
    higher priorities (0 - 6) are reserved for the driver threads used by the
    library.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PThreadDestroy
    *\see CyU3PThreadIdentify
    *\see CyU3PThreadInfoGet
    *\see CyU3PThreadPriorityChange
    *\see CyU3PThreadRelinquish
    *\see CyU3PThreadSleep
    *\see CyU3PThreadSuspend
    *\see CyU3PThreadResume
    *\see CyU3PThreadWaitAbort
    *\see CyU3PThreadStackErrorNotify
 */
extern uint32_t
CyU3PThreadCreate (
        CyU3PThread        *thread_p,           /**< Pointer to the thread structure. Memory for this
                                                   structure has to be allocated by the caller. */
        char               *threadName,         /**< Name string to associate with the thread. All threads
                                                   should be named with the "Thread_Number:Description"
                                                   convention. */
        CyU3PThreadEntry_t  entryFn,            /**< Thread entry function. */
        uint32_t            entryInput,         /**< Parameter to be passed into the thread entry function. */
        void               *stackStart,         /**< Start address of the thread stack. */
        uint32_t            stackSize,          /**< Size of the thread stack in bytes. */
        uint32_t            priority,           /**< Priority to be assigned to this thread. */
        uint32_t            preemptThreshold,   /**< Threshold value for thread pre-emption. Only threads
                                                   with higher priorities than this value will be allowed
                                                   to pre-empt this thread. */
        uint32_t            timeSlice,          /**< Maximum execution time allowed for this thread in timer
                                                   ticks. It is recommended that time slices be disabled by
                                                   specifying CYU3P_NO_TIME_SLICE as the value. */
        uint32_t            autoStart           /**< Whether this thread should be suspended or started
                                                   immediately. Can be set to CYU3P_AUTO_START or
                                                   CYU3P_DONT_START. */
        );

/** \brief Free up and remove a thread from the RTOS scheduler.

    <B>Description</B>\n
    This function removes a previously created thread from the scheduler, and
    frees up the resources associated with it.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PThreadCreate
 */
extern uint32_t
CyU3PThreadDestroy (
        CyU3PThread *thread_ptr         /**< Pointer to the thread structure. */
        );

/** \brief Get the thread structure corresponding to the current thread.

    <B>Description</B>\n
    This function returns a pointer to the thread structure corresponding
    to the active thread, or NULL if called from interrupt context.

    <B>Return value</B>\n
    * Pointer to the thread structure of the currently running thread.

    **\see
    *\see CyU3PThreadCreate
    *\see CyU3PThreadInfoGet
 */
extern CyU3PThread *
CyU3PThreadIdentify (
        void);

/** \brief Retrieve information regarding a specified thread.

    <B>Description</B>\n
    This function is used to retrieve information about a thread whose pointer is
    provided. This function is used by the debug mechanism in the firmware library
    to get information about the source thread which is queuing log messages.

    Valid (non-NULL) pointers need to be provided only for the output fields that
    need to be retrieved.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PThreadCreate
    *\see CyU3PThreadIdentify
 */
extern uint32_t
CyU3PThreadInfoGet (
        CyU3PThread *thread_p,                  /**< Pointer to thread to be queried. */
        uint8_t **name_p,                       /**< Output variable to be filled with the thread's name string. */
        uint32_t *priority,                     /**< Output variable to be filled with the thread priority. */
        uint32_t *preemptionThreshold,          /**< Output variable to be filled with the pre-emption threshold. */
        uint32_t *timeSlice                     /**< Output variable to be filled with the time slice value. */
        );

/** \brief Change the priority of a thread.

    <B>Description</B>\n
    This function can be used to change the priority of a specified thread.
    Though this is not expected to be used commonly, it can be used for temporary
    priority changes to prevent deadlocks.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PThreadCreate
    *\see CyU3PThreadRelinquish
    *\see CyU3PThreadSleep
 */
extern uint32_t
CyU3PThreadPriorityChange (
        CyU3PThread *thread_p,                  /**< Pointer to thread to be modified. */
        uint32_t newPriority,                   /**< Priority value to assign to the thread. */
        uint32_t *oldPriority                   /**< Output variable that will hold the original priority. */
        );

/** \brief Relinquish control to the OS scheduler.

    <B>Description</B>\n
    This is a RTOS call for fair scheduling which relinquishes control to other ready
    threads that are at the same priority level. The thread that relinquishes control
    remains in ready state and can regain control if there are no other ready threads
    with the same priority level.
 
    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PThreadCreate
    *\see CyU3PThreadSleep
    *\see CyU3PThreadSuspend
 */
extern void
CyU3PThreadRelinquish (
        void);

/** \brief Puts a thread to sleep for the specified timer ticks.

    <B>Description</B>\n
    This function puts the current thread to sleep for the specified number of timer ticks.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PThreadCreate
    *\see CyU3PThreadRelinquish
    *\see CyU3PThreadSuspend
 */
extern uint32_t
CyU3PThreadSleep (
        uint32_t timerTicks             /**< Number of timer ticks to sleep. */
        );

/** \brief Suspends the specified thread.

    <B>Description</B>\n
    This function is used to suspend a thread that is in the ready state, and can
    be called from any thread.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PThreadCreate
    *\see CyU3PThreadRelinquish
    *\see CyU3PThreadSleep
    *\see CyU3PThreadResume
 */
extern uint32_t
CyU3PThreadSuspend (
        CyU3PThread *thread_p           /**< Pointer to the thread to be suspended. */
        );

/** \brief Resume operation of a thread that was previously suspended.

    <B>Description</B>\n
    This function is used to resume operation of a thread that was suspended
    using the CyU3PThreadSuspend call. Threads that are suspended because
    they are blocked on Mutexes or Events cannot be resumed using this call.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PThreadSuspend
 */
extern uint32_t
CyU3PThreadResume (
        CyU3PThread *thread_p           /**< Pointer to thread to be resumed. */
        );

/** \brief Returns a thread to ready state by aborting all waits on the thread.

    <B>Description</B>\n
    This function is used to restore a thread to ready state by aborting
    any waits that the thread is performing on Queues, Mutexes or Events.
    The wait operations will return with an error code that indicates that
    they were aborted.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PThreadCreate
    *\see CyU3PThreadSleep
    *\see CyU3PThreadSuspend
    *\see CyU3PThreadResume
 */
extern uint32_t
CyU3PThreadWaitAbort (
        CyU3PThread *thread_p           /**< Pointer to the thread to restore. */
        );

#ifdef CYU3P_DEBUG

/** \brief Type of thread stack overflow handler function.

    <B>Description</B>\n
    This type represents the callback function that will be called when the RTOS
    detects a stack overflow condition on one of the user created threads.

    **\see
    *\see CyU3PThreadStackErrorNotify
 */
typedef void (*CyU3PThreadStackErrorHandler_t) (
        CyU3PThread *thread_p           /**< Pointer to thread on which stack overflow was detected. */
        );

/** \brief Registers a callback function that will be notified of thread stack overflows.

    <B>Description</B>\n
    This function is only provided in debug builds of the firmware
    and allows the application to register a callback function that
    will be called to notify when any thread in the system has
    encountered a stack overflow.

    This API must be called before any user thread is created.
    It is recommended to invoke this API from CyFxApplicationDefine().

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PThreadCreate
    *\see CyU3PThreadStackErrorHandler_t
 */
extern uint32_t
CyU3PThreadStackErrorNotify (
        CyU3PThreadStackErrorHandler_t errorHandler     /**< Pointer to thread stack error handler function. */
        );
#endif

/***************************** Queue functions ****************************/

/** \brief Create a message queue.

    <B>Description</B>\n
    Create a message queue that can hold a specified number of messages of a
    specified size. The memory for the queue should be allocated and passed in
    by the caller.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PQueueDestroy
    *\see CyU3PQueueSend
    *\see CyU3PQueuePrioritySend
    *\see CyU3PQueueReceive
    *\see CyU3PQueueFlush
 */
extern uint32_t
CyU3PQueueCreate (
        CyU3PQueue *queue_p,            /**< Pointer to the queue structure. This should
                                             be allocated by the caller and will be initialized
                                             by the queue create function. */
        uint32_t messageSize,           /**< Size of each message in 4 byte words. Allowed values
                                             are from 1 to 16 (4 bytes to 64 bytes). */
        void *queueStart,               /**< Pointer to memory buffer to be used for message storage.
                                             Should be aligned to 4 bytes. */
        uint32_t queueSize              /**< Total size of the queue in bytes. */
        );

/** \brief Free up a previously created message queue.

    <B>Description</B>\n
    This function frees up a previously created message queue. Any function
    call that is waiting to send or receive messages on this queue will return
    with an error.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PQueueCreate
 */
extern uint32_t
CyU3PQueueDestroy (
        CyU3PQueue *queue_p             /**< Pointer to the queue to be destroyed. */
        );

/** \brief Queue a new message on the specified message queue.

    <B>Description</B>\n
    This function adds a new message on to the specified message queue. If the
    queue is full, it waits for a message slot to be freed. The amount of time
    to wait is specified as a parameter. In case this function is called from
    an interrupt handler, the time-out should be specified as CYU3P_NO_WAIT.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PQueuePrioritySend
    *\see CyU3PQueueReceive
    *\see CyU3PQueueFlush
 */
extern uint32_t
CyU3PQueueSend (
        CyU3PQueue *queue_p,            /**< Queue to add a new message to. */
        void *src_p,                    /**< Pointer to buffer containing message. Should be aligned
                                             to 4 bytes. */
        uint32_t waitOption             /**< Timeout value to wait on the queue. */
        );

/** \brief Add a new message at the head of a message queue.

    <B>Description</B>\n
    This function is used to send a high priority message to a message queue.
    This message will be placed at the head of the queue instead of at the tail.
    As in the case of the CyU3PQueueSend API, this waits for the specified duration
    or until a message slot is free in the queue.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PQueueSend
    *\see CyU3PQueueReceive
    *\see CyU3PQueueFlush
 */
extern uint32_t
CyU3PQueuePrioritySend (
        CyU3PQueue *queue_p,            /**< Pointer to the message queue structure. */
        void *src_p,                    /**< Pointer to message buffer. Should be aligned to 4 bytes. */
        uint32_t waitOption             /**< Timeout duration in timer ticks. */
        );

/** \brief Receive a message from a message queue.

    <B>Description</B>\n
    This function receives the message at the head of the specified queue. If
    the queue is empty, it waits until the specified timeout period has elapsed,
    or until the queue is non-empty. If calling from an interrupt handler, the
    waitOption parameter needs to be set to CYU3P_NO_WAIT.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PQueueSend
    *\see CyU3PQueuePrioritySend
    *\see CyU3PQueueFlush
 */
extern uint32_t
CyU3PQueueReceive (
        CyU3PQueue *queue_p,            /**< Pointer to the message queue. */
        void *dest_p,                   /**< Pointer to buffer where the message should be copied.
                                             Should be aligned to 4 bytes and large enough to hold
                                             the full message. */
        uint32_t waitOption             /**< Timeout duration in timer ticks. */
        );

/** \brief Flushes all messages on a queue.

    <B>Description</B>\n
    The function removes all pending messages on the specified queue.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PQueueSend
    *\see CyU3PQueuePrioritySend
    *\see CyU3PQueueReceive
 */
extern uint32_t
CyU3PQueueFlush (
        CyU3PQueue *queue_p             /**< Pointer to the queue to be flushed. */
        );

/***************************** Mutex functions ****************************/

/** \brief Create a mutex variable.

    <B>Description</B>\n
    This function creates a mutex variable. The mutex data structure has to
    be allocated by the caller, and will be initialized by the function.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.
 
    **\see
    *\see CyU3PMutexDestroy
    *\see CyU3PMutexGet
    *\see CyU3PMutexPut
 */
extern uint32_t
CyU3PMutexCreate (
        CyU3PMutex *mutex_p,            /**< Pointer to Mutex data structure to be initialized. */
        uint32_t priorityInherit        /**< Whether priority inheritance should be allowed for
                                           this mutex. Allowed values for this parameter are
                                           CYU3P_NO_INHERIT and CYU3P_INHERIT. */
        );

/** \brief Destroy a mutex variable.

    <B>Description</B>\n
    This function destroys a mutex that was created using the CyU3PMutexCreate API.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PMutexCreate
 */
extern uint32_t
CyU3PMutexDestroy (
        CyU3PMutex *mutex_p             /**< Pointer to mutex to be destroyed. */
        );

/** \brief Get the lock on a mutex variable.

    <B>Description</B>\n
    This function is used to wait on a mutex variable and to get a lock on it.
    The maximum amount of time to wait is specified through the waitOption
    parameter.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PMutexCreate
    *\see CyU3PMutexDestroy
 */
extern uint32_t
CyU3PMutexGet (
        CyU3PMutex *mutex_p,            /**< Pointer to mutex to be acquired. */
        uint32_t waitOption             /**< Timeout duration in timer ticks. */
        );

/** \brief Release the lock on a mutex variable.

    <B>Description</B>\n
    This function is used to release the lock on a mutex variable. The mutex
    can then be acquired by the highest priority thread from among those
    waiting for it.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PMutexGet
 */
extern uint32_t
CyU3PMutexPut (
        CyU3PMutex *mutex_p             /**< Pointer to mutex to be released. */
        );

/*************************** Semaphore functions **************************/

/** \brief Create a semaphore object.

    <B>Description</B>\n
    This function creates a semaphore object with the specified initial count.
    The semaphore data structure has to be allocated by the caller and will be
    initialized by this function call.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PSemaphoreDestroy
    *\see CyU3PSemaphoreGet
    *\see CyU3PSemaphorePut
 */
extern uint32_t
CyU3PSemaphoreCreate (
        CyU3PSemaphore *semaphore_p,    /**< Pointer to semaphore to be initialized. */
        uint32_t initialCount           /**< Initial count to associate with semaphore. */
        );

/** \brief Destroy a semaphore object.

    <B>Description</B>\n
    This function destroys a semaphore object. All threads waiting to get
    the semaphore will receive an error code identifying that the object has
    been removed.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PSemaphoreCreate
 */
extern uint32_t
CyU3PSemaphoreDestroy (
        CyU3PSemaphore *semaphore_p     /**< Pointer to semaphore to be destroyed. */
        );

/** \brief Get an instance from the specified counting semaphore.

    <B>Description</B>\n
    This function is used to get an instance (i.e., decrement the count by one)
    from the specified counting semaphore. If the count is already zero, the function
    will wait until the count becomes non-zero. The maximum interval to wait for
    is specified using the waitOption parameter.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PSemaphorePut
 */
extern uint32_t
CyU3PSemaphoreGet (
        CyU3PSemaphore *semaphore_p,    /**< Pointer to semaphore to get. */
        uint32_t waitOption             /**< Timeout duration in timer ticks. */
        );

/** \brief Release an instance of the specified counting semaphore.

    <B>Description</B>\n
    This function releases an instance (increments the count by one) of
    the specified counting semaphore. The semaphore put can be done from
    any thread and does not need to be done by the same thread as called
    the get function.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PSemaphoreCreate
    *\see CyU3PSemaphoreDestroy
    *\see CyU3PSemaphoreGet
 */
extern uint32_t
CyU3PSemaphorePut (
        CyU3PSemaphore *semaphore_p     /**< Pointer to semaphore to put. */
        );

/***************************** Event functions ****************************/

/** \brief Create an event flag group.

    <B>Description</B>\n
    This function creates an event flag group consisting of 32 independent
    event flags. The application is free to use as many flags as required
    from this group. If more than 32 flags are required, multiple event flag
    groups have to be created.

    As with other OS service create functions, the caller is expected to
    allocate memory for the Event data structure.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PEventDestroy
    *\see CyU3PEventSet
    *\see CyU3PEventGet
 */
extern uint32_t
CyU3PEventCreate (
        CyU3PEvent *event_p             /**< Pointer to event structure to be initialized. */
        );

/** \brief Destroy an event flag group.

    <B>Description</B>\n
    This function frees up an event flag group. Any threads waiting on one or more
    flags in the group will be re-activated and will receive an error code from the
    event wait function.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PEventCreate
 */
extern uint32_t
CyU3PEventDestroy (
        CyU3PEvent *event_p             /**< Pointer to event group to be destroyed. */
        );

/** \brief Update one or more flags in an event group.

    <B>Description</B>\n
    This function is used to set or clear one or more flags that are part of a
    specified event group. The parameters can be selected so as to set a number
    of specified flags, or to clear of number of specified flags.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PEventGet
 */
extern uint32_t
CyU3PEventSet (
        CyU3PEvent *event_p,            /**< Pointer to event group to update. */
        uint32_t rqtFlag,               /**< Bit-mask that selects event flags of interest. */
        uint32_t setOption              /**< Type of set operation to perform:
                                             CYU3P_EVENT_AND: The rqtFlag will be ANDed with the current flags.
                                             CYU3P_EVENT_OR: The rqtFlag will be ORed with the current flags. */
        );

/** \brief Wait for or get the status of an event flag group.

    <B>Description</B>\n
    This function is used to the get the current status of the flags in a
    event group. This can also be used to wait until one or more flags of
    interest have been signaled. The maximum amount of time to wait is
    specified through the waitOption parameter.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PEventSet
 */
extern uint32_t
CyU3PEventGet (
        CyU3PEvent *event_p,            /**< Pointer to event group to be queried. */
        uint32_t rqtFlag,               /**< Bit-mask that selects event flags of interest. */
        uint32_t getOption,             /**< Type of operation to perform:
                                             CYU3P_EVENT_AND: Use to wait until all selected flags are signaled.
                                             CYU3P_EVENT_AND_CLEAR: Same as above, and clear the flags on read.
                                             CYU3P_EVENT_OR: Wait until any of selected flags are signaled.
                                             CYU3P_EVENT_OR_CLEAR: Same as above, and clear the flags on read. */
        uint32_t *flag_p,               /**< Output parameter filled in with the state of the flags. */
        uint32_t waitOption             /**< Timeout duration in timer ticks. */
        );

/***************************** Timer functions ****************************/

/** \brief Create an application timer.

    <B>Description</B>\n
    This function creates an application timer than can be configured as a
    one-shot timer or as a auto-reload timer.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PTimerCb_t
    *\see CyU3PTimerDestroy
    *\see CyU3PTimerStart
    *\see CyU3PTimerStop
    *\see CyU3PTimerModify
    *\see CyU3PGetTime
    *\see CyU3PSetTime
 */
extern uint32_t
CyU3PTimerCreate (
        CyU3PTimer *timer_p,                    /**< Pointer to the timer structure to be initialized. */
        CyU3PTimerCb_t expirationFunction,      /**< Pointer to callback function called on timer expiration. */
        uint32_t expirationInput,               /**< Parameter to be passed to the callback function. */
        uint32_t initialTicks,                  /**< Initial value for the timer. This timer count will
                                                   be decremented once per timer tick and the callback
                                                   will be invoked once the count reaches zero. */
        uint32_t rescheduleTicks,               /**< The reload value for the timer. If set to zero, the timer will
                                                     be a one-shot timer. */
        uint32_t timerOption                    /**< Timer start control:
                                                     CYU3P_AUTO_ACTIVATE: Start the timer immediately.
                                                     CYU3P_NO_ACTIVATE: Timer needs to be started explicitly. */
        );

/** \brief Destroy an application timer object.

    <B>Description</B>\n
    This function destroys and application timer object.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PTimerCreate
 */
extern uint32_t
CyU3PTimerDestroy (
        CyU3PTimer *timer_p             /**< Pointer to the timer to be destroyed. */
        );

/** \brief Start an application timer.

    <B>Description</B>\n
    This function activates a previously stopped timer. This operation can be used
    to start a timer that was created with the CYU3P_NO_ACTIVATE option, or to re-start
    a one-shot or continuous timer that was stopped previously.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PTimerCreate
    *\see CyU3PTimerStop
    *\see CyU3PTimerModify
 */
extern uint32_t
CyU3PTimerStart (
        CyU3PTimer *timer_p             /**< Timer to be started. */
        );

/** \brief Stop operation of an application timer.

    <B>Description</B>\n
    This function can be used to stop operation of an application timer. The
    parameters associated with the timer can then be modified using the
    CyU3PTimerModify call.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PTimerStart
    *\see CyU3PTimerModify
 */
extern uint32_t
CyU3PTimerStop (
        CyU3PTimer *timer_p             /**< Pointer to timer to be stopped. */
        );

/** \brief Modify parameters of an application timer.

    <B>Description</B>\n
    This function is used to modify the periodicity of an application timer
    and can be called only when the timer is stopped.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS (0) on success.\n
    * Other error codes on failure.

    **\see
    *\see CyU3PTimerCreate
    *\see CyU3PTimerStart
    *\see CyU3PTimerStop
 */
extern uint32_t
CyU3PTimerModify (
        CyU3PTimer *timer_p,            /**< Pointer to the timer. */
        uint32_t initialTicks,          /**< Initial count to be set for the timer. */
        uint32_t rescheduleTicks        /**< Reload count for the timer. */
        );

/** \brief Get the number of elapsed OS timer ticks since FX3 system start-up.

    <B>Description</B>\n
    The function returns the number of elapsed OS timer ticks since the FX3 OS
    was started up.

    <B>Return value</B>\n
    * Current OS timer tick count.

    **\see
    *\see CyU3PSetTime
 */
extern uint32_t
CyU3PGetTime (
        void);

/** \brief Update the timer tick count.

    <B>Description</B>\n
    This function modifies the timer tick count that is started at system reset.
    This function should ideally not be called after the application is started
    up, as it can affect the operation of timers and threads.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PGetTime
 */
extern void
CyU3PSetTime (
        uint32_t newTime                /**< New timer tick value to set. */
        );

/**************************************************************************/

/* Performance reporting functions are only available in a profiling enabled build of the FX3 libraries. */

/** \brief Get OS performance profile information.

    <B>Description</B>\n
    The ThreadX operating system supports collection of system performance data during firmware
    execution. The cumulative thread performance data collected by the OS can be retrieved using
    this API.

    <B>Return Value</B>\n
    * CY_U3P_SUCCESS if the data has been retrieved.\n
    * CY_U3P_ERROR_OPERN_DISABLED if the performance collection is not enabled in the library.
 */
extern uint32_t
CyU3PThreadSystemPerfGet (
        uint32_t *resumeCnt_p,          /**< Destination pointer to be filled with thread resume count. */
        uint32_t *suspendCnd_p,         /**< Destination pointer to be filled with thread suspend count. */
        uint32_t *reqPreemptionCnt_p,   /**< Destination pointer to be filled with count of thread pre-emptions
                                             due to an RTOS API call. */
        uint32_t *intrPreemptionCnt_p,  /**< Destination pointer to be filled with count of thread pre-emptions
                                             due to interrupts. */
        uint32_t *prioInvertCnt_p,      /**< Destination pointer to be filled with count of thread priority
                                             inversions. */
        uint32_t *relinquishCnt_p,      /**< Destination pointer to be filled with count of thread relinquish calls. */
        uint32_t *timeoutCnt_p,         /**< Destination pointer to be filled with count of thread suspension
                                             timeouts. */
        uint32_t *busyReturnCnt_p,      /**< Destination pointer to be filled with number of times a thread
                                             returned with other threads ready to execute. */
        uint32_t *idleReturnCnt_p       /**< Destination pointer to be filled with number of times a thread
                                             returned with no other threads ready to execute. */
        );

/** \brief Get thread specific performance profile information.

    <B>Description</B>\n
    The ThreadX operating system supports collection of system performance data during firmware
    execution. This API returns profiling information specific to one thread object.

    <B>Return Value</B>\n
    * CY_U3P_SUCCESS if the data has been retrieved.\n
    * CY_U3P_ERROR_BAD_POINTER if the thread pointer is not valid.\n
    * CY_U3P_ERROR_OPERN_DISABLED if the performance collection is not enabled in the library.
 */
extern uint32_t
CyU3PThreadPerfGet (
        CyU3PThread *thread_p,          /**< Pointer to thread to be queried. */
        uint32_t *resumeCnt_p,          /**< Destination pointer to be filled with thread resume count. */
        uint32_t *suspendCnd_p,         /**< Destination pointer to be filled with thread suspend count. */
        uint32_t *reqPreemptionCnt_p,   /**< Destination pointer to be filled with count of thread pre-emptions
                                             due to an RTOS API call. */
        uint32_t *intrPreemptionCnt_p,  /**< Destination pointer to be filled with count of thread pre-emptions
                                             due to interrupts. */
        uint32_t *prioInvertCnt_p,      /**< Destination pointer to be filled with count of thread priority
                                             inversions. */
        uint32_t *relinquishCnt_p,      /**< Destination pointer to be filled with count of thread relinquish calls. */
        uint32_t *timeoutCnt_p          /**< Destination pointer to be filled with count of thread suspension
                                             timeouts. */
        );

/** \brief Get global OS timer performance information.

    <B>Description</B>\n
    The ThreadX operating system supports collection of system performance data on all OS
    objects during firmware execution. This function retrieves the performance statistics
    associated with all timer objects in the system.

    <B>Return Value</B>\n
    * CY_U3P_SUCCESS if the data has been retrieved.\n
    * CY_U3P_ERROR_BAD_POINTER if the timer pointer is not valid.\n
    * CY_U3P_ERROR_OPERN_DISABLED if the performance collection is not enabled in the library.
 */
extern uint32_t
CyU3PTimerSystemPerfGet (
        uint32_t *activateCnt_p,        /**< Destination pointer to be filled with count of explicit timer
                                             activations. */
        uint32_t *reactivateCnt_p,      /**< Destination pointer to be filled with count of automatic
                                             timer re-activations. */
        uint32_t *deactivateCnt_p,      /**< Destination pointer to be filled with count of timer de-activations. */
        uint32_t *expiryCnt_p           /**< Destination pointer to be filled with count of timer expirations. */
        );

/** \brief Get timer specific performance profile information.

    <B>Description</B>\n
    The ThreadX operating system supports collection of system performance data on all OS
    objects during firmware execution. This function retrieves the performance statistics
    associated with a specific OS timer object.

    <B>Return Value</B>\n
    * CY_U3P_SUCCESS if the data has been retrieved.\n
    * CY_U3P_ERROR_BAD_POINTER if the timer pointer is not valid.\n
    * CY_U3P_ERROR_OPERN_DISABLED if the performance collection is not enabled in the library.
 */
extern uint32_t
CyU3PTimerPerfGet (
        CyU3PTimer *timer_p,            /**< Pointer to timer to be queried. */
        uint32_t *activateCnt_p,        /**< Destination pointer to be filled with count of explicit timer
                                             activations. */
        uint32_t *reactivateCnt_p,      /**< Destination pointer to be filled with count of automatic
                                             timer re-activations. */
        uint32_t *deactivateCnt_p,      /**< Destination pointer to be filled with count of timer de-activations. */
        uint32_t *expiryCnt_p           /**< Destination pointer to be filled with count of timer expirations. */
        );

/** \brief Get global mutex performance profile information.

    <B>Description</B>\n
    The ThreadX operating system supports collection of system performance data on all OS
    objects during firmware execution. This function retrieves the performance statistics
    associated with all mutex objects in the system.

    <B>Return Value</B>\n
    * CY_U3P_SUCCESS if the data has been retrieved.\n
    * CY_U3P_ERROR_OPERN_DISABLED if the performance collection is not enabled in the library.
 */
extern uint32_t
CyU3PMutexSystemPerfGet (
        uint32_t *putCnt_p,             /**< Destination pointer to be filled with number of Mutex put operations. */
        uint32_t *getCnt_p,             /**< Destination pointer to be filled with number of Mutex get operations. */
        uint32_t *suspCnt_p,            /**< Destination pointer to be filled with number of mutex get suspensions. */
        uint32_t *timeoutCnt_p,         /**< Destination pointer to be filled with number of Mutex get timeouts. */
        uint32_t *invertCnt_p,          /**< Destination pointer to be filled with number of thread priority inversions
                                             on mutexes. */
        uint32_t *inheritCnt_p          /**< Destination pointer to be filled with number of thread priority
                                             inheritances on mutexes. */
        );

/** \brief Get mutex specific performance profile information.

    <B>Description</B>\n
    The ThreadX operating system supports collection of system performance data on all OS
    objects during firmware execution. This function retrieves the performance statistics
    associated with a specific mutex object.

    <B>Return Value</B>\n
    * CY_U3P_SUCCESS if the data has been retrieved.\n
    * CY_U3P_ERROR_BAD_POINTER if the mutex pointer is not valid.\n
    * CY_U3P_ERROR_OPERN_DISABLED if the performance collection is not enabled in the library.
 */
extern uint32_t
CyU3PMutexPerfGet (
        CyU3PMutex *mutex_p,            /**< Pointer to mutex to be queried. */
        uint32_t *putCnt_p,             /**< Destination pointer to be filled with number of Mutex put operations. */
        uint32_t *getCnt_p,             /**< Destination pointer to be filled with number of Mutex get operations. */
        uint32_t *suspCnt_p,            /**< Destination pointer to be filled with number of mutex get suspensions. */
        uint32_t *timeoutCnt_p,         /**< Destination pointer to be filled with number of Mutex get timeouts. */
        uint32_t *invertCnt_p,          /**< Destination pointer to be filled with number of thread priority inversions
                                             on mutexes. */
        uint32_t *inheritCnt_p          /**< Destination pointer to be filled with number of thread priority
                                             inheritances on mutexes. */
        );

/** \brief Get global event flag performance profile information.

    <B>Description</B>\n
    The ThreadX operating system supports collection of system performance data on all OS
    objects during firmware execution. This function retrieves the performance statistics
    associated with all event flag group objects in the system.

    <B>Return Value</B>\n
    * CY_U3P_SUCCESS if the data has been retrieved.\n
    * CY_U3P_ERROR_OPERN_DISABLED if the performance collection is not enabled in the library.
 */
extern uint32_t
CyU3PEventSystemPerfGet (
        uint32_t *setCnt_p,             /**< Destination pointer to be filled with number of event set operations. */
        uint32_t *getCnt_p,             /**< Destination pointer to be filled with number of event get operations. */
        uint32_t *suspCnt_p,            /**< Destination pointer to be filled with number of thread suspensions
                                             due to event get operations. */
        uint32_t *timeoutCnt_p          /**< Destination pointer to be filled with number of event get timeouts. */
        );

/** \brief Get event flag specific performance profile information.

    <B>Description</B>\n
    The ThreadX operating system supports collection of system performance data on all OS
    objects during firmware execution. This function retrieves the performance statistics
    associated with a specific event flag group object.

    <B>Return Value</B>\n
    * CY_U3P_SUCCESS if the data has been retrieved.\n
    * CY_U3P_ERROR_BAD_POINTER if the event flag group pointer is not valid.\n
    * CY_U3P_ERROR_OPERN_DISABLED if the performance collection is not enabled in the library.
 */
extern uint32_t
CyU3PEventPerfGet (
        CyU3PEvent *event_p,            /**< Pointer to the event flag group to be queried. */
        uint32_t *setCnt_p,             /**< Destination pointer to be filled with number of event set operations. */
        uint32_t *getCnt_p,             /**< Destination pointer to be filled with number of event get operations. */
        uint32_t *suspCnt_p,            /**< Destination pointer to be filled with number of thread suspensions
                                             due to event get operations. */
        uint32_t *timeoutCnt_p          /**< Destination pointer to be filled with number of event get timeouts. */
        );

/** \brief Get global message queue performance profile information.

    <B>Description</B>\n
    The ThreadX operating system supports collection of system performance data on all OS
    objects during firmware execution. This function retrieves the performance statistics
    associated with all message queue objects in the system.

    <B>Return Value</B>\n
    * CY_U3P_SUCCESS if the data has been retrieved.\n
    * CY_U3P_ERROR_OPERN_DISABLED if the performance collection is not enabled in the library.
 */
extern uint32_t
CyU3PQueueSystemPerfGet (
        uint32_t *sentCnt_p,            /**< Destination pointer to be filled with number of messages sent. */
        uint32_t *recvCnt_p,            /**< Destination pointer to be filled with number of messages received. */
        uint32_t *emptyCnt_p,           /**< Destination pointer to be filled with number of queue receive
                                             suspensions due to queue emptiness. */
        uint32_t *fullCnt_p,            /**< Destination pointer to be filled with number of queue send
                                             suspensions due to queue fullness. */
        uint32_t *fullErrCnt_p,         /**< Destination pointer to be filled with number of queue send
                                             failures due to queue fullness. */
        uint32_t *timeoutCnt_p          /**< Destination pointer to be filled with number of queue timeouts. */
        );

/** \brief Get message queue specific performance profile information.

    <B>Description</B>\n
    The ThreadX operating system supports collection of system performance data on all OS
    objects during firmware execution. This function retrieves the performance statistics
    associated with a specific message queue object.

    <B>Return Value</B>\n
    * CY_U3P_SUCCESS if the data has been retrieved.\n
    * CY_U3P_ERROR_BAD_POINTER if the message queue pointer is not valid.\n
    * CY_U3P_ERROR_OPERN_DISABLED if the performance collection is not enabled in the library.
 */
extern uint32_t
CyU3PQueuePerfGet (
        CyU3PQueue *queue_p,            /**< Pointer to the queue to be queried. */
        uint32_t *sentCnt_p,            /**< Destination pointer to be filled with number of messages sent. */
        uint32_t *recvCnt_p,            /**< Destination pointer to be filled with number of messages received. */
        uint32_t *emptyCnt_p,           /**< Destination pointer to be filled with number of queue receive
                                             suspensions due to queue emptiness. */
        uint32_t *fullCnt_p,            /**< Destination pointer to be filled with number of queue send
                                             suspensions due to queue fullness. */
        uint32_t *fullErrCnt_p,         /**< Destination pointer to be filled with number of queue send
                                             failures due to queue fullness. */
        uint32_t *timeoutCnt_p          /**< Destination pointer to be filled with number of queue timeouts. */
        );

/** \brief Register a GPIO to be updated when a thread is made active or inactive.

    <B>Description</B>\n
    This function registers an FX3 GPIO (simple GPIO) that will be updated by the RTOS scheduler
    when it is making the specified thread active or inactive. The GPIO number cannot be 0, and has
    to be a valid GPIO number on the FX3 device. The GPIO will be driven high when the thread is made
    active and driven low when it is made inactive.

    This function is only available in a profiling enabled build of the FX3 libraries. The caller is
    responsible for ensuring that the GPIO block is active and that the corresponding GPIO is enabled.

    <B>Return Value</B>\n
    * CY_U3P_SUCCESS if the data has been retrieved.\n
    * CY_U3P_ERROR_INVALID_CALLER if the GPIO block has not been enabled.\n
    * CY_U3P_ERROR_BAD_OPTION if the GPIO specified is invalid.\n
    * CY_U3P_ERROR_OPERN_DISABLED if this function is not supported (regular FX3 build).
 */
extern uint32_t
CyU3PThreadSetActivityGpio (
        CyU3PThread *thread_p,          /**< Pointer to the thread object to be monitored. */
        uint32_t     gpio_id            /**< GPIO to be used for this thread. */
        );

/** \brief Register a GPIO to be updated based on the status of a Mutex variable.

    <B>Description</B>\n
    This function registers an FX3 GPIO (simple GPIO) that will be updated by the RTOS services when
    making changes to the state of a mutex variable. The GPIO will be driven high when the mutex is
    free (available) and driven low when the mutex is not available.

    This function is only available in a profiling enabled build of the FX3 libraries. The caller is
    responsible for ensuring that the GPIO block is active and that the corresponding GPIO is enabled.

    <B>Return Value</B>\n
    * CY_U3P_SUCCESS if the data has been retrieved.\n
    * CY_U3P_ERROR_INVALID_CALLER if the GPIO block has not been enabled.\n
    * CY_U3P_ERROR_BAD_OPTION if the GPIO specified is invalid.\n
    * CY_U3P_ERROR_OPERN_DISABLED if this function is not supported (regular FX3 build).
 */
extern uint32_t
CyU3PMutexSetActivityGpio (
        CyU3PMutex  *mutex_p,           /**< Pointer to the mutex object to be monitored. */
        uint32_t     gpio_id            /**< GPIO to be used for this mutex. */
        );

/** \brief Register a GPIO to be updated based on the status of a Semaphore variable.

    <B>Description</B>\n
    This function registers an FX3 GPIO (simple GPIO) that will be updated by the RTOS services when
    making changes to the state of a semaphore variable. The GPIO will be driven high when the semaphore
    count is non-zero, and driven low when the semaphore count is zero.

    This function is only available in a profiling enabled build of the FX3 libraries. The caller is
    responsible for ensuring that the GPIO block is active and that the corresponding GPIO is enabled.

    <B>Return Value</B>\n
    * CY_U3P_SUCCESS if the data has been retrieved.\n
    * CY_U3P_ERROR_INVALID_CALLER if the GPIO block has not been enabled.\n
    * CY_U3P_ERROR_BAD_OPTION if the GPIO specified is invalid.\n
    * CY_U3P_ERROR_OPERN_DISABLED if this function is not supported (regular FX3 build).
 */
extern uint32_t
CyU3PSemaphoreSetActivityGpio (
        CyU3PSemaphore  *semaphore_p,   /**< Pointer to the semaphore object to be monitored. */
        uint32_t         gpio_id        /**< GPIO to be used for this semaphore. */
        );

/** \brief Register a GPIO to be updated based on the status of a event flags group.

    <B>Description</B>\n
    This function registers an FX3 GPIO (simple GPIO) that will be updated by the RTOS services when
    making changes to the state of an event flags group. The GPIO will be driven high when at least
    one of the events in the group is active, and will be driven low when all of the events are
    inactive.

    This function is only available in a profiling enabled build of the FX3 libraries. The caller is
    responsible for ensuring that the GPIO block is active and that the corresponding GPIO is enabled.

    <B>Return Value</B>\n
    * CY_U3P_SUCCESS if the data has been retrieved.\n
    * CY_U3P_ERROR_INVALID_CALLER if the GPIO block has not been enabled.\n
    * CY_U3P_ERROR_BAD_OPTION if the GPIO specified is invalid.\n
    * CY_U3P_ERROR_OPERN_DISABLED if this function is not supported (regular FX3 build).
 */
extern uint32_t
CyU3PEventSetActivityGpio (
        CyU3PEvent  *event_p,           /**< Pointer to the semaphore object to be monitored. */
        uint32_t     gpio_id            /**< GPIO to be used for this semaphore. */
        );

/***** ThreadX RTOS *****/

#ifdef __CYU3P_TX__
#ifdef CYU3P_OS_DEFINED
#error Only one RTOS can be defined
#endif

#define CYU3P_OS_DEFINED

#include "cyu3tx.h"
#endif

#include "cyu3externcend.h"

#endif /* _INCLUDED_CYU3P_OS_H_ */

/*[]*/

