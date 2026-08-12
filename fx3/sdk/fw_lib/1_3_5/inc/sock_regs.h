/*
 ## Cypress FX3 Core Library Header (sock_regs.h)
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

#ifndef _INCLUDED_SOCK_REGS_H_
#define _INCLUDED_SOCK_REGS_H_

#include <cyu3types.h>

/*
   This file contains the definitions for all of the DMA socket and descriptor registers
   on the FX3 device. The information is in this file should be used in conjunction with
   the structure definitions in cyu3socket.h and cyu3descriptor.h
 */

/*###########################################################################
  ####################### Socket Register Definitions #######################
  ###########################################################################*/

/*
   The following definitions correspond to the registers that make up the
   CyU3PDmaSocket_t structure.
 */

/*
   ############################################################################
   dscrChain Register definition.
   ############################################################################
 */

/*
   Descriptor number of currently active descriptor.  A value of 0xFFFF designates
   no (more) active descriptors available.  When activating a socket CPU
   shall write number of first descriptor in here. Only modify this field
   when go_suspend=1 or go_enable=0
 */
#define CY_U3P_DSCR_NUMBER_MASK                             (0x0000ffff) /* <0:15> RW:RW:X:N/A */
#define CY_U3P_DSCR_NUMBER_POS                              (0)


/*
   Number of descriptors still left to process.  This value is unrelated
   to actual number of descriptors in the list.  It is used only to generate
   an interrupt to the CPU when the value goes low or zero (or both).  When
   this value reaches 0 it will wrap around to 255.  The socket will not
   suspend or be otherwise affected unless the descriptor chains ends with
   0xFFFF descriptor number.
 */
#define CY_U3P_DSCR_COUNT_MASK                              (0x00ff0000) /* <16:23> RW:RW:X:N/A */
#define CY_U3P_DSCR_COUNT_POS                               (16)


/*
   The low watermark for dscr_count.  When dscr_count is equal or less than
   dscr_low the status bit dscr_is_low is set and an interrupt can be generated
   (depending on int mask).
 */
#define CY_U3P_DSCR_LOW_MASK                                (0xff000000) /* <24:31> R:RW:X:N/A */
#define CY_U3P_DSCR_LOW_POS                                 (24)

/*
   ############################################################################
   xferSize Register definition.
   ############################################################################
 */

/*
   The number of bytes or buffers (depends on unit bit in SCK_STATUS) that
   are part of this transfer.  A value of 0 signals an infinite/undetermined
   transaction size.
   Valid data bytes remaining in the last buffer beyond the transfer size
   will be read by socket and passed on to the core. FW must ensure that
   no additional bytes beyond the transfer size are present in the last buffer.
 */
#define CY_U3P_TRANS_SIZE_MASK                              (0xffffffff) /* <0:31> R:RW:X:N/A */
#define CY_U3P_TRANS_SIZE_POS                               (0)

/*
   ############################################################################
   xferCount Register definition.
   ############################################################################
 */

/*
   The number of bytes or buffers (depends on unit bit in SCK_STATUS) that
   have been transferred through this socket so far.  If trans_size is >0
   and trans_count>=trans_size the  ‘trans_done’ bits in SCK_STATUS is both
   set.  If SCK_STATUS.susp_trans=1 the socket is also suspended and the
   ‘suspend’ bit set. This count is updated only when a descriptor is completed
   and the socket proceeds to the next one.
   Exception: When socket suspends with PARTIAL_BUF=1, this value has been
   (incorrectly) incremented by 1 (UNIT=1) or DSCR_SIZE.BYTE_COUNT (UNIT=0).
    Firmware must correct this before resuming the socket.
 */
#define CY_U3P_TRANS_COUNT_MASK                             (0xffffffff) /* <0:31> RW:RW:X:N/A */
#define CY_U3P_TRANS_COUNT_POS                              (0)

/*
   ############################################################################
   status Register definition.
   ############################################################################
 */

/*
   Number of available (free for ingress, occupied for egress) descriptors
   beyond the current one.  This number is incremented by the adapter whenever
   an event is received on this socket and decremented whenever it activates
   a new descriptor. This value is used to create a signal to the IP Cores
   that indicates at least one buffer is available beyond the current one
   (sck_more_buf_avl).
 */
#define CY_U3P_AVL_COUNT_MASK                               (0x0000001f) /* <0:4> RW:RW:0:N/A */
#define CY_U3P_AVL_COUNT_POS                                (0)


/*
   Minimum number of available buffers required by the adapter before activating
   a new one.  This can be used to guarantee a minimum number of buffers
   available with old data to implement rollback.  If AVL_ENABLE, the socket
   will remain in STALL state until AVL_COUNT>=AVL_MIN.
 */
#define CY_U3P_AVL_MIN_MASK                                 (0x000003e0) /* <5:9> R:RW:0:N/A */
#define CY_U3P_AVL_MIN_POS                                  (5)


/*
   Enables the functioning of AVL_COUNT and AVL_MIN. When 0, it will disable
   both stalling on AVL_MIN and generation of the sck_more_buf_avl signal
   described above.
 */
#define CY_U3P_AVL_ENABLE                                   (1u << 10) /* <10:10> R:RW:0:N/A */


/*
   Internal operating state of the socket.  This field is used for debugging
   and to safely modify active sockets (see go_suspend).
 */
#define CY_U3P_STATE_MASK                                   (0x00038000) /* <15:17> RW:R:0:N/A */
#define CY_U3P_STATE_POS                                    (15)

/*
   Descriptor state. This is the default initial state indicating the descriptor
   registers are NOT valid in the Adapter. The Adapter will start loading
   the descriptor from memory if the socket becomes enabled and not suspended.
   Suspend has no effect on any other state.
 */
#define CY_U3P_STATE_DESCR                                  (0)
/*
   Stall state. Socket is stalled waiting for data to be loaded into the
   Fetch Queue or waiting for an event.
 */
#define CY_U3P_STATE_STALL                                  (1)
/*
   Active state. Socket is available for core data transfers.
 */
#define CY_U3P_STATE_ACTIVE                                 (2)
/*
   Event state. Core transfer is done. Descriptor is being written back to
   memory and an event is being generated if enabled.
 */
#define CY_U3P_STATE_EVENT                                  (3)
/*
   Check states. An active socket gets here based on the core’s EOP request
   to check the Transfer size and determine whether the buffer should be
   wrapped up. Depending on result, socket will either go back to Active
   state or move to the Event state.
 */
#define CY_U3P_STATE_CHECK1                                 (4)
/*
   Socket is suspended
 */
#define CY_U3P_STATE_SUSPENDED                              (5)
/*
   Check states. An active socket gets here based on the core’s EOP request
   to check the Transfer size and determine whether the buffer should be
   wrapped up. Depending on result, socket will either go back to Active
   state or move to the Event state.
 */
#define CY_U3P_STATE_CHECK2                                 (6)
/*
   Waiting for confirmation that event was sent.
 */
#define CY_U3P_STATE_WAITING                                (7)

/*
   Indicates the socket received a ZLP
 */
#define CY_U3P_ZLP_RCVD                                     (1u << 18) /* <18:18> RW:R:0:N/A */


/*
   Indicates the socket is currently in suspend state.  In suspend mode there
   is no active descriptor; any previously active descriptor has been wrapped
   up, copied back to memory and SCK_DSCR.dscr_number has been updated using
   DSCR_CHAIN as needed.  If the next descriptor is known (SCK_DSCR.dscr_number!=0xFFFF),
   this descriptor will be loaded after the socket resumes from suspend state.
   A socket can only be resumed by changing go_suspend from 1 to 0.  If the
   socket is suspended while go_suspend=0, it must first be set and then
   again cleared.
 */
#define CY_U3P_SUSPENDED                                    (1u << 19) /* <19:19> RW:R:0:N/A */


/*
   Indicates the socket is currently enabled when asserted.  After go_enable
   is changed, it may take some time for enabled to make the same change.
    This value can be polled to determine this fact.
 */
#define CY_U3P_ENABLED                                      (1u << 20) /* <20:20> RW:R:0:N/A */


/*
   Enable (1) or disable (0) truncating of BYTE_COUNT when TRANS_COUNT+BYTE_COUNT>=TRANS_SIZE.
    When enabled, ensures that an ingress transfer never contains more bytes
   then allowed.  This function is needed to implement burst-based prototocols
   that can only transmit full bursts of more than 1 byte.
 */
#define CY_U3P_TRUNCATE                                     (1u << 21) /* <21:21> R:RW:1:N/A */


/*
   Enable (1) or disable (0) sending of produce events from any descriptor
   in this socket.  If 0, events will be suppressed, and the descriptor will
   not be copied back into memory when completed.
 */
#define CY_U3P_EN_PROD_EVENTS                               (1u << 22) /* <22:22> R:RW:1:N/A */


/*
   Enable (1) or disable (0) sending of consume events from any descriptor
   in this socket.  If 0, events will be suppressed, and the descriptor will
   not be copied back into memory when completed.
 */
#define CY_U3P_EN_CONS_EVENTS                               (1u << 23) /* <23:23> R:RW:1:N/A */


/*
   When set, the socket will suspend before activating a descriptor with
   BYTE_COUNT<BUFFER_SIZE.
   This is relevant for egress sockets only.
 */
#define CY_U3P_SUSP_PARTIAL                                 (1u << 24) /* <24:24> R:RW:0:N/A */


/*
   When set, the socket will suspend before activating a descriptor with
   TRANS_COUNT+BUFFER_SIZE>=TRANS_SIZE.  This is relevant for both ingress
   and egress sockets.
 */
#define CY_U3P_SUSP_LAST                                    (1u << 25) /* <25:25> R:RW:0:N/A */


/*
   When set, the socket will suspend when trans_count >= trans_size.  This
   equation is checked (and hence the socket will suspend) only at the boundary
   of buffers and packets (ie. buffer wrapup or EOP assertion).
 */
#define CY_U3P_SUSP_TRANS                                   (1u << 26) /* <26:26> R:RW:1:N/A */


/*
   When set, the socket will suspend after wrapping up the first buffer with
   dscr.eop=1.  Note that this function will work the same for both ingress
   and egress sockets.
 */
#define CY_U3P_SUSP_EOP                                     (1u << 27) /* <27:27> R:RW:0:N/A */


/*
   Setting this bit will forcibly wrap-up a socket, whether it is out of
   data or not.  This option is intended mainly for ingress sockets, but
   works also for egress sockets.  Any remaining data in fetch buffers is
   ignored, in write buffers is flushed.  Transaction and buffer counts are
   updated normally, and suspend behavior also happens normally (depending
   on various other settings in this register).G45
 */
#define CY_U3P_WRAPUP                                       (1u << 28) /* <28:28> RW0C:RW1S:0:N/A */


/*
   Indicates whether descriptors (1) or bytes (0) are counted by trans_count
   and trans_size.  Descriptors are counting regardless of whether they contain
   any data or have eop set.
 */
#define CY_U3P_UNIT                                         (1u << 29) /* <29:29> R:RW:0:N/A */


/*
   Directs a socket to go into suspend mode when the current descriptor completes.
    The main use of this bit is to safely append descriptors to an active
   socket without actually suspending it (in most cases). The process is
   outlined in more detail in the architecture spec, and looks as follows:
   1: GO_SUSPEND=1
   2: modify the chain in memory
   3: check if active descriptor is 0xFFFF or last in chain
   4: if so, make corrections as neccessary (complicated)
   5: clear any pending suspend interrupts (SCK_INTR[9:5])
   6: GO_SUSPEND=0
   Note that the socket resumes only when SCK_INTR[9:5]=0 and GO_SUSPEND=0.
 */
#define CY_U3P_GO_SUSPEND                                   (1u << 30) /* <30:30> R:RW:0:N/A */


/*
   Indicates whether socket is enabled.  When go_enable is cleared while
   socket is active, ongoing transfers are aborted after an unspecified amount
   of time.  No update occurs from the descriptor registers back into memory.
    When go_enable is changed from 0 to 1, the socket will reload the active
   descriptor from memory regardless of the contents of DSCR_ registers.
   The socket will not wait for an EVENT to become active if the descriptor
   is available and ready for transfer (has space or data).
   The 'enabled' bit indicates whether the socket is actually enabled or
   not.  This field lags go_enable by an short but unspecificied of time.
 */
#define CY_U3P_GO_ENABLE                                    (1u << 31) /* <31:31> R:RW:0:N/A */

/*
   ############################################################################
   intr Register definition.
   ############################################################################
 */

#define CY_U3P_SCK_INTR_DEFAULT                             (0x00000000)

/*
   Indicates that a produce event has been received or transmitted since
   last cleared.
 */
#define CY_U3P_PRODUCE_EVENT                                (1u << 0) /* <0:0> W1S:RW1C:0:N/A */


/*
   Indicates that a consume event has been received or transmitted since
   last cleared.
 */
#define CY_U3P_CONSUME_EVENT                                (1u << 1) /* <1:1> W1S:RW1C:0:N/A */


/*
   Indicates that dscr_count has fallen below its watermark dscr_low.  If
   dscr_count wraps around to 255 dscr_is_low will remain asserted until
   cleared by s/w
 */
#define CY_U3P_DSCR_IS_LOW                                  (1u << 2) /* <2:2> W1S:RW1C:0:N/A */


/*
   Indicates the no descriptor is available.  Not available means that the
   current descriptor number is 0xFFFF.  Note that this bit will remain asserted
   until cleared by s/w, regardless of whether a new descriptor number is
   loaded.
 */
#define CY_U3P_DSCR_NOT_AVL                                 (1u << 3) /* <3:3> W1S:RW1C:0:N/A */


/*
   Indicates the socket has stalled, waiting for an event signaling its descriptor
   has become available. Note that this bit will remain asserted until cleared
   by s/w, regardless of whether the socket resumes.
 */
#define CY_U3P_STALL                                        (1u << 4) /* <4:4> W1S:RW1C:0:N/A */


/*
   Indicates the socket has gone into suspend mode.  This may be caused by
   any hardware initiated condition (e.g. DSCR_NOT_AVL, any of the SUSP_*)
   or by setting GO_SUSPEND=1.  Note that this bit will remain asserted until
   cleared by s/w, regardless of whether the suspend condition is resolved.
   Note that the socket resumes only when SCK_INTR[9:5]=0 and GO_SUSPEND=0.
 */
#define CY_U3P_SUSPEND                                      (1u << 5) /* <5:5> W1S:RW1C:0:N/A */


/*
   Indicates the socket is suspended because of an error condition (internal
   to the adapter) – if error=1 then suspend=1 as well.  Possible error causes
   are:
   - dscr_size.buffer_error bit already set in the descriptor.
   - dscr_size.byte_count > dscr_size.buffer_size
   - core writes into an inactive socket.
   - core did not consume all the data in the buffer.
   Note that the socket resumes only when SCK_INTR[9:5]=0 and GO_SUSPEND=0.
 */
#define CY_U3P_ERROR                                        (1u << 6) /* <6:6> W1S:RW1C:0:N/A */


/*
   Indicates that TRANS_COUNT has reached the limit TRANS_SIZE.  This flag
   is only set when SUSP_TRANS=1.  Note that because TRANS_COUNT is updated
   only at the granularity of entire buffers, it is possible that TRANS_COUNT
   exceeds TRANS_SIZE before the socket suspends.  Software must detect and
   deal with these situations.  When asserting EOP to the adapter on ingress,
   the trans_count is not updated unless the socket actually suspends (see
   SUSP_TRANS).
   Note that the socket resumes only when SCK_INTR[9:5]=0 and GO_SUSPEND=0.
 */
#define CY_U3P_TRANS_DONE                                   (1u << 7) /* <7:7> W1S:RW1C:0:N/A */


/*
   Indicates that the (egress) socket was suspended because of SUSP_PARTIAL
   condition.  Note that the socket resumes only when SCK_INTR[9:5]=0 and
   GO_SUSPEND=0.
 */
#define CY_U3P_PARTIAL_BUF                                  (1u << 8) /* <8:8> W1S:RW1C:0:N/A */


/*
   Indicates that the socket was suspended because of SUSP_LAST condition.
    Note that the socket resumes only when SCK_INTR[9:5]=0 and GO_SUSPEND=0.
 */
#define CY_U3P_LAST_BUF                                     (1u << 9) /* <9:9> W1S:RW1C:0:N/A */

/*
   ############################################################################
   intrMask Register definition.

   Only unmasked (intrMask bit is set to 1) interrupts will cause a DMA
   interrupt to be raised to the FX3 CPU.
   ############################################################################
 */

#define CY_U3P_SCK_INTR_MASK_DEFAULT                        (0x00000000)

/*
   1: Report interrupt to CPU
 */
#define CY_U3P_PRODUCE_EVENT                                (1u << 0) /* <0:0> R:RW:0:N/A */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_CONSUME_EVENT                                (1u << 1) /* <1:1> R:RW:0:N/A */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_DSCR_IS_LOW                                  (1u << 2) /* <2:2> R:RW:0:N/A */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_DSCR_NOT_AVL                                 (1u << 3) /* <3:3> R:RW:0:N/A */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_STALL                                        (1u << 4) /* <4:4> R:RW:0:N/A */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_SUSPEND                                      (1u << 5) /* <5:5> R:RW:0:N/A */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_ERROR                                        (1u << 6) /* <6:6> R:RW:0:N/A */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_TRANS_DONE                                   (1u << 7) /* <7:7> R:RW:0:N/A */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_PARTIAL_BUF                                  (1u << 8) /* <8:8> R:RW:0:N/A */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LAST_BUF                                     (1u << 9) /* <9:9> R:RW:0:N/A */

/*
   ############################################################################
   sckEvent Register definition.
   ############################################################################
 */

/*
   The active descriptor number for which the event is sent.
 */
#define CY_U3P_SOCK_EVENT_ACTIVE_DSCR_MASK                  (0x0000ffff) /* <0:15> RW:W:0:N/A */
#define CY_U3P_SOCK_EVENT_ACTIVE_DSCR_POS                   (0)


/*
   Type of event
   0: Consume event descriptor is marked empty - BUFFER_OCCUPIED=0)
   1: Produce event descriptor is marked full = BUFFER_OCCUPIED=1)
 */
#define CY_U3P_SOCK_EVENT_EVENT_TYPE                        (1u << 16) /* <16:16> RW:W:0:N/A */

/*###########################################################################
  ##################### Descriptor Register Definitions #####################
  ###########################################################################*/

/*
   The following definitions correspond to the registers that make up the
   CyU3PDmaDescriptor_t structure.
 */

/*
   ############################################################################
   buffer Register definition.
   ############################################################################
 */

/*
   The base address of the buffer where data is written.  This address is
   not necessarily word-aligned to allow for header/trailer/length modification.
 */
#define CY_U3P_BUFFER_ADDR_MASK                             (0xffffffff) /* <0:31> RW:RW:X:N/A */
#define CY_U3P_BUFFER_ADDR_POS                              (0)

/*
   ############################################################################
   sync Register definition.
   ############################################################################
 */

/*
   The socket number of the consuming socket to which the produce event shall
   be sent.
   If cons_ip and cons_sck matches the socket's IP and socket number then
   the matching socket becomes consuming socket.
 */
#define CY_U3P_CONS_SCK_MASK                                (0x000000ff) /* <0:7> RW:RW:X:N/A */
#define CY_U3P_CONS_SCK_POS                                 (0)


/*
   The IP number of the consuming socket to which the produce event shall
   be sent.  Use 0x3F to designate ARM CPU (so software) as consumer; the
   event will be lost in this case and an interrupt should also be generated
   to observe this condition.
 */
#define CY_U3P_CONS_IP_MASK                                 (0x00003f00) /* <8:13> RW:RW:X:N/A */
#define CY_U3P_CONS_IP_POS                                  (8)


/*
   Enable sending of a consume events from this descriptor only.  Events
   are sent only if SCK_STATUS.en_consume_ev=1.  When events are disabled,
   the adapter also does not update the descriptor in memory to clear its
   occupied bit.
 */
#define CY_U3P_EN_CONS_EVENT                                (1u << 14) /* <14:14> RW:RW:X:N/A */


/*
   Enable generation of a consume event interrupt for this descriptor only.
    This interrupt will only be seen by the CPU if SCK_STATUS.int_mask has
   this interrupt enabled as well.  Note that this flag influences the logging
   of the interrupt in SCK_STATUS; it has no effect on the reporting of the
   interrupt to the CPU like SCK_STATUS.int_mask does.
 */
#define CY_U3P_EN_CONS_INT                                  (1u << 15) /* <15:15> RW:RW:X:N/A */


/*
   The socket number of the producing socket to which the consume event shall
   be sent. If prod_ip and prod_sck matches the socket's IP and socket number
   then the matching socket becomes consuming socket.
 */
#define CY_U3P_PROD_SCK_MASK                                (0x00ff0000) /* <16:23> RW:RW:X:N/A */
#define CY_U3P_PROD_SCK_POS                                 (16)


/*
   The IP number of the producing socket to which the consume event shall
   be sent. Use 0x3F to designate ARM CPU (so software) as producer; the
   event will be lost in this case and an interrupt should also be generated
   to observe this condition.
 */
#define CY_U3P_PROD_IP_MASK                                 (0x3f000000) /* <24:29> RW:RW:X:N/A */
#define CY_U3P_PROD_IP_POS                                  (24)


/*
   Enable sending of a produce events from this descriptor only.  Events
   are sent only if SCK_STATUS.en_produce_ev=1.  If 0, events will be suppressed,
   and the descriptor will not be copied back into memory when completed.
 */
#define CY_U3P_EN_PROD_EVENT                                (1u << 30) /* <30:30> RW:RW:X:N/A */


/*
   Enable generation of a produce event interrupt for this descriptor only.
   This interrupt will only be seen by the CPU if SCK_STATUS. int_mask has
   this interrupt enabled as well.  Note that this flag influences the logging
   of the interrupt in SCK_STATUS; it has no effect on the reporting of the
   interrupt to the CPU like SCK_STATUS.int_mask does.
 */
#define CY_U3P_EN_PROD_INT                                  (1u << 31) /* <31:31> RW:RW:X:N/A */

/*
   ############################################################################
   chain Register definition.
   ############################################################################
 */

/*
   Descriptor number of the next task for consumer. A value of 0xFFFF signals
   end of this list.
 */
#define CY_U3P_RD_NEXT_DSCR_MASK                            (0x0000ffff) /* <0:15> RW:RW:X:N/A */
#define CY_U3P_RD_NEXT_DSCR_POS                             (0)


/*
   Descriptor number of the next task for producer. A value of 0xFFFF signals
   end of this list.
 */
#define CY_U3P_WR_NEXT_DSCR_MASK                            (0xffff0000) /* <16:31> RW:RW:X:N/A */
#define CY_U3P_WR_NEXT_DSCR_POS                             (16)

/*
   ############################################################################
   size Register definition.
   ############################################################################
 */

/*
   A marker that is provided by s/w and can be observed by the IP.  It's
   meaning is defined by the IP that uses it.  This bit has no effect on
   the other DMA mechanisms.
 */
#define CY_U3P_MARKER                                       (1u << 0) /* <0:0> RW:RW:X:N/A */


/*
   A marker indicating this descriptor refers to the last buffer of a packet
   or transfer. Packets/transfers may span more than one buffer.  The producing
   IP provides this marker by providing the EOP signal to its DMA adapter.
    The consuming IP observes this marker by inspecting its EOP return signal
   from its own DMA adapter. For more information see section on packets,
   buffers and transfers in DMA chapter.
 */
#define CY_U3P_EOP                                          (1u << 1) /* <1:1> RW:RW:X:N/A */


/*
   Indicates the buffer data is valid (0) or in error (1).
 */
#define CY_U3P_BUFFER_ERROR                                 (1u << 2) /* <2:2> RW:RW:X:N/A */


/*
   Indicates the buffer is in use (1) or empty (0).  A consumer will interpret
   this as:
   0: Buffer is empty, wait until filled.
   1: Buffer has data that can be consumed
   A produce will interpret this as:
   0: Buffer is ready to be filled
   1: Buffer is occupied, wait until empty
 */
#define CY_U3P_BUFFER_OCCUPIED                              (1u << 3) /* <3:3> RW:RW:X:N/A */


/*
   The size of the buffer in multiples of 16 bytes
 */
#define CY_U3P_BUFFER_SIZE_MASK                             (0x0000fff0) /* <4:15> RW:RW:X:N/A */
#define CY_U3P_BUFFER_SIZE_POS                              (4)


/*
   The number of data bytes present in the buffer.  An occupied buffer is
   not always full, in particular when variable length packets are transferred.
 */
#define CY_U3P_BYTE_COUNT_MASK                              (0xffff0000) /* <16:31> RW:RW:X:N/A */
#define CY_U3P_BYTE_COUNT_POS                               (16)



#endif /* _INCLUDED_SOCK_REGS_H_ */

/*[]*/

