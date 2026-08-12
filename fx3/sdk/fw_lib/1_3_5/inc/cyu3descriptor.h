/*
 ## Cypress FX3 Core Library Header (cyu3descriptor.h)
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

#ifndef _INCLUDED_CYU3DESCRIPTOR_H_
#define _INCLUDED_CYU3DESCRIPTOR_H_

#include "cyu3types.h"
#include "cyu3externcstart.h"

/** \file cyu3descriptor.h
    \brief DMA descriptor management.

    <B>Summary</B>\n
    DMA descriptors are data structures that keep track of the source/destinations
    and the memory buffers for a data transfer through the FX3 device. The firmware
    maintains a queue of DMA descriptors that are allocated as required and used by
    the corresponding firmware modules.
 
    This file defines the data structures and the interfaces for descriptor management.

    \section descfunc Descriptor Functions
    This section documents the utility functions that work with the DMA descriptors.
    These functions are supposed to be internal functions for the FX3 API library's
    use and are not expected to be called directly by the user application.

    If an application chooses to call these functions, extreme care must be taken
    to validate the parameters being passed; as these functions do not perform 
    any error checks. Passing in incorrect/invalid parameters can result in
    unpredictable behavior. In particular, the buffer address in the DMA 
    descriptor needs to be in system memory area. Any other buffer address can
    cause the entire DMA engine to freeze, requiring a full device reset.
 */

/**************************************************************************
 ******************************* Data types *******************************
 **************************************************************************/

/** \brief Descriptor data structure.

    <B>Description</B>\n
    This data structure contains the fields that make up a DMA descriptor on
    the FX3 device.

    Each structure member is composed of multiple fields as shown below. Refer
    to the sock_regs.h header file for the definitions used.

    buffer: (CY_U3P_BUFFER_ADDR_MASK)

    sync: (CY_U3P_EN_PROD_INT | CY_U3P_EN_PROD_EVENT | CY_U3P_PROD_IP_MASK |
        CY_U3P_PROD_SCK_MASK | CY_U3P_EN_CONS_INT | CY_U3P_EN_CONS_EVENT |
        CY_U3P_CONS_IP_MASK | CY_U3P_CONS_SCK_MASK)

    chain: (CY_U3P_WR_NEXT_DSCR_MASK | CY_U3P_RD_NEXT_DSCR_MASK)

    size: (CY_U3P_BYTE_COUNT_MASK | CY_U3P_BUFFER_SIZE_MASK | CY_U3P_BUFFER_OCCUPIED |
        CY_U3P_BUFFER_ERROR | CY_U3P_EOP | CY_U3P_MARKER)

    **\see\n
     *\see CyU3PDmaDscrGetConfig
     *\see CyU3PDmaDscrSetConfig
 */
typedef struct CyU3PDmaDescriptor_t
{
    uint8_t    *buffer;    /**< Pointer to buffer used. */
    uint32_t    sync;      /**< Consumer, Producer binding. */
    uint32_t    chain;     /**< Next descriptor links. */
    uint32_t    size;      /**< Current and maximum sizes of buffer. */
} CyU3PDmaDescriptor_t;

/**************************************************************************
 ********************** Global variable declarations **********************
 **************************************************************************/

extern CyU3PDmaDescriptor_t *glDmaDescriptor; /**< Pointer to list of DMA descriptors */

/**************************************************************************
 ********************************* Macros *********************************
 **************************************************************************/

/** \def CY_U3P_DMA_DSCR0_LOCATION
    \brief This is the location of the descriptor pool. As the DMA descriptors
    are fixed in the FX3 memory map by the device architecture, this definition
    should not be changed.
 */
#define CY_U3P_DMA_DSCR0_LOCATION   (0x40000000)

/** \def CY_U3P_DMA_DSCR_COUNT
    \brief This is the number of descriptors in the pool.
 */
#define CY_U3P_DMA_DSCR_COUNT       (512)

/** \def CY_U3P_DMA_DSCR_SIZE
    \brief This is the size of each descriptor in bytes. This value is defined
    by the FX3 device architecture.
 */
#define CY_U3P_DMA_DSCR_SIZE        (16)

/**************************************************************************
 *************************** Function prototypes **************************
 **************************************************************************/

/************************ Descriptor Pool Functions ***********************/

/** \brief Create and initialize the free descriptor list.

    <B>Description</B>\n
   This function initializes the free descriptor list, and marks all of the
   descriptors as available. This function is invoked internal to the library
   and is not expected to be called explicitly.

    <B>Return value</B>\n
   * None

    **\see\n
    *\see CyU3PDmaDscrListDestroy
 */
extern void
CyU3PDmaDscrListCreate (
        void);

/** \brief Destroy the free descriptor list.

    <B>Description</B>\n
   This function de-initializes the free descriptor list, and marks all of the
   descriptors as non-available. This function is invoked internal to the
    library and should not be called explicitly.

    <B>Return value</B>\n
   * None

    **\see\n
    *\see CyU3PDmaDscrListCreate
 */
extern void
CyU3PDmaDscrListDestroy (
        void);

/** \brief Get a descriptor number from the free list.

    <B>Description</B>\n
   This function searches the free list for the first available descriptor,
    and returns the index. This function is used by the DMA channel APIs,
    and can be used to do advanced DMA programming based on direct socket
    access.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if a NULL pointer is passed.\n
    * CY_U3P_ERROR_FAILURE - if no free descriptor is found.

    **\see\n
    *\see CyU3PDmaDscrPut
    *\see CyU3PDmaDscrGetFreeCount
 */
extern CyU3PReturnStatus_t
CyU3PDmaDscrGet (
        uint16_t *index_p       /**< Output parameter which is filled with the free descriptor index. */
        );

/** \brief Add a descriptor number to the free list.

    <B>Description</B>\n
   This function marks a descriptor as available in the free list.
    This function is used internally by the DMA channel APIs, and can be used
    to do advanced DMA programming based on direct socket access.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if an invalid index is passed.

    **\see\n
    *\see CyU3PDmaDscrGet
    *\see CyU3PDmaDscrGetFreeCount
 */
extern CyU3PReturnStatus_t
CyU3PDmaDscrPut (
        uint16_t index          /**< Descriptor index to be marked free. */
        );

/** \brief Get the number of free descriptors available.

    <B>Description</B>\n
    This function returns the number of free descriptors available for use.

    <B>Return value</B>\n
   * The number of free descriptors available.

    **\see\n
    *\see CyU3PDmaDscrGet
    *\see CyU3PDmaDscrPut
 */
extern uint16_t
CyU3PDmaDscrGetFreeCount (
        void);

/** \brief Update the DMA descriptor configuration.

    <B>Description</B>\n
    Update the specified DMA descriptor in FX3 memory with data from the descriptor
    structure passed in as parameter.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if an invalid index or a NULL pointer is passed.

    **\see\n
    *\see CyU3PDmaDscrGetConfig
 */
extern CyU3PReturnStatus_t
CyU3PDmaDscrSetConfig (
        uint16_t index,                 /**< Index of the descriptor to be updated. */
        CyU3PDmaDescriptor_t *dscr_p    /**< Pointer to descriptor structure containing the desired configuration. */
        );

/** \brief Get the descriptor configuration.

    <B>Description</B>\n
    Read the current contents of the specified DMA descriptor into the output descriptor
    data structure.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if an ivalid index or a NULL pointer is passed.

    **\see\n
    *\see CyU3PDmaDscrGetConfig
 */
extern CyU3PReturnStatus_t
CyU3PDmaDscrGetConfig (
        uint16_t              index,    /**< Index of the descriptor to read. */
        CyU3PDmaDescriptor_t *dscr_p    /**< Output parameter that will be filled with the descriptor values. */
        );

/** \brief Create a circular chain of descriptors.

    <B>Description</B>\n
   The function creates a chain of descriptors for DMA operations.
   Both the producer and consumer chains are created with the same
   values. The descriptor sync parameter is updated as provided.
    A DMA buffer is allocated if the bufferSize variable is non zero.
    This function is mainly used by the DMA channel APIs, and can also
    be used to customize descriptors and do advanced DMA operations.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_MEMORY_ERROR - if descriptor or DMA buffer allocation failed.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if number of descriptors required is zero OR the index is NULL.

    **\see\n
    *\see CyU3PDmaDscrChainDestroy
 */
extern CyU3PReturnStatus_t
CyU3PDmaDscrChainCreate (
        uint16_t *dscrIndex_p,  /**< Output parameter that specifies the index of the head descriptor in the
                                           chain. */
        uint16_t count,         /**< Number of descriptors required in the chain. */
        uint16_t bufferSize,    /**< Size of the buffers to be associated with each descriptor. If set to
                                           zero, no buffers will be allocated. */
        uint32_t dscrSync       /**< The sync field to be set in all descriptors. Specifies the producer
                                           and consumer socket information. */
        );

/** \brief Frees the previously created chain of descriptors.

    <B>Description</B>\n
    The function frees the chain of descriptors. This function must be invoked
    only after suspending or disabling the corresponding DMA sockets. The buffers
    pointed to by the descriptors can be optionally freed. This function is mainly
    used by the DMA channel APIs, and can also be used to do advanced DMA programming.

    <B>Return value</B>\n
    * None

    **\see\n
    *\see CyU3PDmaDscrChainDestroy
 */
void
CyU3PDmaDscrChainDestroy (
        uint16_t dscrIndex,     /**< Index of the head descriptor in the chain. */
        uint16_t count,         /**< Number of descriptors in the chain. */
        CyBool_t isProdChain,   /**< Specifies whether to traverse the producer chain or the consumer chain
                                           to get the next descriptor. */
        CyBool_t freeBuffer     /**< Whether the DMA buffers associated with the descriptors should be freed. */
        );

#include "cyu3externcend.h"

#endif /* _INCLUDED_CYU3DESCRIPTOR_H_ */

/*[]*/

