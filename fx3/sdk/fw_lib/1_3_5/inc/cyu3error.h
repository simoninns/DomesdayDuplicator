/*
 ## Cypress FX3 Core Library Header (cyu3error.h)
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

#ifndef _INCLUDED_CYU3ERROR_H_
#define _INCLUDED_CYU3ERROR_H_

#include "cyu3types.h"
#include "cyu3externcstart.h"

/** \file cyu3error.h
    \brief This file lists the error codes that are returned by the firmware library functions.

    \section errorCode Error Codes
    The following return codes are used by the FX3 APIs to indicate success/failure and the
    cause of error in case of failure.
 */

/** \brief List of error codes defined and returned by the FX3 firmware library.

    <B>Description</B>\n
    The error codes below help identify the cause of the failure. These errors
    could be returned by the base RTOS or by the FX3 API itself. The initial error codes
    are all defined and returned by the RTOS. The API specific error code start from
    CY_U3P_ERROR_BAD_ARGUMENT.
 */
typedef enum CyU3PErrorCode_t
{
    CY_U3P_SUCCESS = 0,                 /**< Success code */

    CY_U3P_ERROR_DELETED,               /**< The OS object being accessed has been deleted. */
    CY_U3P_ERROR_BAD_POOL,              /**< Bad memory pool passed to a function. */
    CY_U3P_ERROR_BAD_POINTER,           /**< Bad (NULL or unaligned) pointer passed to a function. */
    CY_U3P_ERROR_INVALID_WAIT,          /**< Non-zero wait requested from interrupt context. */
    CY_U3P_ERROR_BAD_SIZE,              /**< Invalid size value passed into a function. */
    CY_U3P_ERROR_BAD_EVENT_GRP,         /**< Invalid event group passed into a function. */
    CY_U3P_ERROR_NO_EVENTS,             /**< Failed to set/get the event flags specified. */
    CY_U3P_ERROR_BAD_OPTION,            /**< Invalid task option value specified for the function. */
    CY_U3P_ERROR_BAD_QUEUE,             /**< Invalid message queue passed to a function. */
    CY_U3P_ERROR_QUEUE_EMPTY,           /**< The message queue being read is empty. */
    CY_U3P_ERROR_QUEUE_FULL,            /**< The message queue being written to is full. */
    CY_U3P_ERROR_BAD_SEMAPHORE,         /**< Invalid semaphore pointer passed to a function. */
    CY_U3P_ERROR_SEMGET_FAILED,         /**< A semaphore get operation failed. */
    CY_U3P_ERROR_BAD_THREAD,            /**< Invalid thread pointer passed to a function. */
    CY_U3P_ERROR_BAD_PRIORITY,          /**< Invalid thread priority value passed to a function. */
    CY_U3P_ERROR_MEMORY_ERROR,          /**< Failed to allocate memory. */
    CY_U3P_ERROR_DELETE_FAILED,         /**< Failed to delete an object because it is not idle. */
    CY_U3P_ERROR_RESUME_FAILED,         /**< Failed to resume a thread. */
    CY_U3P_ERROR_INVALID_CALLER,        /**< OS function failed because the current caller is not allowed. */
    CY_U3P_ERROR_SUSPEND_FAILED,        /**< Failed to suspend a thread. */
    CY_U3P_ERROR_BAD_TIMER,             /**< Invalid timer pointer passed to a function. */
    CY_U3P_ERROR_BAD_TICK,              /**< Invalid (0) tick value passed to a timer function. */
    CY_U3P_ERROR_ACTIVATE_FAILED,       /**< Failed to activate a timer. */
    CY_U3P_ERROR_BAD_THRESHOLD,         /**< Invalid thread pre-emption threshold value specified. */
    CY_U3P_ERROR_SUSPEND_LIFTED,        /**< Thread suspension was cancelled. */
    CY_U3P_ERROR_WAIT_ABORTED,          /**< Wait operation was aborted. */
    CY_U3P_ERROR_WAIT_ABORT_FAILED,     /**< Failed to abort wait operation on a thread. */
    CY_U3P_ERROR_BAD_MUTEX,             /**< Invalid Mutex pointer passed to a function. */
    CY_U3P_ERROR_MUTEX_FAILURE,         /**< Failed to get a mutex. */
    CY_U3P_ERROR_MUTEX_PUT_FAILED,      /**< Failed to put a mutex because it is not currently owned. */
    CY_U3P_ERROR_INHERIT_FAILED,        /**< Error in priority inheritance. */
    CY_U3P_ERROR_NOT_IDLE,              /**< Operation failed because relevant object is not idle or done. */

    CY_U3P_ERROR_BAD_ARGUMENT = 0x40,   /**< One or more parameters to a function are invalid. */
    CY_U3P_ERROR_NULL_POINTER,          /**< A null pointer has been passed in unexpectedly. */
    CY_U3P_ERROR_NOT_STARTED,           /**< The object/module being referred to has not been started. */
    CY_U3P_ERROR_ALREADY_STARTED,       /**< An object/module that is already active is being started. */
    CY_U3P_ERROR_NOT_CONFIGURED,        /**< Object/module referred to has not been configured. */
    CY_U3P_ERROR_TIMEOUT,               /**< Timeout on relevant operation. */
    CY_U3P_ERROR_NOT_SUPPORTED,         /**< Operation requested is not supported in current mode. */
    CY_U3P_ERROR_INVALID_SEQUENCE,      /**< Invalid function call sequence. */
    CY_U3P_ERROR_ABORTED,               /**< Function call failed as it was aborted by another thread/isr. */
    CY_U3P_ERROR_DMA_FAILURE,           /**< DMA engine failed to completed requested operation. */
    CY_U3P_ERROR_FAILURE,               /**< Failure due to a non-specific system error. */
    CY_U3P_ERROR_BAD_INDEX,             /**< Bad index value was passed in as parameter. Ex: for string descriptor. */
    CY_U3P_ERROR_BAD_ENUM_METHOD,       /**< Bad enumeration method specified. */
    CY_U3P_ERROR_INVALID_CONFIGURATION, /**< Invalid configuration specified. */
    CY_U3P_ERROR_CHANNEL_CREATE_FAILED, /**< Internal DMA channel creation failed. */
    CY_U3P_ERROR_CHANNEL_DESTROY_FAILED,/**< Internal DMA channel destroy failed. */
    CY_U3P_ERROR_BAD_DESCRIPTOR_TYPE,   /**< Invalid descriptor type specified. */
    CY_U3P_ERROR_XFER_CANCELLED,        /**< USB transfer was cancelled. */
    CY_U3P_ERROR_FEATURE_NOT_ENABLED,   /**< When a USB feature like remote wakeup is not enabled. */
    CY_U3P_ERROR_STALLED,               /**< When a USB request / data transfer is stalled. */
    CY_U3P_ERROR_BLOCK_FAILURE,         /**< The block accessed has a fatal error and needs to be re-initialized. */
    CY_U3P_ERROR_LOST_ARBITRATION,      /**< Loss of bus arbitration, invalid bus behaviour or bus busy.  */
    CY_U3P_ERROR_STANDBY_FAILED,        /**< Failed to enter standby mode because one or more wakeup events are
                                             active. */

    CY_U3P_ERROR_INVALID_VOLTAGE_RANGE = 0x60,  /**< Storage device's voltage range does not meet FX3S requirements. */
    CY_U3P_ERROR_CARD_WRONG_RESPONSE,           /**< Incorrect response received from storage device. */
    CY_U3P_ERROR_UNSUPPORTED_CARD,              /**< Storage device features are not supported by FX3S host. */
    CY_U3P_ERROR_CARD_WRONG_STATE,              /**< Storage device failed to move to expected state. */
    CY_U3P_ERROR_CMD_NOT_SUPPORTED,             /**< Storage device failed to support required commands. */
    CY_U3P_ERROR_CRC,                           /**< Response CRC error detected. */
    CY_U3P_ERROR_INVALID_ADDR,                  /**< Out of range address provided for read/write/erase access. */
    CY_U3P_ERROR_INVALID_UNIT,                  /**< Non-existent storage partition selected for transfer. */
    CY_U3P_ERROR_INVALID_DEV,                   /**< Access to port with no connected storage device. */
    CY_U3P_ERROR_ALREADY_PARTITIONED,           /**< Request to partition a device which is already partitioned. */
    CY_U3P_ERROR_NOT_PARTITIONED,               /**< Request to remove partitions on an unpartitioned device. */
    CY_U3P_ERROR_BAD_PARTITION,                 /**< Incorrect partition selected. */
    CY_U3P_ERROR_READ_WRITE_ABORTED,            /**< Read/write transfer was aborted (user cancellation or timeout). */
    CY_U3P_ERROR_WRITE_PROTECTED,               /**< Write request addressed to a write protected storage device. */
    CY_U3P_ERROR_SIB_INIT,                      /**< Storage driver initialization failed. */
    CY_U3P_ERROR_CARD_LOCKED,                   /**< Access to password locked SD card. */
    CY_U3P_ERROR_CARD_LOCK_FAILURE,             /**< Failure while locking/unlocking the SD card. */
    CY_U3P_ERROR_CARD_FORCE_ERASE,              /**< Failure during SD force erase operation. */
    CY_U3P_ERROR_INVALID_BLOCKSIZE,             /**< Block size specified for SDIO device is not supported. */
    CY_U3P_ERROR_INVALID_FUNCTION,              /**< Non-existent SDIO function being accessed. */
    CY_U3P_ERROR_TUPLE_NOT_FOUND,               /**< Non-existent tuple of SDIO card being accessed. */
    CY_U3P_ERROR_IO_ABORTED,                    /**< IO operation to SDIO card aborted. */
    CY_U3P_ERROR_IO_SUSPENDED,		        /**< IO operation to SDIO card suspended. */
    CY_U3P_ERROR_ILLEGAL_CMD,                   /**< Invalid command sent to the SDIO card. */
    CY_U3P_ERROR_SDIO_UNKNOWN,                  /**< Generic error reported by SDIO card. */
    CY_U3P_ERROR_BAD_CMD_ARG,                   /**< SDIO command argument is out of range. */
    CY_U3P_ERROR_UNINITIALIZED_FUNCTION,        /**< Access to uninitialized SDIO function. */
    CY_U3P_ERROR_CARD_NOT_ACTIVE,               /**< Access to SDIO card which is not active. */
    CY_U3P_ERROR_DEVICE_BUSY,                   /**< The storage device is busy handling another request. */
    CY_U3P_ERROR_NO_METADATA,			/**< No metadata present on card */
    CY_U3P_ERROR_CARD_UNHEALTHY,		/**< Card RD/WR Threshold error crossed */
    CY_U3P_ERROR_MEDIA_FAILURE,			/**< Card not responding to read/write transactions */

    CY_U3P_ERROR_NO_REENUM_REQUIRED = 0xFE, /**< FX3 booter supports the NoReEnumeration feature that enables to have 
                                                 a single USB enumeration across the FX3 booter and the final
                                                 application. This error code is returned by CyU3PUsbStart ()
                                                 to indicate that the NoReEnumeration is successful and the
                                                 sequence followed for the regular enumeration post CyU3PUsbStart ()
                                                 is to be skipped. */
    CY_U3P_ERROR_OPERN_DISABLED = 0xFF      /**< The requested feature/operation is not enabled in the current
                                                 configuration. */
} CyU3PErrorCode_t;

#include "cyu3externcend.h"

#endif /* _INCLUDED_CYU3ERROR_H_ */

/*[]*/

