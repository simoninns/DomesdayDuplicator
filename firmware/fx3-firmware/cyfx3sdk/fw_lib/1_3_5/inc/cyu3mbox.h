/*
 ## Cypress FX3 Core Library Header (cyu3mbox.h)
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

#ifndef _INCLUDED_CYU3_MBOX_H_
#define _INCLUDED_CYU3_MBOX_H_

#include "cyu3types.h"
#include "cyu3externcstart.h"

/** \file cyu3mbox.h
    \brief The mailbox handler is responsible for sending/receiving short messages
    from an external processor through the mailbox registers.

    <B>Description</B>\n
    The FX3 device implements a set of mailbox registers that can be used to
    exchange short general purpose messages between FX3 and the external device
    connected on the P-port. Messages of upto 8 bytes can be sent in each
    direction at a time. The mailbox handler is responsible for handling mailbox
    data in both directions.
 */

/**************************************************************************
 ******************************* Data Types *******************************
 **************************************************************************/

/** \brief Structure that holds a packet of mailbox data.

    <B>Description</B>\n
    The FX3 device has 8 byte mailbox registers that can be used when the
    P-port mode is enabled. This structure represents the eight bytes to be
    written to or read from the corresponding mailbox registers.
 */
typedef struct CyU3PMbox
{
    uint32_t w0;                        /**< Contents of the lower mailbox register. */
    uint32_t w1;                        /**< Contents of the upper mailbox register. */
} CyU3PMbox;

/** \brief Type of function to be called to notify about a mailbox related interrupt.

    <B>Description</B>\n
    This type is the prototype for a callback function that will be called to
    notify the application about a mailbox interrupt. The mboxEvt parameter
    will identify the type of interrupt.
   
    If a read interrupt is received, the mailbox registers have to be read;
    and if a write interrupt is received, any pending data can be written to
    the registers.
 */
typedef void (*CyU3PMboxCb_t)(
        CyBool_t mboxEvt                /**< Mailbox event type: 0=Read, 1=Write */
        );

/**************************************************************************
 ********************************* Data *********************************
 **************************************************************************/
extern CyU3PMboxCb_t glMboxCb;          /**< Callback to be called after mailbox intr has happened */

/**************************************************************************
 *************************** Function prototypes **************************
 **************************************************************************/

/** \brief Initialize the mailbox handler.

    <B>Description</B>\n
    Initiate the mailbox related structures and register a callback function that will
    be called on every mailbox related interrupt.

    **\see
    *\see CyU3PMboxDeInit
 */
extern void
CyU3PMboxInit (
        CyU3PMboxCb_t callback          /**< Callback function to be called on interrupt. */
        );

/** \brief De-initialize the mailbox handler.

    <B>Description</B>\n
    Destroys the mailbox related structures.

    **\see
    *\see CyU3PMboxInit
 */
extern void
CyU3PMboxDeInit (
        void);

/** \brief Reset the mailbox handler.

    <B>Description</B>\n
    Clears the data structures and state related to mailbox handler. Can be used for error
    recovery.

    **\see
    *\see CyU3PMboxInit
 */
extern void
CyU3PMboxReset (
        void);

/** \brief This function sends a mailbox message to the external processor.

    <B>Description</B>\n
    This function writes 8 bytes of data to the outgoing mailbox registers after ensuring that
    the external processor has read out the previous message. If the previous message has not
    been read out, this function will block until the registers become free.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS            - If the operation is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - If the mbox pointer provided is NULL.\n
    * CY_U3P_ERROR_FAILURE      - If there is an error getting a mutex lock on the mailboxes.

    **\see
    *\see CyU3PMboxRead
    *\see CyU3PMboxWait
 */
extern CyU3PReturnStatus_t
CyU3PMboxWrite (
        CyU3PMbox *mbox                 /**< Pointer to mailbox message data. */
        );

/** \brief This function reads an incoming mailbox message.

    <B>Description</B>\n
    This function is used to read the contents of an incoming mailbox message. This needs
    to be called in response to a read event callback.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS            - If the operation is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - If the mbox pointer provided is NULL.

    **\see
    *\see CyU3PMboxWrite
 */
extern CyU3PReturnStatus_t
CyU3PMboxRead (
        CyU3PMbox *mbox                 /**< Pointer to buffer to hold the incoming message data. */
        );

/** \brief Wait until the Mailbox register to send messages to P-port is free.

    <B>Description</B>\n
    This function waits until the mailbox register used to send messages to the
    processor/device connected on FX3's P-port is free. This is expected to be used
    in cases where the application needs to ensure that the last message that was
    sent out has been read by the external processor or device.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS       - If the operation is successful.\n
    * CY_U3P_ERROR_FAILURE - If there is a failure getting a mutex lock on the mailboxes.

    **\see
    *\see CyU3PMboxWrite
 */
extern CyU3PReturnStatus_t
CyU3PMboxWait (
        void);

#include "cyu3externcend.h"

#endif /* _INCLUDED_CYU3_MBOX_H_ */

/*[]*/

