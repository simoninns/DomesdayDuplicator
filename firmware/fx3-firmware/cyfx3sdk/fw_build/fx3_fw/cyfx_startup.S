; Cypress FX3 Firmware Startup code (cyfx_startup.s)
; ## =====================================================
; ##
; ##  Copyright Cypress Semiconductor Corporation, 2010-2023,
; ##  All Rights Reserved
; ##  UNPUBLISHED, LICENSED SOFTWARE.
; ##
; ##  CONFIDENTIAL AND PROPRIETARY INFORMATION
; ##  WHICH IS THE PROPERTY OF CYPRESS.
; ##
; ##  Use of this file is governed
; ##  by the license agreement included in the file
; ##
; ##     <install>/license/license.txt
; ##
; ##  where <install> is the Cypress software
; ##  installation root directory path.
; ##
; ## =======================================================

    IMPORT __main
    IMPORT main
    AREA TOOLCHAIN, CODE, READONLY
    EXPORT jump

CY_U3P_GCTL_CONTROL_ADDRESS EQU 0xE0050000
WARM_BOOT_FLAG EQU (1 :SHL: 29)

; The ARM-CC specific tool chain provided init code is part of the __main function.
; Do an unconditional branch to __main if this is a cold boot. Otherwise, just jump to main.
    EXPORT CyU3PToolChainInit
CyU3PToolChainInit
    NOP
    NOP
    LDR     r2, =CY_U3P_GCTL_CONTROL_ADDRESS
    LDR     r0, [r2]
    ANDS    r2, r0, #WARM_BOOT_FLAG
    NOP
    BNE     main
    NOP
    B       __main

jump
    BX  R0

    IMPORT __use_two_region_memory  ; Two region memory has to be enabled as stack and heap are in different regions.

    EXPORT  __user_setup_stackheap
__user_setup_stackheap

    MOV r0, #0
    MOV r2, #0

; The tool chain is not expected to place the stack.
; A heap is not being initialized here, as the library code does not
; use the heap. If the user application requires a heap (makes malloc
; calls), uncomment the code below and set the heap bounds correctly
; in the _HEAP_BOUNDS section below.

;;;    LDR r1, =_HEAP_BOUNDS
;;;    LDR r0, [r1], #4
;;;    LDR r2, [r1]
;;;    MOV r1, #0

    IF  {INTER} = {TRUE}
    BX      lr                  ; Return to caller
    ELSE
    MOV     pc, lr              ; Return to caller
    ENDIF

_HEAP_BOUNDS
    DCD 0x40024000              ; Start address of the heap which grows upwards.
    DCD 0x4002A000              ; Upper bound (limit) of the reserved heap area.

    END

;;[]
