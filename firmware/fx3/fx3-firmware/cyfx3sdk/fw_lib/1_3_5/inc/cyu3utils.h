/*
 ## Cypress FX3 Core Library Header (cyu3utils.h)
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

#ifndef _INCLUDED_CYU3P_UTILS_H_
#define _INCLUDED_CYU3P_UTILS_H_

#include "cyu3types.h"
#include "cyfx3_api.h"
#include "assert.h"
#include "cyu3externcstart.h"

/** \file cyu3utils.h
    \brief Utility functions provided by the FX3 library.

    <B>Description</B>\n
    The utility APIs are generic functions that are provided as helpers
    for the FX3 APIs.
*/

/**************************************************************************
 ********************************** Macros ********************************
 **************************************************************************/

/** \def CY_U3P_MIN
    \brief Find the minimum of two numbers.
 */
#define CY_U3P_MIN(a, b)         (((a) > (b)) ? (b) : (a))

/** \def CY_U3P_MAX
    \brief Find the maximum of two numbers.
 */
#define CY_U3P_MAX(a, b)         (((a) > (b)) ? (a) : (b))

/** \def CY_U3P_MAKEWORD
    \brief Create a word (16-bit) from two 8-bit numbers.
*/
#define CY_U3P_MAKEWORD(u, l)    ((uint16_t)(((u) << 8) | (l)))

/** \def CY_U3P_GET_LSB
    \brief Get the LS byte from a 16-bit number.
*/
#define CY_U3P_GET_LSB(w)        ((uint8_t)((w) & UINT8_MAX))

/** \def CY_U3P_GET_LSW
    \brief Get the LS word from a 32-bit number.
*/
#define CY_U3P_GET_LSW(d)        ((uint16_t)((d) & UINT16_MAX))

/** \def CY_U3P_GET_MSB
    \brief Get the MS byte from a 16-bit number.
*/
#define CY_U3P_GET_MSB(w)        ((uint8_t)((w) >> 8))

/** \def CY_U3P_GET_MSW
    \brief Get the MS word from a 32-bit number.
*/
#define CY_U3P_GET_MSW(d)        ((uint16_t)((d) >> 16))

/** \def CY_U3P_MAKEDWORD
    \brief Create a double word (32-bit) from four 8-bit numbers.
*/
#define CY_U3P_MAKEDWORD(b3, b2, b1, b0) ((uint32_t)((((uint32_t)(b3)) << 24) | (((uint32_t)(b2)) << 16) | \
                                         (((uint32_t)(b1)) << 8) | ((uint32_t)(b0))))

/** \def CY_U3P_DWORD_GET_BYTE0
    \brief Retrieves byte 0 from a 32 bit number.
*/
#define CY_U3P_DWORD_GET_BYTE0(d)        ((uint8_t)((d) & 0xFF))

/** \def CY_U3P_DWORD_GET_BYTE1
    \brief Retrieves byte 1 from a 32 bit number.
*/
#define CY_U3P_DWORD_GET_BYTE1(d)        ((uint8_t)(((d) >>  8) & 0xFF))

/** \def CY_U3P_DWORD_GET_BYTE2
    \brief Retrieves byte 2 from a 32 bit number.
*/
#define CY_U3P_DWORD_GET_BYTE2(d)        ((uint8_t)(((d) >> 16) & 0xFF))

/** \def CY_U3P_DWORD_GET_BYTE3
    \brief Retrieves byte 3 from a 32 bit number.
*/
#define CY_U3P_DWORD_GET_BYTE3(d)        ((uint8_t)(((d) >> 24) & 0xFF))

/** \def CY_U3P_MAKEDWORD1
    \brief Join two 16-bit words into a 32 bit double word.
*/
#define CY_U3P_MAKEDWORD1(uw, lw)       ((uint32_t)(((uint32_t)(uw) << 16) | ((uint32_t)(lw))))

/** \def CyU3PAssert
    \brief Assert function used to log errors when expected conditions are not satisfied.
*/
#define CyU3PAssert(cond)         assert(cond)

/**************************************************************************
 *************************** Function prototypes **************************
 **************************************************************************/

/** \brief Copy data 32-bits at a time from one memory location to another.

    <B>Description</B>\n
    This is a memcpy equivalent function. This requires that the addresses
    provided are four byte aligned. Since the ARM core is 32-bit, this API
    does a faster copy of data than the 1 byte equivalent (CyU3PMemCopy).
    This API is also used by the firmware library, and handles cases where
    the source and destination buffers are overlapping.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PMemCopy
 */
void
CyU3PMemCopy32 (
        uint32_t *dest,                 /**< Pointer to destination buffer. */
        uint32_t *src,                  /**< Pointer to source buffer. */
        uint32_t count                  /**< Size of the buffer (in words) to be copied. */
        );

/** \brief Delay function based on busy spinning in a loop.

    <B>Description</B>\n
    This function is used to insert small delays (of the order of micro-seconds) into
    the firmware application. The delay is implemented using a busy spin loop and can be
    used anywhere. This API should not be used for large delays as other lower or same
    priority threads will not be able to run during this.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PThreadSleep
 */
extern void
CyU3PBusyWait (
        uint16_t usWait                 /**< Delay duration in micro-seconds. */
        );

/** @cond DOXYGEN_HIDE */
/* The busy wait function is provided by the FX3 mini library. */
#define CyU3PBusyWait(usWait)   CyFx3BusyWait(usWait)
/** @endcond */

/** \brief Compute a checksum over a user specified data buffer.

    <B>Description</B>\n
    This function computes the binary sum of all values in a user specified data buffer
    and can be used as a simple checksum to verify data consistency. This checksum API
    is used by the boot-loader to determine the checksum as well.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the checksum is successfully computed.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the parameters provided are erroneous.
 */
extern CyU3PReturnStatus_t
CyU3PComputeChecksum (
        uint32_t *buffer,               /**< Pointer to data buffer on which to calculate the checksum. */
        uint32_t  length,               /**< Length of data buffer on which to calculate the checksum. */
        uint32_t *chkSum                /**< Pointer to buffer that will be filled with the checksum. */
        );

/** \brief Function to read one or more FX3 device registers.

    <B>Description</B>\n
    The FX3 device hardware implements a number of Control and Status registers that
    govern the behavior of and report the current status of each of the blocks. This
    function is used to read one or more contiguous registers from the register space
    of the FX3 device.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the register read(s) is/are successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the arguments passed in are erroneous.

    **\see
    *\see CyU3PWriteDeviceRegisters
 */
extern CyU3PReturnStatus_t
CyU3PReadDeviceRegisters (
        uvint32_t *regAddr,             /**< Address of first register to be read. */
        uint8_t    numRegs,             /**< Number of registers to be read. */
        uint32_t  *dataBuf              /**< Pointer to data buffer into which the registers are to be read. */
        );

/** \brief Function to write one or more FX3 device registers.

    <B>Description</B>\n
    This function is used to write one or more contiguous registers from the register
    space of the FX3 device.

    <B>Note</B>\n
    Use this function with caution and preferably under guidance from Cypress support
    personnel. The function does not implement any validity/side effect checks on the
    values being written into the registers.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the register write(s) is/are successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the arguments passed in are erroneous.

    **\see
    *\see CyU3PReadDeviceRegisters
 */
extern CyU3PReturnStatus_t
CyU3PWriteDeviceRegisters (
        uvint32_t *regAddr,             /**< Address of first register to be written. */
        uint8_t    numRegs,             /**< Number of registers to be written. */
        uint32_t  *dataBuf              /**< Pointer to data buffer containing data to be written to the registers. */
        );

/** @cond DOXYGEN_HIDE */
#ifndef CY_USE_ARMCC

/**< \brief Insert a single NOP instruction.

     <B>Description</B>\n
     This function inserts a single NOP instruction. This is a replacement for the __nop intrinsic
     that is provided by the ARM RVCT tool-chain. This is made an inline assembly macro to avoid
     un-necessary delays due to function calls.
 */
#define __nop()                                                                         \
{                                                                                       \
    __asm__ __volatile__                                                                \
    (                                                                                   \
        "mov r0, r0\n\t"                                                                \
    );                                                                                  \
}

/**< \brief Disable interrupts at the CPU level.

     <B>Description</B>\n
     This function disables interrupts using the CPSR register on the ARM CPU. This is a replacement
     for the __disable_irq intrinsic provided by the ARM RVCT tool-chain. This is made an inline
     assembly macro to avoid un-necessary delays due to function calls.
 */
#define __disable_irq()                                                                 \
{                                                                                       \
    __asm__ __volatile__                                                                \
        (                                                                               \
         "stmdb sp!, {r0}\n\t"          /* Save R0 value on the stack. */               \
         "mrs   r0, CPSR\n\t"           /* Read current CPSR value. */                  \
         "orr   r0, r0, #0xC0\n\t"      /* Disable IRQ and FIQ. */                      \
         "msr   CPSR_c, r0\n\t"         /* Write back to disable interrupts. */         \
         "ldmia sp!, {r0}\n\t"          /* Restore R0 register content. */              \
        );                                                                              \
}

/**< \brief Enable interrupts at the CPU level.

     <B>Description</B>\n
     This function enables interrupts using the CPSR register on the ARM CPU. This is a replacement
     for the __enable_irq intrinsic provided by the ARM RVCT tool-chain. This is made an inline
     assembly macro to avoid un-necessary delays due to function calls.
 */
#define __enable_irq()                                                                  \
{                                                                                       \
    __asm__ __volatile__                                                                \
        (                                                                               \
         "stmdb sp!, {r0}\n\t"          /* Save R0 value on the stack. */               \
         "mrs   r0, CPSR\n\t"           /* Read current CPSR value. */                  \
         "bic   r0, r0, #0xC0\n\t"      /* Enable IRQ and FIQ. */                       \
         "msr   CPSR_c, r0\n\t"         /* Write back to disable interrupts. */         \
         "ldmia sp!, {r0}\n\t"          /* Restore R0 register content. */              \
        );                                                                              \
}

#endif /* CY_USE_ARMCC */

/** @endcond */

#include "cyu3externcend.h"

#endif /* _INCLUDED_CYU3P_UTILS_H_ */

/*[]*/

