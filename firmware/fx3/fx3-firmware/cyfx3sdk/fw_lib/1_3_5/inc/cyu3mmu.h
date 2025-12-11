/*
 ## Cypress FX3 Core Library Header (cyu3mmu.h)
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

#ifndef _INCLUDED_CYU3PMMU_H_
#define _INCLUDED_CYU3PMMU_H_

/** \file cyu3mmu.h
    \brief Cache and Memory Management for the FX3 firmware.

    <B>Description</B>\n
   The FX3 device has 8KB of I-cache and 8KB of D-cache. The default
   cache handling is done internal to the library. The cache and MMU are
   initialized. The cache control should be done using CyU3PDeviceCacheControl
   API. This section explains the extended cache functions that can be
   used for customized / advanced cache handling. Most of these functions are
   standard implementations for the ARM926EJ-S core.

   If the data cache is enabled, then all buffers used for DMA operation
   has to be 32 byte aligned and 32 byte multiple. This is because the size
   of the cache line in the core is 32 bytes. The DMA buffers include all 
   buffers used with DMA APIs as well as used with library APIs like
   CyU3PUsbSetDesc, CyU3PUsbSendEP0Data, CyU3PUsbGetEP0Data,
   CyU3PUsbHostSendSetupRqt etc.
 */

#include "cyu3types.h"
#include "cyu3externcstart.h"

#define CYU3P_ITCM_BASE_ADDR          (0x00000000)      /**< I-TCM base address. */
#define CYU3P_ITCM_SIZE               (0x00004000)      /**< I-TCM size in bytes. */
#define CYU3P_ITCM_SZ_EN              (0x00000015)      /**< I-TCM size in the format for cp15:c9 */

#define CYU3P_DTCM_BASE_ADDR          (0x10000000)      /**< D-TCM base address. */
#define CYU3P_DTCM_SIZE               (0x00002000)      /**< D-TCM size in bytes. */
#define CYU3P_DTCM_SZ_EN              (0x00000011)      /**< D-TCM size in the format for cp15:c9 */

#define CYU3P_SYSMEM_BASE_ADDR        (0x40000000)      /**< SYSMEM base address. */
#define CYU3P_SYSMEM_SIZE             (0x00080000)      /**< SYSMEM size in bytes. */

#define CYU3P_MMIO_BASE_ADDR          (0xE0000000)      /**< MMIO base address. */
#define CYU3P_MMIO_SIZE               (0x10000000)      /**< MMIO size in bytes. */

#define CYU3P_ROM_BASE_ADDR           (0xF0000000)      /**< BootROM base address. */
#define CYU3P_ROM_SIZE                (0x00008000)      /**< BootROM size in bytes. */

#define CYU3P_VIC_BASE_ADDR           (0xFFFFF000)      /**< VIC base address. */
#define CYU3P_VIC_SIZE                (0x00001000)      /**< VIC size in bytes. */

#define CYU3P_GCTL_PAGE_TABLE_ADDR    (0xE0058000)      /**< Address of MMIO mapped page table. */

#define CYU3P_CACHE_LINE_SZ           (5)               /**< Log(2) of cache line size in bytes. */
#define CYU3P_CACHE_SIZE              (13)              /**< Log(2) of cache size in bytes. */
#define CYU3P_CACHE_NWAYS             (2)               /**< Log(2) of number of cache ways. */

/** \def CYU3P_CACHE_WAY_SZ
    \brief Log(2) of the size of a cache way in bytes.
 */
#define CYU3P_CACHE_WAY_SZ            (CYU3P_CACHE_SIZE - CYU3P_CACHE_NWAYS - CYU3P_CACHE_LINE_SZ)

#define CYU3P_ICACHE_EN_MASK          (0x1000)          /**< Mask for I-Cache enable bit in cp15:c1 */
#define CYU3P_DCACHE_EN_MASK          (0x04)            /**< Mask for D-Cache enable bit in cp15:c1 */
#define CYU3P_MMU_EN_MASK             (0x01)            /**< Mask for MMU enable bit in cp15:c1 */
#define CYU3P_CACHE_MMU_EN_MASK       (0x1005)          /**< Mask for Cache and MMU enable bits. */
#define CYU3P_CACHE_REPLACEMENT_MASK  (0x4000)          /**< Mask for Cache replacement policy bit in cp15:c1 */
#define CYU3P_TCMREG_ADDRESS_MASK     (0xFFFFF000)      /**< Mask for TCM base address in cp15:c9 */

/** \brief Disable the instruction cache.

    <B>Description</B>\n
    Function to disable the Instruction Cache. Should be called from a privileged mode,
    after ensuring that the I-Cache has been flushed.  This function should not be
    explicitly called. The CyU3PDeviceCacheControl API should be called instead. The
    I-cache is kept disabled by default at firmware start-up.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PSysDisableCacheMMU
    *\see CyU3PSysEnableICache
 */
extern void
CyU3PSysDisableICache (
        void);

/** \brief Disable the data cache.

    <B>Description</B>\n
    Function to disable the Data Cache. Should be called from a privileged mode,
    after ensuring that the D-Cache has been cleaned and flushed. This function
    should not be explicitly called. The CyU3PDeviceCacheControl API should be
    called instead. The D-cache is kept disabled by default at firmware start-up.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PSysDisableCacheMMU
    *\see CyU3PSysEnableDCache
 */
extern void
CyU3PSysDisableDCache (
        void);

/** \brief Disable the MMU.

    <B>Description</B>\n
    Function to disable the Memory Management Unit. All memory will be accessed
    through physical addresses once MMU has been disabled. Caller should ensure
    that caches have been flushed and disabled before disabling the MMU. This
    function should not be explicitly called and is invoked from the library.
    This function is provided for debug purposes only.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PSysDisableCacheMMU
    *\see CyU3PSysEnableMMU
 */
extern void
CyU3PSysDisableMMU (
        void);

/** \brief Disable the caches and MMU.

    <B>Description</B>\n
    Function to disable the Instruction and Data caches as well as the MMU. This
    function should not be explicitly called and is invoked from the library during
    initialization. It is provided for debug purposes only.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PSysDisableCacheMMU
    *\see CyU3PSysDisableDCache
    *\see CyU3PSysDisableICache
 */
extern void
CyU3PSysDisableCacheMMU (
        void);

/** \brief Enable the instruction cache.

    <B>Description</B>\n
    Function to enable the instruction cache. The function also sets up both caches
    to use random replacement strategy. This function should not be explicitly called.
    The CyU3PDeviceCacheControl API should be called instead. The function is available
    for debug purposes.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PSysDisableICache
    *\see CyU3PSysEnableCacheMMU
 */
extern void
CyU3PSysEnableICache (
        void);

/** \brief Enable the Data Cache.

    <B>Description</B>\n
    Function to enable the Data Cache. The MMU must also be enabled for the
    D-Cache to function properly. This function should not be explicitly called.
    The CyU3PDeviceCacheControl API should be called instead.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PSysDisableDCache
    *\see CyU3PSysEnableCacheMMU
 */
extern void
CyU3PSysEnableDCache (
        void);

/** \brief Enable the MMU.

    <B>Description</B>\n
    Function to enable the MMU. The page tables should be setup before calling this
    function. This function should not be explicitly called and is invoked from the
    library during initialization. It is provided for debug purposes.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PSysDisableMMU
    *\see CyU3PSysEnableCacheMMU
 */
extern void
CyU3PSysEnableMMU (
        void);

/** \brief Enable the Caches and the MMU.

    <B>Description</B>\n
    Function to enable the Instruction and Data caches as well as the MMU. Sets up
    both caches to use random replacement strategy. This function should not be
    explicitly called and is invoked from the library during initialization. It is
    provided for debug purposes.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PSysDisableCacheMMU
    *\see CyU3PSysEnableICache
    *\see CyU3PSysEnableDCache
    *\see CyU3PSysEnableMMU
 */
extern void
CyU3PSysEnableCacheMMU (
        void);

/** \brief Flush both I and D Caches.

    <B>Description</B>\n
   Function to flush both the Instruction and Data Caches. The caller
   is responsible for ensuring that the Data cache is clean.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3PSysFlushCaches (
        void);

/** \brief Flush the I-Cache.

    <B>Description</B>\n
   Function to flush the Instruction Cache.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3PSysFlushICache (
        void);

/** \brief Flush the D-Cache.

    <B>Description</B>\n
   Function to flush the Data Cache. The caller should ensure that the
   cache has been cleaned before calling this function.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3PSysFlushDCache (
        void);

/** \brief Clean the entire D-Cache.

    <B>Description</B>\n
   Function to clean the entire D-Cache. The Test/Clean functionality
   of the Arm 926 core is used here. The Cache is not flushed by this
   function.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3PSysCleanDCache (
        void);

/** \brief Clean and Flush the entire D-Cache.

    <B>Description</B>\n
   Function to clean and flush the entire Data cache using the test/clean
   command.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3PSysClearDCache (
        void);

/** \brief Flush a code region from the I-Cache.

    <B>Description</B>\n
   Function to remove a specified code region from the Instruction Cache.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3PSysFlushIRegion (
        uint32_t *addr,                 /**< Start address of the region to flush. */
        uint32_t  len                   /**< Length of the region in bytes. */
        );

/** \brief Flush a data region from the D-Cache.

    <B>Description</B>\n
   Function to flush a specified data region from the Data Cache.
   If the D-cache is enabled and the cache is handled by the application,
   then this API should be invoked before receiving any data using DMA.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3PSysFlushDRegion (
        uint32_t *addr,                 /**< Start address of the region to clean. */
        uint32_t  len                   /**< Length of the region in bytes. */
        );

/** \brief Clean a data region from the D-Cache.

    <B>Description</B>\n
   Function to clean a specified data region from the Data Cache.
   If the D-cache is enabled and the cache is handled by the application,
   then this API should be invoked before sending any data using DMA.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3PSysCleanDRegion (
        uint32_t *addr,                 /**< Start address of the region to clean. */
        uint32_t  len                   /**< Length of the region in bytes. */
        );

/** \brief Clean and flush a data region from the D-Cache.

    <B>Description</B>\n
   Function to clean and flush a specified data region from the Data Cache.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3PSysClearDRegion (
        uint32_t *addr,                 /**< Start address of the region to flush. */
        uint32_t  len                   /**< Length of the region in bytes. */
        );

/** \brief Memory barrier synchronization function.

    <B>Description</B>\n
   Function that ensures that all write buffers to memory have been
   drained. This call can be used as a synchronization barrier.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3PSysBarrierSync (
        void);

/** \brief Function to lock a code region into the I-Cache.

    <B>Description</B>\n
   Function to load a specified code region into the I-cache and lock
   it. A maximum of 3 lock operations can be called, with the size of
   the code region limited to under 2 KB in each case. The caller should
   ensure that the I-Cache has been flushed, to avoid the possibility
   of the content already being in the cache.

    <B>Return value</B>\n
    * The way index into which the region was locked.
 */
extern uint32_t
CyU3PSysCacheIRegion (
        uint32_t *addr,                         /**< Start address of the region to cache. */
        uint32_t  len                           /**< Length of the region in bytes. */
        );

/** \brief Function to lock a data region into the D-Cache.

    <B>Description</B>\n
   Function to load a specified data region into the D-cache and lock
   it. A maximum of 3 lock operations can be called, with the size of
   the data region limited to under 2 KB in each case. The caller should
   ensure that the D-Cache has been flushed, to avoid the possibility
   of the content already being in the cache.

    <B>Return value</B>\n
    * The way index into which the region was locked.
 */
extern uint32_t
CyU3PSysCacheDRegion (
        uint32_t *addr,                         /**< Start address of the region to cache. */
        uint32_t  len                           /**< Length of the region in bytes. */
        );

/** \brief Function to initialize the TCM regions.

    <B>Description</B>\n
    This function enables the TCMs by writing to the TCM region registers. This
    function should not be explicitly invoked. The library calls this function
    during initialization. It is provided for debug purposes.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3PSysInitTCMs (
        void);

/** \brief Function to create the page tables for the device memory map.

    <B>Description</B>\n
   This function uses the GCTL registers to create a one-level (section
   entries only) page table for the FX3 device memory map. The function
   is not expected to be explicitly invoked and is called by the library
   during initialization. It is provided for debug purposes.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3PInitPageTable (
        void);

/** \brief Function to lock the TLB entry for a page walk.

    <B>Description</B>\n
   This function loads the page table walk information corresponding to
   a specified address into the TLB and locks it. This function is should
   not be called explicitly and is provided for debug purposes.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3PSysLockTLBEntry (
        uint32_t *addr                          /**< Address range whose TLB entry is to be locked in place. */
        );

/** \brief Invalidate the TLB entry corresponding to a specified MVA.

    <B>Description</B>\n
   This function invalidates one of the TLB entries corresponding to the
   specified Modified Virtual Address. Even locked down entries will be
   invalidated. Multiple calls of this function may be needed if multi-level
   page tables are in use. This function is should not be called explicitly
   and is provided for debug purposes.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3PSysFlushTLBEntry (
        uint32_t *addr                          /**< Address whose TLB entry is to be invalidated. */
        );

/** \brief Function to lock all valid page walks into TLB.

    <B>Description</B>\n
    This function locks the page table walk information for all valid addresses
    on the FX3 device into the TLB. This function should be called after
    CyU3PInitPageTable has been called. This function is should not be called
    explicitly and is provided for debug purposes.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3PSysLoadTLB (
        void);

#include "cyu3externcend.h"

#endif /* _INCLUDED_CYU3PMMU_H_ */

/*[]*/

