/*
 ## Cypress FX3 Core Library Header (cyu3sibpp.h)
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

#ifndef _INCLUDED_CYU3SIBPP_H_
#define _INCLUDED_CYU3SIBPP_H_

#include <cyu3sib.h>
#include <cyu3externcstart.h>

/** \file cyu3sibpp.h
    \brief This is an FX3S internal header file that defines some global data structures
    that are used by the storage driver module.
 */

/************************************************************************************/
/********************************DATATYPES*******************************************/
/************************************************************************************/

#define CY_U3P_SIB_STACK_SIZE               (0x800) /**< Size of stack used by driver thread: 2 KB. */
#define CY_U3P_SIB_THREAD_PRIORITY          (4)     /**< Priority of driver thread. */

#define CY_U3P_SIB_EVT_DRVENTRY             (1 << 4)    /**< Event id used to signal thread to start executing. */
#define CY_U3P_SIB_EVT_PORT_POS             (5)         /**< Position of port specified event bit. */
#define CY_U3P_SIB_EVT_PORT_0               (1 << 5)    /**< Event notifying an interrupt on port 0. */
#define CY_U3P_SIB_EVT_PORT_1               (1 << 6)    /**< Event notifying an interrupt on port 1. */
#define CY_U3P_SIB_EVT_ABORT                (1 << 7)    /**< Event notifying an abort request. */
#define CY_U3P_SIB_TIMER0_EVT		    (1 << 15)   /**< Event notifying a write timeout on port 0. */
#define CY_U3P_SIB_TIMER1_EVT	            (1 << 16)   /**< Event notifying a write timeout on port 1. */
#define CY_U3P_SIB_IDLE_TIMER0_EVT	    (1 << 17)   /**< Event notifying a idle timeout on port 0. */
#define CY_U3P_SIB_IDLE_TIMER1_EVT	    (1 << 18)   /**< Event notifying a idle timeout on port 1. */

#define CY_U3P_PARTITION_TYPE_DATA_AREA     0xDA        /**< Partition type definition for user data area. */
#define CY_U3P_PARTITION_TYPE_AP_BOOT       0xAB        /**< Partition type definition for external processor boot. */
#define CY_U3P_PARTITION_TYPE_BENICIA_BOOT  0xBB        /**< Partition type definition for FX3S boot. */
#define CY_U3P_BOOT_PARTITION_NUM_BLKS      0x08        /**< Number of blocks reserved for partition header. */

/** \brief Enumeration identifying partitions on storage peripherals.
 */
typedef enum
{
    CY_U3P_SIB_DEV_PARTITION_0 = 0, /**< Device partition 0 */
    CY_U3P_SIB_DEV_PARTITION_1,     /**< Device partition 1 */
    CY_U3P_SIB_DEV_PARTITION_2,     /**< Device partition 2 */
    CY_U3P_SIB_DEV_PARTITION_3,     /**< Device partition 3 */
    CY_U3P_SIB_NUM_PARTITIONS       /**< Maximum paritions allowed */
} CyU3PSibDevPartition;

/** \def CyU3PSibEnableCoreIntr
    \brief Enable the core interrupt for the specified storage port.
 */
#define CyU3PSibEnableCoreIntr(portId) \
{\
    if ((portId) == CY_U3P_SIB_PORT_0) \
    { \
        CyU3PVicEnableInt (CY_U3P_VIC_SIB0_CORE_VECTOR); \
    } \
    else \
    { \
        CyU3PVicEnableInt (CY_U3P_VIC_SIB1_CORE_VECTOR); \
    } \
}

/** \def CyU3PSibDisableCoreIntr
    \brief Disable the core interrupt for the specified storage port.
 */
#define CyU3PSibDisableCoreIntr(portId) \
{\
    if ((portId) == CY_U3P_SIB_PORT_0) \
    { \
        CyU3PVicDisableInt (CY_U3P_VIC_SIB0_CORE_VECTOR); \
    } \
    else \
    { \
        CyU3PVicDisableInt (CY_U3P_VIC_SIB1_CORE_VECTOR); \
    } \
}

/** \brief Data structure that holds the storage driver state.

    <B>Description</B>\n
    This type defines a global data structure that holds all state information corresponding to the
    FX3S storage driver.
 */
typedef struct CyU3PSibGlobalData
{
    CyBool_t            isActive;                               /**< Driver is active or not. */
    CyBool_t            s0Enabled;                              /**< Whether S0 port is enabled by user. */
    CyBool_t            s1Enabled;                              /**< Whether S1 port is enabled by user. */
    CyU3PSibEvtCbk_t    sibEvtCbk;                              /**< Callback used for event notifications. */

    uint32_t            wrCommitSize[CY_U3P_SIB_NUM_PORTS];     /**< Size at which writes should be committed. */
    uint32_t            nextWrAddress[CY_U3P_SIB_NUM_PORTS];    /**< Address at which a write has been paused. */
    uint32_t            openWrSize[CY_U3P_SIB_NUM_PORTS];       /**< Size of currently open write operation. */
    CyBool_t            wrCommitPending[CY_U3P_SIB_NUM_PORTS];  /**< Whether write commit is pending. */
    CyU3PSibLunLocation activePartition[CY_U3P_SIB_NUM_PORTS];  /**< Partition on which transfer is happening. */
    uint32_t            partConfig[CY_U3P_SIB_NUM_PORTS];       /**< Partition config definition from device. */
    CyU3PDmaChannel     sibDmaChannel;                          /**< DMA channel used for device init. */
} CyU3PSibGlobalData;

extern CyU3PSibGlobalData glSibDevInfo;                         /**< Storage driver status. */

/** @cond DOXYGEN_HIDE */

/* Macros to GET/SET the global data structure fields. */
#define CyU3PIsSibActive()                      (glSibDevInfo.isActive)
#define CyU3PSetSibActive()                     (glSibDevInfo.isActive = CyTrue)
#define CyU3PClearSibActive()                   (glSibDevInfo.isActive = CyFalse)

#define CyU3PSibIsS0Enabled()                   (glSibDevInfo.s0Enabled)
#define CyU3PSibSetS0Enabled()                  (glSibDevInfo.s0Enabled = CyTrue)
#define CyU3PSibClearS0Enabled()                (glSibDevInfo.s0Enabled = CyFalse)

#define CyU3PSibIsS1Enabled()                   (glSibDevInfo.s1Enabled)
#define CyU3PSibSetS1Enabled()                  (glSibDevInfo.s1Enabled = CyTrue)
#define CyU3PSibClearS1Enabled()                (glSibDevInfo.s1Enabled = CyFalse)

#define CyU3PSibSetEventCallback(cb)            (glSibDevInfo.sibEvtCbk = (cb))
#define CyU3PSibGetEventCallback                (glSibDevInfo.sibEvtCbk)

#define CyU3PSibIsWrCommitPending(port)         (glSibDevInfo.wrCommitPending[(port)])
#define CyU3PSibSetWrCommitPending(port)        (glSibDevInfo.wrCommitPending[(port)] = CyTrue)
#define CyU3PSibClearWrCommitPending(port)      (glSibDevInfo.wrCommitPending[(port)] = CyFalse)

#define CyU3PSibGetWrCommitSize(port)           (glSibDevInfo.wrCommitSize[(port)])
#define CyU3PSibSetWrCommitSize(port,size)      (glSibDevInfo.wrCommitSize[(port)] = (size))

#define CyU3PSibGetNextWrAddress(port)          (glSibDevInfo.nextWrAddress[(port)])
#define CyU3PSibSetNextWrAddress(port,addr)     (glSibDevInfo.nextWrAddress[(port)] = (addr))

#define CyU3PSibGetOpenWrSize(port)             (glSibDevInfo.openWrSize[(port)])
#define CyU3PSibSetOpenWrSize(port,size)        (glSibDevInfo.openWrSize[(port)] = (size))

#define CyU3PSibGetActivePartition(port)        (glSibDevInfo.activePartition[(port)])
#define CyU3PSibSetActivePartition(port,loc)    (glSibDevInfo.activePartition[(port)] = (loc))

#define CyU3PSibGetPartitionConfig(port)        (glSibDevInfo.partConfig[(port)])
#define CyU3PSibSetPartitionConfig(port,cfg)    (glSibDevInfo.partConfig[(port)] = (cfg))

#define CyU3PSibGetDmaChannelHandle             (&(glSibDevInfo.sibDmaChannel))

/** @endcond */

#define CYU3P_SIB_INT_READ_SOCKET               (CY_U3P_SIB_SOCKET_6)   /**< Socket used by driver to read from devices. */
#define CYU3P_SIB_INT_WRITE_SOCKET              (CY_U3P_SIB_SOCKET_7)   /**< Socket used by driver to write to devices. */

/** \def CY_U3P_DMA_SIB_NUM_SCK
    \brief Number of storage sockets supported by FX3S.
 */
#define CY_U3P_DMA_SIB_NUM_SCK          (8)

/** @cond DOXYGEN_HIDE */

#define CY_U3P_DMA_SIB_MIN_CONS_SCK     (0)
#define CY_U3P_DMA_SIB_MAX_CONS_SCK     (CY_U3P_DMA_SIB_NUM_SCK - 1)
#define CY_U3P_DMA_SIB_MIN_PROD_SCK     (0)
#define CY_U3P_DMA_SIB_MAX_PROD_SCK     (CY_U3P_DMA_SIB_NUM_SCK - 1)

/* Macros to access the glSibLunInfo structure. */
#define CyU3PSibLunIsValid(port,lun)                    (glSibLunInfo[(port)][(lun)].valid != 0)
#define CyU3PSibLunGetStartAddr(port,lun)               (glSibLunInfo[(port)][(lun)].startAddr)
#define CyU3PSibLunGetNumBlocks(port,lun)               (glSibLunInfo[(port)][(lun)].numBlocks)
#define CyU3PSibLunGetLocation(port,lun)                (glSibLunInfo[(port)][(lun)].location)

#define CyU3PSibLunSetValid(port,lun)                   (glSibLunInfo[(port)][(lun)].valid = 1)
#define CyU3PSibLunClearValid(port,lun)                 (glSibLunInfo[(port)][(lun)].valid = 0)

#define CyU3PSibLunSetStartAddr(port,lun,addr)          (glSibLunInfo[(port)][(lun)].startAddr = (addr))
#define CyU3PSibLunSetNumBlocks(port,lun,cnt)           (glSibLunInfo[(port)][(lun)].numBlocks = (cnt))
#define CyU3PSibLunSetLocation(port,lun,loc)            (glSibLunInfo[(port)][(lun)].location = (loc))

/* Macros to access the glSibCtxt structure. */
#define CyU3PSibIsDeviceBusy(port)                      (glSibCtxt[(port)].inUse)
#define CyU3PSibSetDeviceBusy(port)                     (glSibCtxt[(port)].inUse = CyTrue)
#define CyU3PSibClearDeviceBusy(port)                   (glSibCtxt[(port)].inUse = CyFalse)

/** @endcond */

/** \brief List of storage interface DMA sockets.
 */
typedef enum
{
    CY_U3P_SIB_SOCKET0  = 0x0200,               /**< SIB socket #0. */
    CY_U3P_SIB_SOCKET1,                         /**< SIB socket #1. */
    CY_U3P_SIB_SOCKET2,                         /**< SIB socket #2. */
    CY_U3P_SIB_SOCKET3,                         /**< SIB socket #3. */
    CY_U3P_SIB_SOCKET4,                         /**< SIB socket #4. */
    CY_U3P_SIB_SOCKET5,                         /**< SIB socket #5. */
    CY_U3P_SIB_SOCKET_6,                        /**< S-port socket number 6: Reserved for driver usage. */
    CY_U3P_SIB_SOCKET_7                         /**< S-port socket number 7: Reserved for driver usage. */
} CyU3PSibSocketId_t;

/** \brief Structure that holds transfer state corresponding to each storage port.
 */
typedef struct CyU3PSibCtxt
{
    uint8_t             isRead;                 /**< Indicates if its a read or a write operation. */
    volatile uint8_t    inUse;                  /**< Variable to indicate if the card is being used. */
    uint8_t             partition;              /**< Flag indicating if the device is partitioned. */
    uint8_t             activeUnitId;           /**< The current active logical unit of the card */
    uint8_t             numBootLuns;            /**< Number of Logical UNits containing boot code. */
    uint8_t             numUserLuns;            /**< Number of user accessible logical units on the device. */
    CyU3PReturnStatus_t status;                 /**< Return status from internal API calls. */
    CyU3PMutex          mutexLock;              /**< Mutex lock for the port */
    CyU3PTimer          writeTimer;             /**< Sib Write Timeout Timer */
    CyU3PTimer          idleTimer;              /**< Sib Idle Timeout Timer */
    CyU3PTimerCb_t      writeTimerCbk;          /**< SIB Write Timer Call back function */
    CyU3PTimerCb_t      idleTimerCbk;           /**< SIB Idle Timer Call back function */
} CyU3PSibCtxt_t;


/************************************************************************************/
/******************************* GLOBAL VARIABLES ***********************************/
/************************************************************************************/
extern CyU3PThread          glSibThread;                                /**< Storage driver thread */
extern CyU3PEvent           glSibEvent;                                 /**< Event used to signal driver. */
extern CyU3PSibCtxt_t       glSibCtxt[CY_U3P_SIB_NUM_PORTS];            /**< Storage device/port information. */
extern CyU3PSibIntfParams_t glSibIntfParams[CY_U3P_SIB_NUM_PORTS];      /**< Per-port interface parameters. */
extern CyU3PSibLunInfo_t    glSibLunInfo[CY_U3P_SIB_NUM_PORTS][CY_U3P_SIB_NUM_PARTITIONS]; /**< Partition info. */
extern CyU3PSibEvtCbk_t     glSibEvtCbk;                                /**< SIB Event Call back function */

/** \brief Register information about a storage partition.

    <B>Description</B>\n
    This function stores information about a storage partition in the driver
    data structures.
 */
extern void
CyU3PSibUpdateLunInfo (
        uint8_t  portId,                        /**< Port on which storage device is connected. */
        uint8_t  partNum,                       /**< Partition number which is being updated. */
        uint8_t  partType,                      /**< Type of storage partition. */
        uint8_t  location,                      /**< Location of this storage partition (boot area or data area). */
        uint32_t startAddr,                     /**< Start address for this storage partition. */
        uint32_t partSize                       /**< Size of the partition in sectors. */
        );

/** \brief Initialize the storage device connected on the specified storage port.

    <B>Description</B>\n
    Initialize the storage device connected on the specified storage port.
 */
extern CyU3PReturnStatus_t
CyU3PSibInitCard (
        uint8_t portId                          /**< Port on which the device should be initialized. */
        );

#include <cyu3externcend.h>

#endif /* _INCLUDED_CYU3SIBPP_H_ */

/*[]*/

