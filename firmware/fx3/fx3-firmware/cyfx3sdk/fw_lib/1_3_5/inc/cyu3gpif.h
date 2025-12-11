/*
 ## Cypress FX3 Core Library Header (cyu3gpif.h)
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

/** \file cyu3gpif.h
    \brief This file defines the GPIF related data structures and APIs in the FX3 SDK.

    <B>Description</B>\n
   The FX3 device includes a General Programmable Interface which can be used
   to connect it to any other processor, ASIC or FPGA using most master or
   slave protocols. The GPIF-II block on the FX3 device takes care of
   implementing the desired protocols by using a programmable state machine.

   The GPIF-II APIs provide functions that can be used to configure the GPIF
   state machine to implement any desired protocol. Functions are also provided
   to control the execution of the state machine and to manage the actual data
   transfer across the GPIF configured interface.
 */

#ifndef _INCLUDED_CYU3GPIF_H_
#define _INCLUDED_CYU3GPIF_H_

#include "cyu3types.h"
#include "cyu3dma.h"
#include "cyu3externcstart.h"

/**************************************************************************
 ********************************* Macros *********************************
 **************************************************************************/

/** \def CYU3P_GPIF_NUM_STATES
    \brief Number of states supported by the GPIF hardware.
 */
#define CYU3P_GPIF_NUM_STATES                           (256)

/** \def CYU3P_GPIF_NUM_TRANS_FNS
    \brief Number of distinct transfer functions supported by the GPIF hardware.
 */
#define CYU3P_GPIF_NUM_TRANS_FNS                        (32)

/** \def CYU3P_GPIF_INVALID_STATE
    \brief Invalid state index for use in state machine control functions.
 */
#define CYU3P_GPIF_INVALID_STATE                        (0xFFFF)

/** \def CYU3P_PIB_THREAD_COUNT
    \brief Number of DMA threads on the GPIF (P-Port)
 */
#define CYU3P_PIB_THREAD_COUNT                          (4)

/** \def CYU3P_PIB_SOCKET_COUNT
    \brief Number of DMA sockets on the GPIF (P-port)
 */
#define CYU3P_PIB_SOCKET_COUNT                          (32)

/** \def CYU3P_PIB_MAX_BURST_SETTING
    \brief Maximum burst size allowed for P-port sockets. The constant corresponds
    to Log(2) of size, which means that the max. size is 16 KB.
 */
#define CYU3P_PIB_MAX_BURST_SETTING                     (14)

/**************************************************************************
 ******************************* Data types *******************************
 **************************************************************************/

/** \brief Information on a single GPIF transition from one state to another.

    <B>Description</B>\n
    The GPIF state machine on the FX3 device is defined through a
    set of transition descriptors. These descriptors include fields for
    specifying the next state, the conditions for transition, and the
    output values.

    This structure encapsulates all of the information that forms the left
    and right transition descriptors for a state.

    **\see
    *\see CyU3PGpifWaveformLoad
 */
typedef struct CyU3PGpifWaveData
{
    uint32_t leftData[3];                       /**< 12 byte left transition descriptor. */
    uint32_t rightData[3];                      /**< 12 byte right transition descriptor. */
} CyU3PGpifWaveData;


/** \brief List of GPIF related events tracked by the driver.

    <B>Description</B>\n
    This type lists the various GPIF hardware and state machine related events
    that may be notified to the user application.
 */
typedef enum CyU3PGpifEventType
{
    CYU3P_GPIF_EVT_END_STATE = 0,               /**< State machine has reached the designated end state. */
    CYU3P_GPIF_EVT_SM_INTERRUPT,                /**< State machine has raised a software interrupt. */
    CYU3P_GPIF_EVT_SWITCH_TIMEOUT,              /**< Desired state machine switch has timed out. */
    CYU3P_GPIF_EVT_ADDR_COUNTER,                /**< Address counter has reached the limit. */
    CYU3P_GPIF_EVT_DATA_COUNTER,                /**< Data counter has reached the limit. */
    CYU3P_GPIF_EVT_CTRL_COUNTER,                /**< Control counter has reached the limit. */
    CYU3P_GPIF_EVT_ADDR_COMP,                   /**< Address comparator match has been obtained. */
    CYU3P_GPIF_EVT_DATA_COMP,                   /**< Data comparator match has been obtained. */
    CYU3P_GPIF_EVT_CTRL_COMP,                   /**< Control comparator match has been obtained. */
    CYU3P_GPIF_EVT_CRC_ERROR                    /**< Incorrect CRC received on a read operation. */
} CyU3PGpifEventType;

/** \brief List of supported comparators in the GPIF hardware.

    <B>Description</B>\n
    This function lists the hardware comparators that are part of the GPIF block.
    Each of these comparators is configured by calling the CyU3PGpifInitComparator
    function with the corresponding type.

    **\see
    *\see CyU3PGpifInitComparator
 */
typedef enum CyU3PGpifComparatorType
{
    CYU3P_GPIF_COMP_CTRL = 0,                   /**< Control signals comparator. */
    CYU3P_GPIF_COMP_ADDR,                       /**< Address bus comparator. */
    CYU3P_GPIF_COMP_DATA                        /**< Data bus comparator. */
} CyU3PGpifComparatorType;

/** \brief Callback type that is invoked to inform the application about GPIF events.

    <B>Description</B>\n
    This type defines the signature for the callback function that is invoked
    by the GPIF driver to inform the user application about GPIF related events.
    A callback function of this type should be registered with the GPIF driver.

    **\see
    *\see CyU3PGpifRegisterCallback
 */
typedef void (*CyU3PGpifEventCb_t) (
        CyU3PGpifEventType event,               /**< Event type that is being notified. */
        uint8_t            currentState         /**< Current state of the State Machine. */
        );

/** \brief Special callback type used for notification of GPIF state machine interrupts.

    <B>Description</B>\n
    The generic GPIF interrupt callback mechanism provided by CyU3PGpifRegisterCallback
    delivers the callback in thread context. While this allows the user to call potentially
    blocking API calls from the callback, the latency from interrupt to callback will not be
    predictable and can extend to about 100 us.

    This callback type provides notifications of only GPIF state machine interrupts (raised
    using the INTR_CPU action in the state machine) in interrupt context. This notification
    can be used to obtain a low latency notification of state machine interrupts. Please note
    that API calls that require a mutex get (such as DMA channel APIs) cannot be made directly
    from this callback.

    **\see
    *\see CyU3PGpifRegisterSMIntrCallback
 */
typedef void (*CyU3PGpifSMIntrCb_t) (
        uint8_t stateId                         /**< Current state of the GPIF state machine. */
        );

/** \cond GPIF_INTERNAL
 */

/** \brief List of Alpha outputs from the GPIF state machine.

    <B>Description</B>\n
    Alpha outputs are the input defined outputs that are specified in the
    GPIF state machine. This type enumerates the alpha outputs supported
    by the GPIF hardware in the FX3 device.
 */
typedef enum CyU3PGpifAlphaOp
{
    CYU3GPIF_ALPHA_DQ_OEN   = 0x01,             /**< Bit 00 = Output enable for Data bus. */
    CYU3GPIF_ALPHA_UPD_DOUT = 0x02,             /**< Bit 01 = Update output data by popping from FIFO. */
    CYU3GPIF_ALPHA_SAMP_DIN = 0x04,             /**< Bit 02 = Sample the input data. */
    CYU3GPIF_ALPHA_SAMP_AIN = 0x08,             /**< Bit 03 = Sample the input address. */
    CYU3GPIF_ALPHA_USER0    = 0x10,             /**< Bit 04 = User selected alpha. */
    CYU3GPIF_ALPHA_USER1    = 0x20,             /**< Bit 05 = User selected alpha. */
    CYU3GPIF_ALPHA_USER2    = 0x40,             /**< Bit 06 = User selected alpha. */
    CYU3GPIF_ALPHA_USER3    = 0x80              /**< Bit 07 = User selected alpha. */
} CyU3PGpifAlphaOp;

/** \brief List of Beta outputs from the GPIF state machine.

    <B>Description</B>\n
    Beta outputs are the state defined outputs that are specified in the GPIF
    state machine. This type enumerates the beta outputs supported by the
    GPIF hardware in the FX3 device.
 */
typedef enum CyU3PGpifBetaOp
{
    CYU3GPIF_BETA_USER0      = 0x00000001U,     /**< Bit 00 = User selected beta. */
    CYU3GPIF_BETA_USER1      = 0x00000002U,     /**< Bit 01 = User selected beta. */
    CYU3GPIF_BETA_USER2      = 0x00000004U,     /**< Bit 02 = User selected beta. */
    CYU3GPIF_BETA_USER3      = 0x00000008U,     /**< Bit 03 = User selected beta. */
    CYU3GPIF_BETA_RESERVED0  = 0x00000010U,     /**< Bit 04 = Thread number field: Set separately. */
    CYU3GPIF_BETA_RESERVED1  = 0x00000020U,     /**< Bit 05 = Thread number field: Set separately. */
    CYU3GPIF_BETA_POP_RQ     = 0x00000040U,     /**< Bit 06 = Pop data from read queue. */
    CYU3GPIF_BETA_PUSH_WQ    = 0x00000080U,     /**< Bit 07 = Push data into write queue. */
    CYU3GPIF_BETA_POP_ARQ    = 0x00000100U,     /**< Bit 08 = Pop address from read queue. */
    CYU3GPIF_BETA_PUSH_AWQ   = 0x00000200U,     /**< Bit 09 = Push address into write queue. */
    CYU3GPIF_BETA_ADDR_OEN   = 0x00000400U,     /**< Bit 10 = Output enable for address. */
    CYU3GPIF_BETA_CTRL_OEN   = 0x00000800U,     /**< Bit 11 = Output enable for control pins set as outputs. */
    CYU3GPIF_BETA_INC_CTRL   = 0x00001000U,     /**< Bit 12 = Increment control counter. */
    CYU3GPIF_BETA_RST_CTRL   = 0x00002000U,     /**< Bit 13 = Reset control counter. */
    CYU3GPIF_BETA_INC_ADDR   = 0x00004000U,     /**< Bit 14 = Increment address counter. */
    CYU3GPIF_BETA_RST_ADDR   = 0x00008000U,     /**< Bit 15 = Reset address counter. */
    CYU3GPIF_BETA_INC_DATA   = 0x00010000U,     /**< Bit 16 = Increment data counter. */
    CYU3GPIF_BETA_RST_DATA   = 0x00020000U,     /**< Bit 17 = Reset data counter. */
    CYU3GPIF_BETA_INTR_CPU   = 0x00040000U,     /**< Bit 18 = Raise interrupt to ARM CPU. */
    CYU3GPIF_BETA_INTR_HOST  = 0x00080000U,     /**< Bit 19 = Raise interrupt to interface host (peer). */
    CYU3GPIF_BETA_UPD_AOUT   = 0x00100000U,     /**< Bit 20 = Update the address for output. */
    CYU3GPIF_BETA_REG_ACCESS = 0x00200000U,     /**< Bit 21 = Perform MMIO register access. */
    CYU3GPIF_BETA_INIT_CRC   = 0x00400000U,     /**< Bit 22 = Initialize CRC computation. */
    CYU3GPIF_BETA_CALC_CRC   = 0x00800000U,     /**< Bit 23 = Include data in CRC calculation. */
    CYU3GPIF_BETA_USE_CRC    = 0x01000000U,     /**< Bit 24 = Check CRC against input value, or output CRC. */
    CYU3GPIF_BETA_SET_DRQ    = 0x02000000U,     /**< Bit 25 = Assert the DRQ output. */
    CYU3GPIF_BETA_CLR_DRQ    = 0x04000000U,     /**< Bit 26 = De-assert the DRQ output. */
    CYU3GPIF_BETA_THR_DONE   = 0x08000000U,     /**< Bit 27 = Shut-down thread. */
    CYU3GPIF_BETA_THR_WRAPUP = 0x10000000U,     /**< Bit 28 = Shut-down thread and wrap up buffer. */
    CYU3GPIF_BETA_SEND_EOP   = 0x20000000U,     /**< Bit 29 = Send EOP to DMA adapter. */
    CYU3GPIF_BETA_SEND_EOT   = 0x40000000U,     /**< Bit 30 = Send EOT to DMA adapter. */
    CYU3GPIF_BETA_PP_ACCESS  = (int)0x80000000  /**< Bit 31 = Direct access to PP registers. */
} CyU3PGpifBetaOp;

/** \brief List of lambda inputs from the GPIF state machine.

    <B>Description</B>\n
    Lambda inputs determine the state transitions performed by the GPIF
    hardware. Each state in the SM can select a number of lambdas that will
    determine its behavior. This type enumerates the available lambda input
    signals.

    **\see
    *\see CyU3PGpifOutputConfigure
 */
typedef enum CyU3PGpifLambdaIp
{
    CYU3GPIF_LAMBDA_CTRL00 = 0,                 /**< Bit 00 = CTRL[0] input. */
    CYU3GPIF_LAMBDA_CTRL01,                     /**< Bit 01 = CTRL[1] input. */
    CYU3GPIF_LAMBDA_CTRL02,                     /**< Bit 02 = CTRL[2] input. */
    CYU3GPIF_LAMBDA_CTRL03,                     /**< Bit 03 = CTRL[3] input. */
    CYU3GPIF_LAMBDA_CTRL04,                     /**< Bit 04 = CTRL[4] input. */
    CYU3GPIF_LAMBDA_CTRL05,                     /**< Bit 05 = CTRL[5] input. */
    CYU3GPIF_LAMBDA_CTRL06,                     /**< Bit 06 = CTRL[6] input. */
    CYU3GPIF_LAMBDA_CTRL07,                     /**< Bit 07 = CTRL[7] input. */
    CYU3GPIF_LAMBDA_CTRL08,                     /**< Bit 08 = CTRL[8] input. */
    CYU3GPIF_LAMBDA_CTRL09,                     /**< Bit 09 = CTRL[9] input. */
    CYU3GPIF_LAMBDA_CTRL10,                     /**< Bit 10 = CTRL[10] input. */
    CYU3GPIF_LAMBDA_CTRL11,                     /**< Bit 11 = CTRL[11] input. */
    CYU3GPIF_LAMBDA_CTRL12,                     /**< Bit 12 = CTRL[12] input. */
    CYU3GPIF_LAMBDA_CTRL13,                     /**< Bit 13 = CTRL[13] input. */
    CYU3GPIF_LAMBDA_CTRL14,                     /**< Bit 14 = CTRL[14] input. */
    CYU3GPIF_LAMBDA_CTRL15,                     /**< Bit 15 = CTRL[15] input. */
    CYU3GPIF_LAMBDA_DOUT_VALID,                 /**< Bit 16 = EGRESS_DATA_VALID */
    CYU3GPIF_LAMBDA_DIN_VALID,                  /**< Bit 17 = INGRESS_DATA_VALID */
    CYU3GPIF_LAMBDA_CTRL_CNT,                   /**< Bit 18 = Control counter hit limit. */
    CYU3GPIF_LAMBDA_ADDR_CNT,                   /**< Bit 19 = Address counter hit limit. */
    CYU3GPIF_LAMBDA_DATA_CNT,                   /**< Bit 20 = Data counter hit limit. */
    CYU3GPIF_LAMBDA_CTRL_CMP,                   /**< Bit 21 = Control comparator match. */
    CYU3GPIF_LAMBDA_ADDR_CMP,                   /**< Bit 22 = Address comparator match. */
    CYU3GPIF_LAMBDA_DATA_CMP,                   /**< Bit 23 = Data comparator match. */
    CYU3GPIF_LAMBDA_DMA_THRES,                  /**< Bit 24 = Current thread is above DMA threshold (watermark). */
    CYU3GPIF_LAMBDA_DMA_AREADY,                 /**< Bit 25 = Address socket is ready to be written into / read from. */
    CYU3GPIF_LAMBDA_DMA_DREADY,                 /**< Bit 26 = Data socket is ready to be written into / read from. */
    CYU3GPIF_LAMBDA_CRC_ERROR,                  /**< Bit 27 = CRC error has been detected. */
    CYU3GPIF_LAMBDA_ONE,                        /**< Bit 28 = Logic: 1 */
    CYU3GPIF_LAMBDA_INTR_ACTV,                  /**< Bit 29 = CPU interrupt is already active (pending). */
    CYU3GPIF_LAMBDA_CPU_DEF,                    /**< Bit 30 = CPU controlled lambda signal. */
    CYU3GPIF_LAMBDA_RESERVED0                   /**< Bit 31 = Reserved: Unused. */
} CyU3PGpifLambdaIp;

/** \endcond
 */

/** \brief List of control outputs and flags from the GPIF hardware.

    <B>Description</B>\n
    The GPIF block in the FX3 device can generate a set of control outputs and
    DMA status flags that can be used by the external processor to control data
    transfers. This enumeration lists the various control outputs and flags that
    are supported by the GPIF.

    The four Alpha outputs and Beta outputs listed below correspond to the output
    signals configured in the GPIF II Designer project. The Alpha outputs are the
    early outputs (low latency) configured in GPIF II Designer, and the Beta outputs
    are the delayed (higher latency) output signals.

    The user defined output signals as well as the various DMA flags can be
    selected for connection to a given GPIF control signal.

    **\see
    *\see CyU3PGpifOutputConfigure
 */
typedef enum CyU3PGpifOutput_t
{
    CYU3P_GPIF_OP_ALPHA0 = 0,                   /**< First early output configured in GPIF II Designer. */
    CYU3P_GPIF_OP_ALPHA1,                       /**< Second early output configured in GPIF II Designer. */
    CYU3P_GPIF_OP_ALPHA2,                       /**< Third early output configured in GPIF II Designer. */
    CYU3P_GPIF_OP_ALPHA3,                       /**< Fourth early output configured in GPIF II Designer. */
    CYU3P_GPIF_OP_BETA0 = 8,                    /**< First delayed output configured in GPIF II Designer. */
    CYU3P_GPIF_OP_BETA1,                        /**< Second delayed output configured in GPIF II Designer. */
    CYU3P_GPIF_OP_BETA2,                        /**< Third delayed output configured in GPIF II Designer. */
    CYU3P_GPIF_OP_BETA3,                        /**< Fourth delayed output configured in GPIF II Designer. */
    CYU3P_GPIF_OP_THR0_READY = 16,              /**< DMA ready flag for Thread 0. */
    CYU3P_GPIF_OP_THR1_READY,                   /**< DMA ready flag for Thread 1. */
    CYU3P_GPIF_OP_THR2_READY,                   /**< DMA ready flag for Thread 2. */
    CYU3P_GPIF_OP_THR3_READY,                   /**< DMA ready flag for Thread 3. */
    CYU3P_GPIF_OP_THR0_PART,                    /**< DMA watermark flag for Thread 0. */
    CYU3P_GPIF_OP_THR1_PART,                    /**< DMA watermark flag for Thread 1. */
    CYU3P_GPIF_OP_THR2_PART,                    /**< DMA watermark flag for Thread 2. */
    CYU3P_GPIF_OP_THR3_PART,                    /**< DMA watermark flag for Thread 3. */
    CYU3P_GPIF_OP_DMA_READY,                    /**< DMA ready flag for the active DMA thread. */
    CYU3P_GPIF_OP_PARTIAL,                      /**< DMA watermark flag for the active DMA thread. */
    CYU3P_GPIF_OP_PPDRQ                         /**< PPDRQ interrupt status derived from the PP_DRQR5_MASK register. */
} CyU3PGpifOutput_t;

/** \brief Structure that holds all configuration inputs for the GPIF hardware.

    <B>Description</B>\n
    The GPIF block on the FX3 device has a set of general configuration registers,
    transition function registers and state descriptors that need to be initialized
    to make the GPIF state machine functional. This structure encapsulates all the
    data that is required to program the GPIF block to load a user defined state
    machine.

    **\see
    *\see CyU3PGpifLoad
    *\see CyU3PGpifWaveData
 */
typedef struct CyU3PGpifConfig_t
{
    const uint16_t           stateCount;        /**< Number of states to be initialized. */
    const CyU3PGpifWaveData *stateData;         /**< Pointer to array containing state descriptors. */
    const uint8_t           *statePosition;     /**< Pointer to array index -> state number mapping. */
    const uint16_t           functionCount;     /**< Number of transition functions to be initialized. */
    const uint16_t          *functionData;      /**< Pointer to array containing transition function data. */
    const uint16_t           regCount;          /**< Number of GPIF config registers to be initialized. */
    const uint32_t          *regData;           /**< Pointer to array containing GPIF register values. */
} CyU3PGpifConfig_t;

/** \cond GPIF_INTERNAL
 */

/**************************************************************************
 **************************** Global Variables ****************************
 **************************************************************************/

extern CyU3PGpifEventCb_t      glGpifEvtCb;
extern CyU3PGpifSMIntrCb_t     glGpifSMCb;

/** \endcond
 */

/**************************************************************************
 *************************** Function prototypes **************************
 **************************************************************************/

/** \brief Function to program the user defined state machine into the GPIF registers.

    <B>Description</B>\n
    This function allows all of the configuration values corresponding to the
    user defined state machine to be loaded into the GPIF registers. The data
    to be passed as parameter to this function is received as output from the
    GPIF Designer tool.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the configuration was successful.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - if a 32 bit GPIF configuration is being used on a part that does not support this.\n
    * CY_U3P_ERROR_ALREADY_STARTED - if the GPIF hardware has already been programmed.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the input to the API is inconsistent.

    **\see
    *\see CyU3PGpifConfig_t
 */
extern CyU3PReturnStatus_t
CyU3PGpifLoad (
        const CyU3PGpifConfig_t *conf           /**< Pointer to GPIF configuration structure. */
        );

/** \brief This function registers an event callback for the GPIF driver.

    <B>Description</B>\n
    The GPIF driver keeps track of GPIF related events and raises notifications
    to the application logic when required. This function is used to register
    the callback function that will be invoked to notify the application about
    GPIF events.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PGpifEventCb_t
    *\see CyU3PGpifEventType
 */
extern void
CyU3PGpifRegisterCallback (
        CyU3PGpifEventCb_t cbFunc               /**< Pointer to callback function. */
        );

/** \brief Register a callback function for fast notification of GPIF state machine interrupts.

    <B>Description</B>\n
    This function registers a callback function that will be invoked from interrupt context
    when a GPIF state machine interrupt is detected (caused by the INTR_CPU action in the
    GPIF state machine).

    This callback provides a fast notification of the interrupt and is not subject to thread
    switching delays. Please note that API calls that require a mutex get or equivalent cannot
    be directly called from this callback function.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PGpifSMIntrCb_t
 */
extern void
CyU3PGpifRegisterSMIntrCallback (
        CyU3PGpifSMIntrCb_t cb                  /**< Callback function pointer. */
        );

/** \brief Initialize the GPIF state machine table.

    <B>Description</B>\n
    The GPIF state machine is programmed in the form of a set of waveform
    table entries. This function is used to take the output state machine
    data from the designer tool and to initialize the state machine based
    on these values.

    Note that the state data is generated in the form of a transition data
    array and a state index mapping table by the GPIF designer tool. The
    transitionData parameter contains all unique combinations of transition
    data that are part of this state machine. The stateDataMap maps each
    state index to the transition data entry from the array.

    It is possible to load multiple disjoint state machine configurations
    into distinct parts of the waveform memory by making multiple calls to
    this API. Each call needs to specify a distinct range of states to be
    initialized.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if any of the parameters are invalid.\n
    * CY_U3P_ERROR_ALREADY_STARTED if the GPIF hardware is running.

    **\see
    *\see CyU3PGpifWaveData
 */
extern CyU3PReturnStatus_t
CyU3PGpifWaveformLoad (
        uint8_t            firstState,          /**< The first state to be initialized in this function call. */
        uint16_t           stateCnt,            /**< Number of states to be initialized with data. */
        uint8_t           *stateDataMap,        /**< Table that maps state indices to the descriptor table indices. */
        CyU3PGpifWaveData *transitionData       /**< Table containing the transition information for various states. */
        );

/** \brief Initialize the GPIF configuration registers.

    <B>Description</B>\n
    The GPIF hardware has a number of configuration registers that control
    the functioning of the state machine. These registers need to be initialized
    with values provided by the GPIF designer tool.

    This function takes in an array containing the register values, and programs
    all of the configuration registers with these values.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if successful.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - if a 32 bit GPIF configuration is being used on a part that does not support this.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if any of the parameters are invalid.\n
    * CY_U3P_ERROR_ALREADY_STARTED if the GPIF hardware is running.\n

    **\see
    *\see CyU3PGpifWaveformLoad
    *\see CyU3PGpifInitTransFunctions
    *\see CyU3PGpifRegisterConfig
 */
extern CyU3PReturnStatus_t
CyU3PGpifConfigure (
        uint8_t         numRegs,        /**< Number of registers to configure (size of array). */
        const uint32_t *regData         /**< Pointer to uint32_t[] array, where element [i] contains the
                                             data to be loaded into GPIF config register "i". */
        );

/** \brief Initialize the GPIF configuration registers.

    <B>Description</B>\n
    This is an alternate flavor of the GPIF register configuration function
    that can be used when only a small subset of the registers needs to be
    initialized. The input is accepted in the form of a two-columned matrix,
    column 0 representing the register address to be initialized and column 1
    representing the value to be written into this register.

    Please note that the registers will be initialized in the same order they
    are provided in the array, and that it is possible to write multiple times
    to the same register.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if successful.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - if a 32 bit GPIF configuration is being used on a part that does not support this.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if any of the parameters are invalid.\n
    * CY_U3P_ERROR_ALREADY_STARTED if the GPIF hardware is running.\n

    **\see
    *\see CyU3PGpifConfigure
 */
extern CyU3PReturnStatus_t
CyU3PGpifRegisterConfig (
        uint16_t numRegs,               /**< Number of registers to configure (size of the matrix). */
        uint32_t regData[][2]           /**< Reference to array of {address, data} tuples corresponding
                                             to GPIF registers to initialize. */
        );

/** \brief Initialize the GPIF transition function registers.

    <B>Description</B>\n
    This function initializes the GPIF transition function registers with data
    provided by the GPIF designer tool.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if any of the parameters are invalid.\n
    * CY_U3P_ERROR_ALREADY_STARTED if the GPIF hardware is running.

    **\see
    *\see CyU3PGpifConfigure
 */
extern CyU3PReturnStatus_t
CyU3PGpifInitTransFunctions (
        uint16_t *fnTable               /**< Pointer to the table (array) containing the values
                                             with which the registers should be initialized. */
        );

/** \brief Disables the GPIF state machine and hardware.

    <B>Description</B>\n
    This function is used to disable the GPIF state machine and hardware. If the forceReload parameter
    is set to true, the current configuration information is lost and needs to be restored using the
    CyU3PGpifLoad or equivalent functions. If the forceReload parameter is set to false, the GPIF
    configuration is retained and it is possible to restart the it by calling the CyU3PGpifSMStart API.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PGpifConfigure
 */
extern void
CyU3PGpifDisable (
        CyBool_t forceReload                    /**< Whether a GPIF re-configuration is to be forced. */
        );

/** \brief Start the waveform state machine from the specified state.

    <B>Description</B>\n
    This function starts off the GPIF state machine from the specified state
    index. Please note that this function should only be used to start the
    state machine from an idle state. The CyU3PGpifSMSwitch function should
    be used to switch from one state to another in mid-execution.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if successful.\n
    * CY_U3P_ERROR_NOT_CONFIGURED if GPIF has not been configured as yet.\n
    * CY_U3P_ERROR_ALREADY_STARTED if the state machine is already active.

    **\see
    *\see CyU3PGpifSMSwitch
 */
extern CyU3PReturnStatus_t
CyU3PGpifSMStart (
        uint8_t stateIndex,             /**< State from which to start execution. This should be 0 in most cases. */
        uint8_t initialAlpha            /**< Initial alpha values to start GPIF state machine operation with. */
        );

/** \brief This function is used to start GPIF state machine execution from a desired
    state.

    <B>Description</B>\n
    This function allows the caller to switch to a desired state and continue
    GPIF state machine execution from there. The toState parameter specifies
    the state to initiate operation from.

    The fromState parameter can be used to ensure that the transition to toState
    happens only when the state machine is in a well defined idle state. If a
    valid state id (0 - 255) is passed as the fromState, the transition is only
    allowed from that state index. If not, the state machine is immediately
    switched to the toState.

    The endState can be used to obtain a notification when the state machine
    execution has reached the designated end state. Again, this functionality
    is only valid if a valid endState value is passed in.

    The switchTimeout specifies the amount of time to wait for the transition
    to the desired toState. This is only meaningful if a fromState is specified,
    and the timeout value is specified in terms of GPIF hardware clock cycles.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if any of the parameters are invalid.

    **\see
    *\see CyU3PGpifSMStart
 */
extern CyU3PReturnStatus_t
CyU3PGpifSMSwitch (
        uint16_t fromState,             /**< The state from which to do the switch to the desired state. */
        uint16_t toState,               /**< The state to which to transition from fromState. */
        uint16_t endState,              /**< The end state for this execution path. */
        uint8_t  initialAlpha,          /**< Initial Alpha values to use when switching states. */
        uint32_t switchTimeout          /**< Timeout setting for the switch operation in GPIF clock cycles. */
        );

/** \brief Function to configure the GPIF control counter.

    <B>Description</B>\n
    The GPIF hardware includes a 16-bit control counter, one of whose bits
    can be connected to the CTRL[9] output from the device. This function is
    used to configure and initialize the control counter and to select the
    bit that should be connected to the CTRL[9] output. Please note that the
    specified bit location will be truncated to a value less than 16.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3PGpifInitCtrlCounter (
        uint16_t initValue,                     /**< Initial (reset) value for the counter. */
        uint16_t limit,                         /**< Value at which to stop the counter and flag an event. */
        CyBool_t reload,                        /**< Whether to reload the counter and continue after limit is hit. */
        CyBool_t upCount,                       /**< Whether to count upwards from the initial value. */
        uint8_t  outputBit                      /**< Selects counter bit to be connected to CTRL[9] output. */
        );

/** \brief Function to configure the GPIF address counter.

    <B>Description</B>\n
    This function is used to configure the GPIF address counter with the desired
    initial value, limit and counting mode.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3PGpifInitAddrCounter (
        uint32_t initValue,                     /**< Initial value to start the counter from. */
        uint32_t limit,                         /**< Counter limit at which the counter stops. */
        CyBool_t reload,                        /**< Whether to reload the counter when the limit is reached. */
        CyBool_t upCount,                       /**< Whether to count upwards from the initial value. */
        uint8_t  increment                      /**< The value to be incremented/decremented from the counter at each
                                                     step. */
        );

/** \brief Function to configure the GPIF data counter.

    <B>Description</B>\n
    This function is used to configure the GPIF data counter with the desired
    initial value, limit and counting mode.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3PGpifInitDataCounter (
        uint32_t initValue,                     /**< Initial value to start the counter from. */
        uint32_t limit,                         /**< Counter limit at which the counter stops. */
        CyBool_t reload,                        /**< Whether to reload the counter when the limit is reached. */
        CyBool_t upCount,                       /**< Whether to count upwards from the initial value. */
        uint8_t  increment                      /**< The value to be incremented/decremented from the counter at each
                                                     step. */
        );

/** \brief This function configures one of the comparators in the GPIF hardware.

    <B>Description</B>\n
    The GPIF hardware includes comparators that can be used to check if the
    control, address or data values match a desired pattern. There is a separate
    comparator for each class of signals. This function is used to configure
    and initialize one of these comparators as required.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the type specified is invalid.

    **\see
    *\see CyU3PGpifComparatorType
 */
extern CyU3PReturnStatus_t
CyU3PGpifInitComparator (
        CyU3PGpifComparatorType type,           /**< The type of comparator to configure. */
        uint32_t                value,          /**< The value to compare the signals against. */
        uint32_t                mask            /**< Mask that specifies which bits are to be used in the comparison. */
        );

/** \brief Function to pause/resume the GPIF state machine.

    <B>Description</B>\n
    The GPIF state machine continuously functions on the basis of configuration
    values that are set using the CyU3PGpifWaveformLoad API. While the state
    machine normally functions without any software control, it is possible for
    the software to stop/start the functioning of the state machine at will.
    This function allows the application to either pause or resume the state
    machine.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if successful.\n
    * CY_U3P_ERROR_NOT_CONFIGURED if the state machine has not been configured.
 */
extern CyU3PReturnStatus_t
CyU3PGpifSMControl (
        CyBool_t pause                          /**< Whether to pause the state machine or resume it. */
        );

/** \brief Function to set/clear the software controlled state machine input.

    <B>Description</B>\n
    The GPIF hardware supports one software driven internal input signal that
    can be used to control/direct the state machine functionality. This function
    is used to set the state of this input signal to a desired value.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3PGpifControlSWInput (
        CyBool_t set                            /**< Whether to set (assert) the software input signal. */
        );

/** \brief Function to select an active socket on the P-port and to configure it.

    <B>Description</B>\n
    The GPIF hardware allows 4 different sockets on the P-port to be accessed at a time
    by supporting 4 independent DMA threads. The active socket for each thread and its
    properties can be selected by the user at run-time. This should be done in software
    only in the case where it is not being done through the PP registers or the state
    machine itself.

    This function allows the user to select and configure the active socket in the case
    where software is responsible for these actions. The API will respond with an error
    if the hardware is taking care of socket configurations.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if successful.\n
    * CY_U3P_ERROR_FAILURE if the socket selection and configuration is being done automatically.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if one or more of the parameters are out of range.
 */
extern CyU3PReturnStatus_t
CyU3PGpifSocketConfigure (
        uint8_t            threadIndex,         /**< Thread index whose active socket is to be configured. */
        CyU3PDmaSocketId_t socketNum,           /**< The socket to be associated with this thread. */
        uint16_t           watermark,           /**< Watermark level for this socket in number of 4-byte words. */
        CyBool_t           flagOnData,          /**< Whether the partial flag should be set when the socket contains
                                                     more data than the watermark. If false, the flag will be set when
                                                     the socket contains less data than the watermark. */
        uint8_t            burst                /**< Logarithm (to the base 2) of the burst size for this socket.
                                                     The burst size is the minimum number of words of data that will
                                                     be sourced/sinked across the GPIF interface without further
                                                     updates of the GPIF DMA flags. The device connected to FX3 is
                                                     expected to complete a burst that it has started regardless of any
                                                     flag changes in between. Please note that this has to be set to
                                                     a non-zero value (burst size is greater than one), when the GPIF
                                                     is being configured with a 32-bit data bus and functioning at
                                                     100 MHz. */
        );

/** \brief Read a specified number of data words from the GPIF interface.

    <B>Description</B>\n
    The GPIF hardware supports a mode where data can be read from the P-port in
    terms of words of the specified interface width. The data can be read from
    any one of the four threads of data transfer across the P-port.

    This function is used to read a specified number of data words into a
    buffer, one word at a time. Please note that each data word will be
    placed in the buffer after padding to 32 bits. A timeout period can be
    specified, and if any of the data words is not available within the
    specified period from the previous one, the operation will return with
    a timeout error.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the thread number is invalid.\n
    * CY_U3P_ERROR_FAILURE if the operation is not permitted by the GPIF configuration.
 */
extern CyU3PReturnStatus_t
CyU3PGpifReadDataWords (
        uint32_t  threadIndex,                  /**< DMA thread index from which to read data. */
        CyBool_t  selectThread,                 /**< Whether the target thread should be enabled explicitly. */
        uint32_t  numWords,                     /**< Number of words of data to read. */
        uint32_t *buffer_p,                     /**< Buffer pointer into which the data should be read. */
        uint32_t  waitOption                    /**< Timeout duration to wait for data. */
        );

/** \brief Write a specified number of data words to the GPIF interface.

    <B>Description</B>\n
    The GPIF hardware supports a mode where data can be written to the P-port in
    terms of words of the specified interface width. The data can be written to
    any one of the four threads of data transfer across the P-port.

    This function is used to write a specified number of data words from a
    buffer, one word at a time. Please note that each data word in the buffer
    is expected to be padded to 32 bits. A timeout period can be specified, and
    if any of the data words are not sent out within the specified period from
    the previous one, the operation will return with a timeout error.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if successful.
 */
extern CyU3PReturnStatus_t
CyU3PGpifWriteDataWords (
        uint32_t  threadIndex,                  /**< DMA thread index through which to write data. */
        CyBool_t  selectThread,                 /**< Whether the target thread should be enabled explicitly. */
        uint32_t  numWords,                     /**< Number of words of data to write. */
        uint32_t *buffer_p,                     /**< Pointer to buffer containing the data. */
        uint32_t  waitOption                    /**< Timeout duration to wait for data sending. */
        );

/** \brief Configure the functionality of a GPIF output pin.

    <B>Description</B>\n
    This function is used to configure the functionality of a GPIF output pin. This is
    primarily used to map a selected flag to a particular control output pin. The function
    configures the selected control pin as output, updates the polarity and connects the
    selected flag/signal to the pin.

    <B>Note</B>\n
    This configuration is likely to be overridden by any GPIF configuration API calls
    such as CyU3PGpifLoad or CyU3PGpifRegisterConfig.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the configuration is successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the control pin or output selected is invalid.

    **\see
    *\see CyU3PGpifOutput_t
 */
extern CyU3PReturnStatus_t
CyU3PGpifOutputConfigure (
        uint8_t           ctrlPin,              /**< Control pin number to be configured. */
        CyU3PGpifOutput_t opFlag,               /**< Selected output flag. */
        CyBool_t          isActiveLow           /**< Polarity: 0=Active high, 1=Active low. */
        );

/** \brief Get the current state of the GPIF state machine.

    <B>Description</B>\n
    This function is used to fetch the current state of the GPIF state machine. This is
    primarily used to provide debug information.

    <B>Note</B>:\n
    In cases where the GPIF interface is externally clocked, this API could return incorrect
    results if the states are changing while the API is called. In such cases, the caller needs
    to call this API repeatedly until a stable return value is received.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the query is successful.\n
    * CY_U3P_ERROR_NOT_CONFIGURED if the state machine has not been configured as yet.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the return parameter has not been provided as part of the call.

    **\see
    *\see CyU3PGpifSMStart
    *\see CyU3PGpifSMSwitch
 */
extern CyU3PReturnStatus_t
CyU3PGpifGetSMState (
        uint8_t *curState_p                     /**< Return parameter to be filled with current state information. */
        );

#include "cyu3externcend.h"

#endif /* _INCLUDED_CYU3GPIF_H_ */

/*[]*/

