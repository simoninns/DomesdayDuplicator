/*
 ## Cypress FX3 Core Library Header (pib_regs.h)
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

#ifndef _INCLUDED_PIB_REGS_H_
#define _INCLUDED_PIB_REGS_H_

#include <cyu3types.h>

#include <gpif_regs.h>

#define PIB_BASE_ADDR                            (0xe0010000)

typedef struct
{
    uvint32_t config;                             /* 0xe0010000 */
    uvint32_t intr;                               /* 0xe0010004 */
    uvint32_t intr_mask;                          /* 0xe0010008 */
    uvint32_t clock_detect;                       /* 0xe001000c */
    uvint32_t rd_mailbox0;                        /* 0xe0010010 */
    uvint32_t rd_mailbox1;                        /* 0xe0010014 */
    uvint32_t wr_mailbox0;                        /* 0xe0010018 */
    uvint32_t wr_mailbox1;                        /* 0xe001001c */
    uvint32_t error;                              /* 0xe0010020 */
    uvint32_t eop_eot;                            /* 0xe0010024 */
    uvint32_t dll_ctrl;                           /* 0xe0010028 */
    uvint32_t rsrvd0[4085];
    uvint32_t gpif_config;                        /* 0xe0014000 */
    uvint32_t gpif_bus_config;                    /* 0xe0014004 */
    uvint32_t gpif_bus_config2;                   /* 0xe0014008 */
    uvint32_t gpif_ad_config;                     /* 0xe001400c */
    uvint32_t gpif_status;                        /* 0xe0014010 */
    uvint32_t gpif_intr;                          /* 0xe0014014 */
    uvint32_t gpif_intr_mask;                     /* 0xe0014018 */
    uvint32_t gpif_serial_in_config;              /* 0xe001401c */
    uvint32_t gpif_serial_out_config;             /* 0xe0014020 */
    uvint32_t gpif_ctrl_bus_direction;            /* 0xe0014024 */
    uvint32_t gpif_ctrl_bus_default;              /* 0xe0014028 */
    uvint32_t gpif_ctrl_bus_polarity;             /* 0xe001402c */
    uvint32_t gpif_ctrl_bus_toggle;               /* 0xe0014030 */
    uvint32_t gpif_ctrl_bus_select[16];           /* 0xe0014034 */
    uvint32_t gpif_ctrl_count_config;             /* 0xe0014074 */
    uvint32_t gpif_ctrl_count_reset;              /* 0xe0014078 */
    uvint32_t gpif_ctrl_count_limit;              /* 0xe001407c */
    uvint32_t gpif_addr_count_config;             /* 0xe0014080 */
    uvint32_t gpif_addr_count_reset;              /* 0xe0014084 */
    uvint32_t gpif_addr_count_limit;              /* 0xe0014088 */
    uvint32_t gpif_state_count_config;            /* 0xe001408c */
    uvint32_t gpif_state_count_limit;             /* 0xe0014090 */
    uvint32_t gpif_data_count_config;             /* 0xe0014094 */
    uvint32_t gpif_data_count_reset;              /* 0xe0014098 */
    uvint32_t gpif_data_count_limit;              /* 0xe001409c */
    uvint32_t gpif_ctrl_comp_value;               /* 0xe00140a0 */
    uvint32_t gpif_ctrl_comp_mask;                /* 0xe00140a4 */
    uvint32_t gpif_data_comp_value;               /* 0xe00140a8 */
    uvint32_t gpif_data_comp_mask;                /* 0xe00140ac */
    uvint32_t gpif_addr_comp_value;               /* 0xe00140b0 */
    uvint32_t gpif_addr_comp_mask;                /* 0xe00140b4 */
    uvint32_t gpif_data_ctrl;                     /* 0xe00140b8 */
    uvint32_t gpif_ingress_data[4];               /* 0xe00140bc */
    uvint32_t gpif_egress_data[4];                /* 0xe00140cc */
    uvint32_t gpif_ingress_address[4];            /* 0xe00140dc */
    uvint32_t gpif_egress_address[4];             /* 0xe00140ec */
    uvint32_t gpif_thread_config[4];              /* 0xe00140fc */
    uvint32_t gpif_lambda_stat;                   /* 0xe001410c */
    uvint32_t gpif_alpha_stat;                    /* 0xe0014110 */
    uvint32_t gpif_beta_stat;                     /* 0xe0014114 */
    uvint32_t gpif_waveform_ctrl_stat;            /* 0xe0014118 */
    uvint32_t gpif_waveform_switch;               /* 0xe001411c */
    uvint32_t gpif_waveform_switch_timeout;       /* 0xe0014120 */
    uvint32_t gpif_crc_config;                    /* 0xe0014124 */
    uvint32_t gpif_crc_data;                      /* 0xe0014128 */
    uvint32_t gpif_beta_deassert;                 /* 0xe001412c */
    uvint32_t gpif_function[32];                  /* 0xe0014130 */
    uvint32_t rsrvd1[916];
    struct
    {
        uvint32_t waveform0;                      /* 0xe0015000 */
        uvint32_t waveform1;                      /* 0xe0015004 */
        uvint32_t waveform2;                      /* 0xe0015008 */
        uvint32_t waveform3;                      /* 0xe001500c */
    } gpif_left[256];
    struct
    {
        uvint32_t waveform0;                      /* 0xe0016000 */
        uvint32_t waveform1;                      /* 0xe0016004 */
        uvint32_t waveform2;                      /* 0xe0016008 */
        uvint32_t waveform3;                      /* 0xe001600c */
    } gpif_right[256];
    uvint32_t rsrvd2[896];
    uvint32_t pp_id;                              /* 0xe0017e00 */
    uvint32_t pp_init;                            /* 0xe0017e04 */
    uvint32_t pp_config;                          /* 0xe0017e08 */
    uvint32_t reserve0[3];
    uvint32_t pp_sock_wr_config;                  /* 0xe0017e18 */
    uvint32_t pp_intr_mask;                       /* 0xe0017e1c */
    uvint32_t pp_drqr5_mask;                      /* 0xe0017e20 */
    uvint32_t pp_sock_mask;                       /* 0xe0017e24 */
    uvint32_t pp_error;                           /* 0xe0017e28 */
    uvint32_t pp_dma_xfer;                        /* 0xe0017e2c */
    uvint32_t pp_dma_size;                        /* 0xe0017e30 */
    uvint32_t pp_wr_mailbox0;                     /* 0xe0017e34 */
    uvint32_t pp_wr_mailbox1;                     /* 0xe0017e38 */
    uvint32_t pp_mmio_addr;                       /* 0xe0017e3c */
    uvint32_t pp_mmio_data;                       /* 0xe0017e40 */
    uvint32_t pp_mmio;                            /* 0xe0017e44 */
    uvint32_t pp_event;                           /* 0xe0017e48 */
    uvint32_t pp_rd_mailbox0;                     /* 0xe0017e4c */
    uvint32_t pp_rd_mailbox1;                     /* 0xe0017e50 */
    uvint32_t pp_sock_stat;                       /* 0xe0017e54 */
    uvint32_t rsrvd3[42];
    uvint32_t id;                                 /* 0xe0017f00 */
    uvint32_t power;                              /* 0xe0017f04 */
    uvint32_t rsrvd4[62];
    struct
    {
        uvint32_t dscr;                           /* 0xe0018000 */
        uvint32_t size;                           /* 0xe0018004 */
        uvint32_t count;                          /* 0xe0018008 */
        uvint32_t status;                         /* 0xe001800c */
        uvint32_t intr;                           /* 0xe0018010 */
        uvint32_t intr_mask;                      /* 0xe0018014 */
        uvint32_t rsrvd5[2];
        uvint32_t dscr_buffer;                    /* 0xe0018020 */
        uvint32_t dscr_sync;                      /* 0xe0018024 */
        uvint32_t dscr_chain;                     /* 0xe0018028 */
        uvint32_t dscr_size;                      /* 0xe001802c */
        uvint32_t rsrvd6[19];
        uvint32_t event;                          /* 0xe001807c */
    } sck[32];
    uvint32_t rsrvd7[7104];
    uvint32_t sck_intr0;                          /* 0xe001ff00 */
    uvint32_t sck_intr1;                          /* 0xe001ff04 */
    uvint32_t sck_intr2;                          /* 0xe001ff08 */
    uvint32_t sck_intr3;                          /* 0xe001ff0c */
    uvint32_t sck_intr4;                          /* 0xe001ff10 */
    uvint32_t sck_intr5;                          /* 0xe001ff14 */
    uvint32_t sck_intr6;                          /* 0xe001ff18 */
    uvint32_t sck_intr7;                          /* 0xe001ff1c */
} PIB_REGS_T, *PPIB_REGS_T;

#define PIB        ((PPIB_REGS_T) PIB_BASE_ADDR)


/*
   PIB Configuration Register
 */
#define CY_U3P_PIB_CONFIG_ADDRESS                           (0xe0010000)
#define CY_U3P_PIB_CONFIG                                   (*(uvint32_t *)(0xe0010000))
#define CY_U3P_PIB_CONFIG_DEFAULT                           (0xe8000000)

/*
   Provides a device ID as defined in IROS. This field is visible in PP_INIT
   registers. This must be provided by boot ROM.  This register is not writable
   when GCTL_CONTROL.BOOTROM_EN=0 to prevent spoofing.
 */
#define CY_U3P_PIB_DEVICE_ID_MASK                           (0x0000ffff) /* <0:15> R:RW:0:No */
#define CY_U3P_PIB_DEVICE_ID_POS                            (0)


/*
   This field specifies Min value for NRC and NCC timing parameters.
   0 - 3 cycles (default)
   1 - 8 cycles
   PMMC HW waits for this minimum period before looking for start bit after
   a prev. command without response or after a previous response.
   The MIN value for NRC & NCC is 8 per MMC specs.
   It has been observed that certain popular APs voilate this min timing
   spec. This config bit fix will allow PMMC HW to work such APs. FW may
   program this field to 1 if NRC/NCC min is desired at 8 cycles.
   It should be noted that with this fix multicard cannot co exisit on PMMC
   bus..
 */
#define CY_U3P_PIB_PMMC_NRC_NCC_MIN8                        (1u << 21) /* <21:21> R:RW:0:No */


/*
   This field specifies PMMC boot timing.
   00 - Default speed
   01 - High Speed
   10 - Reserved
   11 - Reserved
   This field is used in BOOT state after HW reset or CMD0(GO_PRE_IDLE) and
   CMD0(BOOT_INITIATION) command.
 */
#define CY_U3P_PIB_PMMC_BOOT_HS_TIMING_MASK                 (0x00c00000) /* <22:23> R:RW:0:No */
#define CY_U3P_PIB_PMMC_BOOT_HS_TIMING_POS                  (22)


/*
   This field specifies PMMC boot bus width.
   00 - 1 bit
   01 - 4 bit
   10 - 8 bit
   11 - Reserved
   This field is used in BOOT state after HW reset or CMD0(GO_PRE_IDLE) and
   CMD0(BOOT_INITIATION) command.
 */
#define CY_U3P_PIB_PMMC_BOOT_BUSWIDTH_MASK                  (0x03000000) /* <24:25> R:RW:0:No */
#define CY_U3P_PIB_PMMC_BOOT_BUSWIDTH_POS                   (24)


/*
   This bit enables MMC 4.3/4.4 boot if PCFG selects PMMC mode.
   When set PMMC HW will sense MMC4.3/4.4 boot commands. When this bit is
   zero HW ignores boot commands. FW/BootROM sets this bit. FW does not need
   to clear this bit. This field is used by PMMC HW after a HW reset or CMD0(GO_PRE_IDLE)
   to PMMC block
 */
#define CY_U3P_PIB_EMMC_BOOT_ENABLE                         (1u << 26) /* <26:26> R:RW:0:No */


/*
   FW writes 0 to reset PMMC-registers and PMMC-StateMachine. After clearing
   this bit, firmware sets this bit to bring PMMC out of reset. This reset
   will place PMMC in IDLE state.
   
   This reset should NOT be used during CMD0(GO_IDLE) initialization as it
   can corrupt CMD1 command in flight. This reset should be used only for
   error recovery. PMMC HW will clear all necessary registers for CMD0(GO_IDLE)
   iitialization.
 */
#define CY_U3P_PIB_PMMC_RESETN                              (1u << 27) /* <27:27> R:RW:1:No */


/*
   P-Port configuraiton
   0: GPIF-II mode
   1: P-MMC mode
   The BootROM will interpret the PMODE pins through GPIO and choose the
   appropriate GPIF and P-Port configuration.
 */
#define CY_U3P_PIB_PCFG                                     (1u << 28) /* <28:28> R:RW:0:No */


/*
   Variable indicating initialization mode to Application processor (PP_CONFIG.CFGMODE).
    Cleared by firmware after P-Port is properly initialized and ready to
   transact.
 */
#define CY_U3P_PIB_PP_CFGMODE                               (1u << 29) /* <29:29> RW:RW:1:No */


/*
   Enables the PP_MMIO protocol for accessing individual MMIO registers externally
   over P-Port.  Once disabled this function cannot be re-enabled.
   0: Disabled
   1: Enabled
 */
#define CY_U3P_PIB_MMIO_ENABLE                              (1u << 30) /* <30:30> R:RW0C:1:No */


/*
   Enables the entire P-Port IP.
 */
#define CY_U3P_PIB_ENABLE                                   (1u << 31) /* <31:31> R:RW:1:No */



/*
   PIB Interrupt Request Register
 */
#define CY_U3P_PIB_INTR_ADDRESS                             (0xe0010004)
#define CY_U3P_PIB_INTR                                     (*(uvint32_t *)(0xe0010004))
#define CY_U3P_PIB_INTR_DEFAULT                             (0x00000001)

/*
   Indicates that the RD_MAILBOX is empty and a new message can be written.
    This bit sets when AP writes PP_EVENT.RD_MB_FULL=0.  It must be cleared
   by firmware.
 */
#define CY_U3P_PIB_INTR_RD_MB_EMPTY                         (1u << 0) /* <0:0> RW1S:RW1C:1:Yes */


/*
   Indicates that a message is present in WR_MAILBOX and must be read.  This
   bit sets when AP writes a message in the WR_MAILBOX.  It must be cleared
   by firmware and causes  PP_EVENT.WR_MB_EMPTY to assert.
 */
#define CY_U3P_PIB_INTR_WR_MB_FULL                          (1u << 1) /* <1:1> RW1S:RW1C:0:Yes */


/*
   A MMC interrupt has been received.  The specific interrupt received is
   indicated in PMMC_INTR.
 */
#define CY_U3P_PIB_INTR_PMMC_INTR1                          (1u << 3) /* <3:3> W:R :0:No */


/*
   Indicates that the interrupt is from GPIF block. Consult GPIF_STAT register
 */
#define CY_U3P_PIB_INTR_GPIF_INTERRUPT                      (1u << 4) /* <4:4> W:R:0:No */


/*
   DLL has achieved phase lock. Interrupt clears after writing 1.
 */
#define CY_U3P_PIB_INTR_DLL_LOCKED                          (1u << 5) /* <5:5> W1S:RW1C:0:Yes */


/*
   DLL has lost phase lock. Interrupt clears after writing 1.
 */
#define CY_U3P_PIB_INTR_DLL_LOST_LOCK                       (1u << 6) /* <6:6> W1S:RW1C:0:Yes */


/*
   PIB_CLK is no longer present.  See PIB_CLOCK_DETECT for more details.
 */
#define CY_U3P_PIB_INTR_CLOCK_LOST                          (1u << 7) /* <7:7> W1S:RW1C:0:No */


/*
   AP has written a new value into PP_CONFIG
 */
#define CY_U3P_PIB_INTR_CONFIG_CHANGE                       (1u << 8) /* <8:8> W1S:RW1C:0:Yes */


/*
   Indicates that AP has written to PP_WR_THRESHOLD register
 */
#define CY_U3P_PIB_INTR_WR_THRESHOLD                        (1u << 9) /* <9:9> W1S:RW1C:0:Yes */


/*
   Indicates that AP has written to PP_RD_THRESHOLD register
 */
#define CY_U3P_PIB_INTR_RD_THRESHOLD                        (1u << 10) /* <10:10> W1S:RW1C:0:Yes */


/*
   The socket based link controller encountered an error and needs attention.
    FW clears this bit after handling the error.  The error code is indicated
   in PIB_ERROR.PIB_ERR_CODE
 */
#define CY_U3P_PIB_INTR_PIB_ERR                             (1u << 29) /* <29:29> W1S:RW1C:0:Yes */


/*
   An unrecoverable error occurred in the PMMC controller. FW clears this
   bit after handling the eror. The error code is indicated in PIB_ERROR.MMC_ERR_CODE
 */
#define CY_U3P_PIB_INTR_MMC_ERR                             (1u << 30) /* <30:30> W1S:RW1C:0:Yes */


/*
   An error occurred in the GPIF.  FW clears this bit after handling the
   error.  The error code is indicated in PIB_ERROR.GPIF_ERR_CODE
 */
#define CY_U3P_PIB_INTR_GPIF_ERR                            (1u << 31) /* <31:31> W1S:RW1C:0:Yes */



/*
   PIB Interrupt Mask Register
 */
#define CY_U3P_PIB_INTR_MASK_ADDRESS                        (0xe0010008)
#define CY_U3P_PIB_INTR_MASK                                (*(uvint32_t *)(0xe0010008))
#define CY_U3P_PIB_INTR_MASK_DEFAULT                        (0x00000000)

/*
   Mask for corresponding interrupt in PIB_INTR
 */
#define CY_U3P_PIB_INTR_RD_MB_EMPTY                         (1u << 0) /* <0:0> R:RW:0:No */


/*
   Mask for corresponding interrupt in PIB_INTR
 */
#define CY_U3P_PIB_INTR_WR_MB_FULL                          (1u << 1) /* <1:1> R:RW:0:No */


/*
   Mask for corresponding interrupt in PIB_INTR
 */
#define CY_U3P_PIB_INTR_PMMC_INTR                           (1u << 3) /* <3:3> R:RW:0:No */


/*
   Mask for corresponding interrupt in PIB_INTR
 */
#define CY_U3P_PIB_INTR_GPIF_INTERRUPT                      (1u << 4) /* <4:4> R:RW:0:No */


/*
   Mask for corresponding interrupt in PIB_INTR
 */
#define CY_U3P_PIB_INTR_DLL_LOCKED                          (1u << 5) /* <5:5> R:RW:0:No */


/*
   Mask for corresponding interrupt in PIB_INTR
 */
#define CY_U3P_PIB_INTR_DLL_LOST_LOCK                       (1u << 6) /* <6:6> R:RW:0:No */


/*
   Mask for corresponding interrupt in PIB_INTR
 */
#define CY_U3P_PIB_INTR_CLOCK_LOST                          (1u << 7) /* <7:7> R:RW:0:No */


/*
   Mask for corresponding interrupt in PIB_INTR
 */
#define CY_U3P_PIB_INTR_CONFIG_CHANGE                       (1u << 8) /* <8:8> R:RW:0:No */


/*
   Mask for corresponding interrupt in PIB_INTR
 */
#define CY_U3P_PIB_INTR_WR_THRESHOLD                        (1u << 9) /* <9:9> R:RW:0:No */


/*
   Mask for corresponding interrupt in PIB_INTR
 */
#define CY_U3P_PIB_INTR_RD_THRESHOLD                        (1u << 10) /* <10:10> R:RW:0:No */


/*
   Mask for corresponding interrupt in PIB_INTR
 */
#define CY_U3P_PIB_INTR_PIB_ERR                             (1u << 29) /* <29:29> R:RW:0:No */


/*
   Mask for corresponding interrupt in PIB_INTR
 */
#define CY_U3P_PIB_INTR_MMC_ERR                             (1u << 30) /* <30:30> R:RW:0:No */


/*
   Mask for corresponding interrupt in PIB_INTR
 */
#define CY_U3P_PIB_INTR_GPIF_ERR                            (1u << 31) /* <31:31> R:RW:0:No */



/*
   PIB Clock detector configuration
 */
#define CY_U3P_PIB_CLOCK_DETECT_ADDRESS                     (0xe001000c)
#define CY_U3P_PIB_CLOCK_DETECT                             (*(uvint32_t *)(0xe001000c))
#define CY_U3P_PIB_CLOCK_DETECT_DEFAULT                     (0x00000000)

/*
   Number of busclk cycles for each measurement period.
 */
#define CY_U3P_PIB_BUS_CYCLES_MASK                          (0x0000ffff) /* <0:15> R:RW:0:No */
#define CY_U3P_PIB_BUS_CYCLES_POS                           (0)


/*
   Minimum number of posedges required on PIBCLK pin to declare clock presence
   during each measurement period.
 */
#define CY_U3P_PIB_INTF_CYCLES_MASK                         (0x000f0000) /* <16:19> R:RW:0:No */
#define CY_U3P_PIB_INTF_CYCLES_POS                          (16)


/*
   Indicates latest clock presence measurement
 */
#define CY_U3P_PIB_CLOCK_PRESENT                            (1u << 30) /* <30:30> RW:R:0:No */


/*
   Enables detector
 */
#define CY_U3P_PIB_ENABLE                                   (1u << 31) /* <31:31> R:RW:0:No */



/*
   Read (Egress) Mailbox Register
 */
#define CY_U3P_PIB_RD_MAILBOX0_ADDRESS                      (0xe0010010)
#define CY_U3P_PIB_RD_MAILBOX0                              (*(uvint32_t *)(0xe0010010))
#define CY_U3P_PIB_RD_MAILBOX0_DEFAULT                      (0x00000000)

/*
   Mailbox message from FX3 to Application Processor.
 */
#define CY_U3P_PIB_PP_RD_MAILBOX_L_MASK                     (0xffffffff) /* <0:31> R:RW:0:No */
#define CY_U3P_PIB_PP_RD_MAILBOX_L_POS                      (0)



/*
   Read (Egress) Mailbox Register
 */
#define CY_U3P_PIB_RD_MAILBOX1_ADDRESS                      (0xe0010014)
#define CY_U3P_PIB_RD_MAILBOX1                              (*(uvint32_t *)(0xe0010014))
#define CY_U3P_PIB_RD_MAILBOX1_DEFAULT                      (0x00000000)

/*
   Mailbox message from FX3 to Application Processor.
 */
#define CY_U3P_PIB_PP_RD_MAILBOX_H_MASK                     (0xffffffff) /* <0:31> R:RW:0:No */
#define CY_U3P_PIB_PP_RD_MAILBOX_H_POS                      (0)



/*
   Write (Ingress) Mailbox Register
 */
#define CY_U3P_PIB_WR_MAILBOX0_ADDRESS                      (0xe0010018)
#define CY_U3P_PIB_WR_MAILBOX0                              (*(uvint32_t *)(0xe0010018))
#define CY_U3P_PIB_WR_MAILBOX0_DEFAULT                      (0x00000000)

/*
   Mailbox message from Application Processor to FX3
 */
#define CY_U3P_PIB_PP_WR_MAILBOX_L_MASK                     (0xffffffff) /* <0:31> RW:R:0:No */
#define CY_U3P_PIB_PP_WR_MAILBOX_L_POS                      (0)



/*
   Write (Ingress) Mailbox Register
 */
#define CY_U3P_PIB_WR_MAILBOX1_ADDRESS                      (0xe001001c)
#define CY_U3P_PIB_WR_MAILBOX1                              (*(uvint32_t *)(0xe001001c))
#define CY_U3P_PIB_WR_MAILBOX1_DEFAULT                      (0x00000000)

/*
   Mailbox message from Application Processor to FX3
 */
#define CY_U3P_PIB_PP_WR_MAILBOX_H_MASK                     (0xffffffff) /* <0:31> RW:R:0:No */
#define CY_U3P_PIB_PP_WR_MAILBOX_H_POS                      (0)



/*
   PIB Error Indicator Register
 */
#define CY_U3P_PIB_ERROR_ADDRESS                            (0xe0010020)
#define CY_U3P_PIB_ERROR                                    (*(uvint32_t *)(0xe0010020))
#define CY_U3P_PIB_ERROR_DEFAULT                            (0x00000000)

/*
   The socket based link controller encountered an error and needs attention.
   Error codes are further described in BROS.  Corresponds to interrupt bit
   PIB_ERROR.
 */
#define CY_U3P_PIB_PIB_ERR_CODE_MASK                        (0x0000003f) /* <0:5> RW:R:0:No */
#define CY_U3P_PIB_PIB_ERR_CODE_POS                         (0)

/*
   Write being done to a Read Socket or Read being done to a write skt
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH0_DIR_ERROR               (1)
/*
   Write being done to a Read Socket or Read being done to a write skt
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH1_DIR_ERROR               (2)
/*
   Write being done to a Read Socket or Read being done to a write skt
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH2_DIR_ERROR               (3)
/*
   Write being done to a Read Socket or Read being done to a write skt
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH3_DIR_ERROR               (4)
/*
   Write exceeds the space available in the buffer
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH0_WR_OVERFLOW             (5)
/*
   Write exceeds the space available in the buffer
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH1_WR_OVERFLOW             (6)
/*
   Write exceeds the space available in the buffer
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH2_WR_OVERFLOW             (7)
/*
   Write exceeds the space available in the buffer
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH3_WR_OVERFLOW             (8)
/*
   Reads exceeds the byte count of the buffer
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH0_RD_UNDERRUN             (9)
/*
   Reads exceeds the byte count of the buffer
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH1_RD_UNDERRUN             (10)
/*
   Reads exceeds the byte count of the buffer
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH2_RD_UNDERRUN             (11)
/*
   Reads exceeds the byte count of the buffer
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH3_RD_UNDERRUN             (12)
/*
   Socket has gone inactive within a DMA Transfer
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH0_SCK_ACTIVE              (0x12)
/*
   Adapter Unable to service write request though buffer is available
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH0_ADAP_OVERFLOW           (0x13)
/*
   Adapter Unable to service read request though buffer is available
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH0_ADAP_UNDERFLOW          (0x14)
/*
   A Read socket has been wrapped up
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH0_READ_FORCE_END          (0x15)
/*
   A read socket with burstsize > 0 is switched before 8-byte boundary
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH0_READ_BURST_ERR          (0x16)
/*
   Socket has gone inactive within a DMA Transfer
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH1_SCK_ACTIVE              (0x1a)
/*
   Adapter Unable to service write request though buffer is available
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH1_ADAP_OVERFLOW           (0x1b)
/*
   Adapter Unable to service read request though buffer is available
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH1_ADAP_UNDERFLOW          (0x1c)
/*
   A Read socket has been wrapped up
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH1_READ_FORCE_END          (0x1d)
/*
   A read socket with burstsize > 0 is switched before 8-byte boundary
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH1_READ_BURST_ERR          (0x1e)
/*
   Socket has gone inactive within a DMA Transfer
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH2_SCK_ACTIVE              (0x22)
/*
   Adapter Unable to service write request though buffer is available
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH2_ADAP_OVERFLOW           (0x23)
/*
   Adapter Unable to service read request though buffer is available
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH2_ADAP_UNDERFLOW          (0x24)
/*
   A Read socket has been wrapped up
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH2_READ_FORCE_END          (0x25)
/*
   A read socket with burstsize > 0 is switched before 8-byte boundary
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH2_READ_BURST_ERR          (0x26)
/*
   Socket has gone inactive within a DMA Transfer
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH3_SCK_ACTIVE              (0x2a)
/*
   Adapter Unable to service write request though buffer is available
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH3_ADAP_OVERFLOW           (0x2b)
/*
   Adapter Unable to service read request though buffer is available
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH3_ADAP_UNDERFLOW          (0x2c)
/*
   A Read socket has been wrapped up
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH3_READ_FORCE_END          (0x2d)
/*
   A read socket with burstsize > 0 is switched before 8-byte boundary
 */
#define CY_U3P_PIB_PIB_ERR_CODE_TH3_READ_BURST_ERR          (0x2e)

/*
   An unrecoverable error occurred in the PMMC controller. FW clears this
   bit after handling the eror. The error code is indicated in PIB_ERROR.MMC_ERR_CODE
 */
#define CY_U3P_PIB_MMC_ERR_CODE_MASK                        (0x000003c0) /* <6:9> RW:R:0:No */
#define CY_U3P_PIB_MMC_ERR_CODE_POS                         (6)

/*
   When the buffer is not a multiple of block size
 */
#define CY_U3P_PIB_MMC_ERR_CODE_UNDERRUN                    (1)
/*
   When the buffer is not a multiple of block size
 */
#define CY_U3P_PIB_MMC_ERR_CODE_OVERRUN                     (2)
/*
   Addressed socket not ready for a WRITE command 1st block
 */
#define CY_U3P_PIB_MMC_ERR_CODE_DATA_NOT_READY              (3)
/*
   CRC error on block write targetting register space
 */
#define CY_U3P_PIB_MMC_ERR_CODE_REG_DATA_CRC_ERROR          (6)
/*
   CRC error on block write to sockets
 */
#define CY_U3P_PIB_MMC_ERR_CODE_DMA_DATA_CRC_ERROR          (7)

/*
   Error code for the first error code since ERROR=1
   Error codes are specified in detail the USB30PIB BROS.
 */
#define CY_U3P_PIB_GPIF_ERR_CODE_MASK                       (0x00007c00) /* <10:14> RW:R:0:No */
#define CY_U3P_PIB_GPIF_ERR_CODE_POS                        (10)

/*
   Attempt to push to the active address thread which is not dma_ready
 */
#define CY_U3P_PIB_GPIF_ERR_CODE_IN_ADDR_OVER_WRITE         (1)
/*
   Attempt to push to the active address thread which is not dma_ready
 */
#define CY_U3P_PIB_GPIF_ERR_CODE_EG_ADDR_NOT_VALID          (2)
/*
   Attempt to push to the active address thread which is not dma_ready
 */
#define CY_U3P_PIB_GPIF_ERR_CODE_DMA_DATA_RD_ERROR          (3)
/*
   Attempt to push to the active address thread which is not dma_ready
 */
#define CY_U3P_PIB_GPIF_ERR_CODE_DMA_DATA_WR_ERROR          (4)
/*
   Attempt to push to the active address thread which is not dma_ready
 */
#define CY_U3P_PIB_GPIF_ERR_CODE_DMA_ADDR_RD_ERROR          (5)
/*
   Attempt to push to the active address thread which is not dma_ready
 */
#define CY_U3P_PIB_GPIF_ERR_CODE_DMA_ADDR_WR_ERROR          (6)
/*
   received data in serial mode has parity error
 */
#define CY_U3P_PIB_GPIF_ERR_CODE_SERIAL_PARITY_ERROR        (7)
/*
   statemachine has transitioned to an invalid state
 */
#define CY_U3P_PIB_GPIF_ERR_CODE_INVALID_STATE_ERROR        (8)


/*
   PIB EOP/EOT configuration
 */
#define CY_U3P_PIB_EOP_EOT_ADDRESS                          (0xe0010024)
#define CY_U3P_PIB_EOP_EOT                                  (*(uvint32_t *)(0xe0010024))
#define CY_U3P_PIB_EOP_EOT_DEFAULT                          (0x00000001)

/*
   This register specifies how EOP bits are set or interpretted for Ingress
   and Egress sockets respectively.
   1: Packet mode behavior
   0: Stream mode behavior
   See Architecture Spec for details.
 */
#define CY_U3P_PIB_PIB_EOP_EOT_CFG_MASK                     (0xffffffff) /* <0:31> R:RW:1:Yes */
#define CY_U3P_PIB_PIB_EOP_EOT_CFG_POS                      (0)


/*
   Configure the DLL phases and enable it.
 */
#define CY_U3P_PIB_DLL_CTRL_ADDRESS                         (0xe0010028)
#define CY_U3P_PIB_DLL_CTRL                                 (*(uvint32_t *)(0xe0010028))
#define CY_U3P_PIB_DLL_CTRL_DEFAULT                         (0x0000f8f0)

/*
   Drives the DLLEN input
       0: DLL is disabled (internally power gated)
       1: DLL is enabled
 */
#define CY_U3P_PIB_DLL_ENABLE                               (1u << 0) /* <0:0> R:RW:0:No */


/*
   0: 23-80MHz
   1: 70-230MHz
 */
#define CY_U3P_PIB_DLL_HIGH_FREQ                            (1u << 1) /* <1:1> R:RW:0:No */


/*
   0: DLL is not in phase lock.
   1: DLL has achieved phase lock.
 */
#define CY_U3P_PIB_DLL_DLL_STAT                             (1u << 2) /* <2:2> W:R:0:Yes */


/*
   Selects the phase of clock used in GPIF and PMMC core.
 */
#define CY_U3P_PIB_DLL_CORE_PHASE_SELECT_MASK               (0x000000f0) /* <4:7> R:RW:0xF:No */
#define CY_U3P_PIB_DLL_CORE_PHASE_SELECT_POS                (4)


/*
   Selects one of the 16 phase shifted clocks for the input synchronizer
   paths. Used only for Async protocols.
 */
#define CY_U3P_PIB_DLL_SYNC_PHASE_SELECT_MASK               (0x00000f00) /* <8:11> R:RW:0x8:No */
#define CY_U3P_PIB_DLL_SYNC_PHASE_SELECT_POS                (8)


/*
   Selects one of the 16 phase shifted clocks for the output paths for both
   PMMC and Sync/Async GPIF protocols
 */
#define CY_U3P_PIB_DLL_OUTPUT_PHASE_SELECT_MASK             (0x0000f000) /* <12:15> R:RW:0xF:No */
#define CY_U3P_PIB_DLL_OUTPUT_PHASE_SELECT_POS              (12)


/*
   DLL Operation mode for the PIB master DLL. This field is used only for
   DLL characterization. During normal operation the only allowed value for
   this field is "0".
     0: DLL is master mode. The DLL locks to its input clock
     1: DLL is in slave mode. The DLL creates delays based on the values
   specified in DLL_SLAVE_DLY register field
 */
#define CY_U3P_PIB_DLL_DLL_MODE                             (1u << 16) /* <16:16> R:RW:0:No */


/*
   DLL Slave mode delay control - to be used only for characterization in
   conjunction with the DLL_MODE field in this register.
 */
#define CY_U3P_PIB_DLL_DLL_SLAVE_DLY_MASK                   (0x07fe0000) /* <17:26> R:RW:0:No */
#define CY_U3P_PIB_DLL_DLL_SLAVE_DLY_POS                    (17)


/*
   Selects the mode of operation of the DLL per the following. When the DLL
   outputs are selected to be observed in the clock_observation_mode test
   mode, this field can be used to control the DLL's DFT behavior. In normal
   operation mode of the chip, only the "000" setting is allowed.  When the
   DFT modes are selected, the relevant DLL observation signals are available
   for monitoring on the chip outputs as specified in the description of
   the clock_observation_mode.
   000 Normal Operation
   111 Slave mode with user configured delay.
 */
#define CY_U3P_PIB_DLL_DLL_DFT_MODE_MASK                    (0x38000000) /* <27:29> R:RW:0:No */
#define CY_U3P_PIB_DLL_DLL_DFT_MODE_POS                     (27)


/*
   Resets the DLL.
      0:  DLL is reset
      1:  DLL is not reset
 */
#define CY_U3P_PIB_DLL_DLL_RESET_N                          (1u << 30) /* <30:30> R:RW:0:No */


/*
   0 - Hardware does not reset DLL when DLL code overrun/under-run occurs
   1 - Hardware resets DLL when DLL code overrun/under-run occurs
 */
#define CY_U3P_PIB_DLL_ENABLE_RESET_ON_ERR                  (1u << 31) /* <31:31> R:RW:0:No */



/*
   P-Port Device ID Register
 */
#define CY_U3P_PIB_PP_ID_ADDRESS                            (0xe0017e00)
#define CY_U3P_PIB_PP_ID                                    (*(uvint32_t *)(0xe0017e00))
#define CY_U3P_PIB_PP_ID_DEFAULT                            (0x00000000)

/*
   Provides a device ID as defined in IROS.
 */
#define CY_U3P_PIB_DEVICE_ID_MASK                           (0x0000ffff) /* <0:15> RW:R:0:No */
#define CY_U3P_PIB_DEVICE_ID_POS                            (0)



/*
   P-Port reset and power control
 */
#define CY_U3P_PIB_PP_INIT_ADDRESS                          (0xe0017e04)
#define CY_U3P_PIB_PP_INIT                                  (*(uvint32_t *)(0xe0017e04))
#define CY_U3P_PIB_PP_INIT_DEFAULT                          (0x00000c01)

/*
   Indicates system woke up through a power-on-reset or RESET# pin reset
   sequence. If firmware does not clear this bit it will stay 1 even through
   software reset, standby and suspend sequences.  This bit is a shadow bit
   of GCTL_CONTROL.
 */
#define CY_U3P_PIB_POR                                      (1u << 0) /* <0:0> RW:R:1:No */


/*
   Indicates system woke up from a s/w induced hard reset sequence (from
   GCTL_CONTROL.HARD_RESET_N or PP_INIT.HARD_RESET_N).  If firmware does
   not clear this bit it will stay 1 even through standby and suspend sequences.
   This bit is a shadow bit of GCTL_CONTROL.
 */
#define CY_U3P_PIB_SW_RESET                                 (1u << 1) /* <1:1> RW:R:0:No */


/*
   Indicates system woke up from a watchdog timer induced hard reset (see
   GCTL_WATCHDOG_CS).  If firmware does not clear this bit it will stay 1
   even through standby and suspend sequences. This bit is a shadow bit of
   GCTL_CONTROL.
 */
#define CY_U3P_PIB_WDT_RESET                                (1u << 2) /* <2:2> RW:R:0:No */


/*
   Indicates system woke up from standby mode (see architecture spec for
   details). If firmware does not clear this bit it will stay 1 even through
   suspend sequences. This bit is a shadow bit of GCTL_CONTROL.
 */
#define CY_U3P_PIB_WAKEUP_PWR                               (1u << 3) /* <3:3> RW:R:0:No */


/*
   Indicates system woke up from suspend state (see architecture spec for
   details). If firmware does not clear this bit it will stay 1 even through
   standby sequences. This bit is a shadow bit of GCTL_CONTROL.
 */
#define CY_U3P_PIB_WAKEUP_CLK                               (1u << 4) /* <4:4> RW:R:0:No */


/*
   Software clears this bit to effect a CPU reset (aka reboot).  No other
   blocks or registers are affected.  The CPU will enter the boot ROM, that
   will use the WARM_BOOT flag to determine whether to reload firmware.
   Unlike the same bit in GCTL_CONTROL, the software needs to explicitly
   clear and then set this bit to bring the internal CPU out of reset.  It
   is permissible to keep the ARM CPU in reset for an extended period of
   time (although not advisable).
 */
#define CY_U3P_PIB_CPU_RESET_N                              (1u << 10) /* <10:10> RW:R:1:No */


/*
   Software clears this bit to effect a global hard reset (all blocks, all
   flops).  This is equivalent to toggling the RESET pin on the device. 
   This function is also available to internal firmware in GCTL_CONTROL.
 */
#define CY_U3P_PIB_HARD_RESET_N                             (1u << 11) /* <11:11> RW:R:1:No */


/*
   0: P-Port is Little Endian
   1: P-Port is Big Endian
 */
#define CY_U3P_PIB_BIG_ENDIAN                               (1u << 15) /* <15:15> RW:R:0:No */



/*
   P-Port Configuration Register
 */
#define CY_U3P_PIB_PP_CONFIG_ADDRESS                        (0xe0017e08)
#define CY_U3P_PIB_PP_CONFIG                                (*(uvint32_t *)(0xe0017e08))
#define CY_U3P_PIB_PP_CONFIG_DEFAULT                        (0x0000004f)

/*
   Size of DMA bursts; only relevant when DRQMODE=1.
   0-14:  DMA burst size is 2BURSTSIZE words
   15: DMA burst size is infinite (DRQ de-asserts on last cycle of transfer)
 */
#define CY_U3P_PIB_BURSTSIZE_MASK                           (0x0000000f) /* <0:3> RW:R:15:No */
#define CY_U3P_PIB_BURSTSIZE_POS                            (0)


/*
   Initialization Mode
   0: Normal operation mode.
   1: Initialization mode.
   This bit is cleared to “0” by firmware, by writing 0 to PIB_CONFIG.PP_CFGMODE,
   after completing the initialization process.  Specific usage of this bit
   is described in the software architecure.  This bit is mirrored directly
   by HW in PIB_CONFIG.
 */
#define CY_U3P_PIB_CFGMODE                                  (1u << 6) /* <6:6> RW:R:1:No */


/*
   DMA signaling mode. See DMA section for more information.
   0: Pulse mode, DRQ will de-assert when DACK de-asserts and will remain
   de-asserted for a specified time (see EROS).  After that DRQ may re-assert
   depending on other settings.
   1: Burst mode, DRQ will de-assert when BURSTSIZE words are transferred
   and will not re-assert until DACK is de-asserted.
 */
#define CY_U3P_PIB_DRQMODE                                  (1u << 7) /* <7:7> RW:R:0:No */


/*
   0: No override
   1: INTR signal is forced to INTR_VALUE
   This bit is used directly in HW to generate INT signal.
 */
#define CY_U3P_PIB_INTR_OVERRIDE                            (1u << 9) /* <9:9> RW:R:0:No */


/*
   0: INTR is de-asserted when INTR_OVERRIDE=1
   1: INTR is asserted on override when INTR_OVERRIDE=1
   This bit is used directly in HW to generate INT signal.
 */
#define CY_U3P_PIB_INTR_VALUE                               (1u << 10) /* <10:10> RW:R:0:No */


/*
   0: INTR is active low
   1: INTR is active high
   This bit is used directly in HW to generate INT signal.
 */
#define CY_U3P_PIB_INTR_POLARITY                            (1u << 11) /* <11:11> RW:R:0:No */


/*
   0: No override
   1: DRQ signal is forced to DRQ_VALUE
 */
#define CY_U3P_PIB_DRQ_OVERRIDE                             (1u << 12) /* <12:12> RW:R:0:No */


/*
   0: DRQ is de-asserted when DRQ_OVERRIDE=1
   1: DRQ is asserted when DRQ_OVERRIDE=1
 */
#define CY_U3P_PIB_DRQ_VALUE                                (1u << 13) /* <13:13> RW:R:0:No */


/*
   0: DRQ is active low
   1: DRQ is active high
 */
#define CY_U3P_PIB_DRQ_POLARITY                             (1u << 14) /* <14:14> RW:R:0:No */


/*
   0: DACK is active low
   1: DACK is active high
 */
#define CY_U3P_PIB_DACK_POLARITY                            (1u << 15) /* <15:15> RW:R:0:No */



/*
   P-port PMMC socket write config
 */
#define CY_U3P_PIB_PP_SOCK_WR_CONFIG_ADDRESS                (0xe0017e18)
#define CY_U3P_PIB_PP_SOCK_WR_CONFIG                        (*(uvint32_t *)(0xe0017e18))
#define CY_U3P_PIB_PP_SOCK_WR_CONFIG_DEFAULT                (0x00000000)

/*
   Socket configuration, one bit per socketl. This config bit indicates which
   socket may send partially valid  data over fixed size blocks.
   0: All data in the block is forwarded
   1: Only DATA_SIZE bytes of a block are accepted during write operation.
   .For NoTA configuration the lower 16 bits are used
 */
#define CY_U3P_PIB_SOCK_WR_CFG_MASK                         (0xffffffff) /* <0:31> RW:R:0:No */
#define CY_U3P_PIB_SOCK_WR_CFG_POS                          (0)



/*
   P-Port Interrupt Mask Register
 */
#define CY_U3P_PIB_PP_INTR_MASK_ADDRESS                     (0xe0017e1c)
#define CY_U3P_PIB_PP_INTR_MASK                             (*(uvint32_t *)(0xe0017e1c))
#define CY_U3P_PIB_PP_INTR_MASK_DEFAULT                     (0x00002000)

/*
   1: Forward EVENT onto INT line
 */
#define CY_U3P_PIB_SOCK_AGG_AL                              (1u << 0) /* <0:0> RW:R:0:No */


/*
   1: Forward EVENT onto INT line
 */
#define CY_U3P_PIB_SOCK_AGG_AH                              (1u << 1) /* <1:1> RW:R:0:No */


/*
   1: Forward EVENT onto INT line
 */
#define CY_U3P_PIB_SOCK_AGG_BL                              (1u << 2) /* <2:2> RW:R:0:No */


/*
   1: Forward EVENT onto INT line
 */
#define CY_U3P_PIB_SOCK_AGG_BH                              (1u << 3) /* <3:3> RW:R:0:No */


/*
   1: Forward EVENT onto INT line
 */
#define CY_U3P_PIB_GPIF_INT                                 (1u << 4) /* <4:4> RW:R:0:No */


/*
   1: Forward EVENT onto INT line
 */
#define CY_U3P_PIB_PIB_ERR                                  (1u << 5) /* <5:5> RW:R:0:No */


/*
   1: Forward EVENT onto INT line
 */
#define CY_U3P_PIB_MMC_ERR                                  (1u << 6) /* <6:6> RW:R:0:No */


/*
   1: Forward EVENT onto INT line
 */
#define CY_U3P_PIB_GPIF_ERR                                 (1u << 7) /* <7:7> RW:R:0:No */


/*
   1: Forward EVENT onto INT line
 */
#define CY_U3P_PIB_DMA_WMARK_EV                             (1u << 11) /* <11:11> RW:R:0:No */


/*
   1: Forward EVENT onto INT line
 */
#define CY_U3P_PIB_DMA_READY_EV                             (1u << 12) /* <12:12> RW:R:0:No */


/*
   1: Forward EVENT onto INT line
 */
#define CY_U3P_PIB_RD_MB_FULL                               (1u << 13) /* <13:13> RW:R:1:No */


/*
   1: Forward EVENT onto INT line
 */
#define CY_U3P_PIB_WR_MB_EMPTY                              (1u << 14) /* <14:14> RW:R:0:No */


/*
   1: Forward EVENT onto INT line
 */
#define CY_U3P_PIB_WAKEUP                                   (1u << 15) /* <15:15> RW:R:0:No */



/*
   P-Port DRQ/R5 Mask Register
 */
#define CY_U3P_PIB_PP_DRQR5_MASK_ADDRESS                    (0xe0017e20)
#define CY_U3P_PIB_PP_DRQR5_MASK                            (*(uvint32_t *)(0xe0017e20))
#define CY_U3P_PIB_PP_DRQR5_MASK_DEFAULT                    (0x00002000)

/*
   1: Forward EVENT onto DRQ line
 */
#define CY_U3P_PIB_SOCK_AGG_AL                              (1u << 0) /* <0:0> RW:R:0:No */


/*
   1: Forward EVENT onto DRQ line
 */
#define CY_U3P_PIB_SOCK_AGG_AH                              (1u << 1) /* <1:1> RW:R:0:No */


/*
   1: Forward EVENT onto DRQ line
 */
#define CY_U3P_PIB_SOCK_AGG_BL                              (1u << 2) /* <2:2> RW:R:0:No */


/*
   1: Forward EVENT onto DRQ line
 */
#define CY_U3P_PIB_SOCK_AGG_BH                              (1u << 3) /* <3:3> RW:R:0:No */


/*
   1: Forward EVENT onto DRQ line
 */
#define CY_U3P_PIB_GPIF_INT                                 (1u << 4) /* <4:4> RW:R:0:No */


/*
   1: Forward EVENT onto DRQ line
 */
#define CY_U3P_PIB_PIB_ERR                                  (1u << 5) /* <5:5> RW:R:0:No */


/*
   1: Forward EVENT onto DRQ line
 */
#define CY_U3P_PIB_MMC_ERR                                  (1u << 6) /* <6:6> RW:R:0:No */


/*
   1: Forward EVENT onto DRQ line
 */
#define CY_U3P_PIB_GPIF_ERR                                 (1u << 7) /* <7:7> RW:R:0:No */


/*
   1: Forward EVENT onto DRQ line
 */
#define CY_U3P_PIB_DMA_WMARK_EV                             (1u << 11) /* <11:11> RW:R:0:No */


/*
   1: Forward EVENT onto DRQ line
 */
#define CY_U3P_PIB_DMA_READY_EV                             (1u << 12) /* <12:12> RW:R:0:No */


/*
   1: Forward EVENT onto DRQ line
 */
#define CY_U3P_PIB_RD_MB_FULL                               (1u << 13) /* <13:13> RW:R:1:No */


/*
   1: Forward EVENT onto DRQ line
 */
#define CY_U3P_PIB_WR_MB_EMPTY                              (1u << 14) /* <14:14> RW:R:0:No */


/*
   1: Forward EVENT onto DRQ line
 */
#define CY_U3P_PIB_WAKEUP                                   (1u << 15) /* <15:15> RW:R:0:No */



/*
   P-Port Socket Mask Register
 */
#define CY_U3P_PIB_PP_SOCK_MASK_ADDRESS                     (0xe0017e24)
#define CY_U3P_PIB_PP_SOCK_MASK                             (*(uvint32_t *)(0xe0017e24))
#define CY_U3P_PIB_PP_SOCK_MASK_DEFAULT                     (0x00000000)

/*
   For socket <x>, bit <x> indicates:
   0: Socket does not affect SOCK_AGG_A/B
   1: Socket does affect SOCK_AGG_A/B
 */
#define CY_U3P_PIB_SOCK_MASK_MASK                           (0xffffffff) /* <0:31> RW:R:0:No */
#define CY_U3P_PIB_SOCK_MASK_POS                            (0)



/*
   P-Port Error Indicator Register
 */
#define CY_U3P_PIB_PP_ERROR_ADDRESS                         (0xe0017e28)
#define CY_U3P_PIB_PP_ERROR                                 (*(uvint32_t *)(0xe0017e28))
#define CY_U3P_PIB_PP_ERROR_DEFAULT                         (0x00000000)

/*
   Mirror of corresponding field in PIB_ERROR
 */
#define CY_U3P_PIB_PIB_ERR_CODE_MASK                        (0x0000003f) /* <0:5> RW:R:0:No */
#define CY_U3P_PIB_PIB_ERR_CODE_POS                         (0)


/*
   Mirror of corresponding field in PIB_ERROR
 */
#define CY_U3P_PIB_MMC_ERR_CODE_MASK                        (0x000003c0) /* <6:9> RW:R:0:No */
#define CY_U3P_PIB_MMC_ERR_CODE_POS                         (6)


/*
   Mirror of corresponding field in PIB_ERROR
 */
#define CY_U3P_PIB_GPIF_ERR_CODE_MASK                       (0x00007c00) /* <10:14> RW:R:0:No */
#define CY_U3P_PIB_GPIF_ERR_CODE_POS                        (10)



/*
   P-Port DMA Transfer Register
 */
#define CY_U3P_PIB_PP_DMA_XFER_ADDRESS                      (0xe0017e2c)
#define CY_U3P_PIB_PP_DMA_XFER                              (*(uvint32_t *)(0xe0017e2c))
#define CY_U3P_PIB_PP_DMA_XFER_DEFAULT                      (0x00000000)

/*
   Processor specified socket number for data transfer
 */
#define CY_U3P_PIB_DMA_SOCK_MASK                            (0x000000ff) /* <0:7> RW:R:0:No */
#define CY_U3P_PIB_DMA_SOCK_POS                             (0)


/*
   0: Disable ongoing transfer. If no transfer is ongoing ignore disable
   1: Enable data transfer
 */
#define CY_U3P_PIB_DMA_ENABLE                               (1u << 8) /* <8:8> RW:R:0:No */


/*
   0: Read (Transfer from FX3 -> Egress direction)
   1: Write (Transfer to FX3 -> Ingress direction)
 */
#define CY_U3P_PIB_DMA_DIRECTION                            (1u << 9) /* <9:9> RW:R:0:No */


/*
   0: Short Transfer (DMA_ENABLE clears at end of buffer
   1: Long Transfer (DMA_ENABLE must be cleared by AP at end of transfer)
 */
#define CY_U3P_PIB_LONG_TRANSFER                            (1u << 10) /* <10:10> RW:R:0:No */


/*
   Indicates that DMA_SIZE value is valid and corresponds to the socket selected
   in PP_DMA_XFER.  SIZE_VALID will be 0 for a short period after PP_DMA_XFER
   is written into.  AP shall poll SIZE_VALID or DMA_READY before reading
   DMA_SIZE
 */
#define CY_U3P_PIB_SIZE_VALID                               (1u << 12) /* <12:12> RW:R:0:No */


/*
   Indicates that link controller is busy processing a transfer. A zero length
   transfer would cause DMA_READY to never assert.
   0: No DMA is in progress
   1: DMA is busy
 */
#define CY_U3P_PIB_DMA_BUSY                                 (1u << 13) /* <13:13> RW:R:0:No */


/*
   0: No errors
   1: DMA transfer error
   This bit is set when a DMA error occurs and cleared when the next transfer
   is started using DMA_ENABLE=1.
 */
#define CY_U3P_PIB_DMA_ERROR                                (1u << 14) /* <14:14> RW:R:0:No */


/*
   Indicates that the link controller is ready to exchange data.
   0: Socket not ready for transfer
   1: Socket ready for transfer; SIZE_VALID is also guaranteed 1
 */
#define CY_U3P_PIB_DMA_READY                                (1u << 15) /* <15:15> RW:R:0:No */



/*
   P-Port DMA Transfer Size Register
 */
#define CY_U3P_PIB_PP_DMA_SIZE_ADDRESS                      (0xe0017e30)
#define CY_U3P_PIB_PP_DMA_SIZE                              (*(uvint32_t *)(0xe0017e30))
#define CY_U3P_PIB_PP_DMA_SIZE_DEFAULT                      (0x00000000)

/*
   Size of DMA transfer. Number of bytes available for read/write when read,
   number of bytes to be read/written when written.
 */
#define CY_U3P_PIB_DMA_SIZE_MASK                            (0x0000ffff) /* <0:15> RW:R:0:No */
#define CY_U3P_PIB_DMA_SIZE_POS                             (0)



/*
   P-Port Write (Ingress) Mailbox Registers
 */
#define CY_U3P_PIB_PP_WR_MAILBOX0_ADDRESS                   (0xe0017e34)
#define CY_U3P_PIB_PP_WR_MAILBOX0                           (*(uvint32_t *)(0xe0017e34))
#define CY_U3P_PIB_PP_WR_MAILBOX0_DEFAULT                   (0x00000000)

/*
   Write mailbox message from AP
 */
#define CY_U3P_PIB_WR_MAILBOX_L_MASK                        (0xffffffff) /* <0:31> RW:R:0:No */
#define CY_U3P_PIB_WR_MAILBOX_L_POS                         (0)



/*
   P-Port Write (Ingress) Mailbox Registers
 */
#define CY_U3P_PIB_PP_WR_MAILBOX1_ADDRESS                   (0xe0017e38)
#define CY_U3P_PIB_PP_WR_MAILBOX1                           (*(uvint32_t *)(0xe0017e38))
#define CY_U3P_PIB_PP_WR_MAILBOX1_DEFAULT                   (0x00000000)

/*
   Write mailbox message from AP
 */
#define CY_U3P_PIB_WR_MAILBOX_H_MASK                        (0xffffffff) /* <0:31> RW:R:0:No */
#define CY_U3P_PIB_WR_MAILBOX_H_POS                         (0)



/*
   P-Port MMIO Address Registers
 */
#define CY_U3P_PIB_PP_MMIO_ADDR_ADDRESS                     (0xe0017e3c)
#define CY_U3P_PIB_PP_MMIO_ADDR                             (*(uvint32_t *)(0xe0017e3c))
#define CY_U3P_PIB_PP_MMIO_ADDR_DEFAULT                     (0x00000000)

/*
   Address in MMIO register space to be used for access.
 */
#define CY_U3P_PIB_MMIO_ADDR_MASK                           (0xffffffff) /* <0:31> RW:R:0:No */
#define CY_U3P_PIB_MMIO_ADDR_POS                            (0)



/*
   P-Port MMIO Data Registers
 */
#define CY_U3P_PIB_PP_MMIO_DATA_ADDRESS                     (0xe0017e40)
#define CY_U3P_PIB_PP_MMIO_DATA                             (*(uvint32_t *)(0xe0017e40))
#define CY_U3P_PIB_PP_MMIO_DATA_DEFAULT                     (0x00000000)

/*
   32-bit data word for read or write transaction
 */
#define CY_U3P_PIB_MMIO_DATA_MASK                           (0xffffffff) /* <0:31> RW:R:0:No */
#define CY_U3P_PIB_MMIO_DATA_POS                            (0)



/*
   P-Port MMIO Control Registers
 */
#define CY_U3P_PIB_PP_MMIO_ADDRESS                          (0xe0017e44)
#define CY_U3P_PIB_PP_MMIO                                  (*(uvint32_t *)(0xe0017e44))
#define CY_U3P_PIB_PP_MMIO_DEFAULT                          (0x00000000)

/*
   0: No action
   1: Initiate read from MMIO_ADDR, place data in MMIO_DATA
 */
#define CY_U3P_PIB_MMIO_RD                                  (1u << 0) /* <0:0> RW:R:0:No */


/*
   0: No action
   1: Initiate write of MMIO_DATA to MMIO_ADDR
 */
#define CY_U3P_PIB_MMIO_WR                                  (1u << 1) /* <1:1> RW:R:0:No */


/*
   0: No MMIO operation is pending
   1: MMIO operation is being executed
 */
#define CY_U3P_PIB_MMIO_BUSY                                (1u << 2) /* <2:2> RW:R:0:No */


/*
   0: Normal
   1: MMIO operation was not executed because of security contraints (PIB_CONFIG.MMIO_ENABLE=0)
 */
#define CY_U3P_PIB_MMIO_FAIL                                (1u << 3) /* <3:3> RW:R:0:No */



/*
   P-Port Event Register
 */
#define CY_U3P_PIB_PP_EVENT_ADDRESS                         (0xe0017e48)
#define CY_U3P_PIB_PP_EVENT                                 (*(uvint32_t *)(0xe0017e48))
#define CY_U3P_PIB_PP_EVENT_DEFAULT                         (0x00004000)

/*
   0 : SOCK_STAT_A[7:0] is all zeroes
   1: At least one bit set in SOCK_STAT_A[7:0]
 */
#define CY_U3P_PIB_SOCK_AGG_AL                              (1u << 0) /* <0:0> RW:R:0:No */


/*
   0 : SOCK_STAT_A[15:8] is all zeroes
   1: At least one bit set in SOCK_STAT_A[15:8]
 */
#define CY_U3P_PIB_SOCK_AGG_AH                              (1u << 1) /* <1:1> RW:R:0:No */


/*
   0 : SOCK_STAT_B[7:0] is all zeroes
   1: At least one bit set in SOCK_STAT_B[7:0]
 */
#define CY_U3P_PIB_SOCK_AGG_BL                              (1u << 2) /* <2:2> RW:R:0:No */


/*
   0 : SOCK_STAT_B[15:8] is all zeroes
   1: At least one bit set in SOCK_STAT_B[15:8]
 */
#define CY_U3P_PIB_SOCK_AGG_BH                              (1u << 3) /* <3:3> RW:R:0:No */


/*
   1: State machine raised host interrupt
 */
#define CY_U3P_PIB_GPIF_INT                                 (1u << 4) /* <4:4> RW:R:0:No */


/*
   The socket based link controller encountered an error and needs attention.
    FW clears this bit after handling the error.  The error code is indicated
   in PP_ERROR.PIB_ERR_CODE
 */
#define CY_U3P_PIB_PIB_ERR                                  (1u << 5) /* <5:5> RW:R:0:No */


/*
   An unrecoverable error occurred in the PMMC controller. FW clears this
   bit after handling the eror. The error code is indicated in PP_ERROR.MMC_ERR_CODE
 */
#define CY_U3P_PIB_MMC_ERR                                  (1u << 6) /* <6:6> RW:R:0:No */


/*
   An error occurred in the GPIF.  FW clears this bit after handling the
   error.  The error code is indicated in PP_ERROR.GPIF_ERR_CODE
 */
#define CY_U3P_PIB_GPIF_ERR                                 (1u << 7) /* <7:7> RW:R:0:No */


/*
   Usage of DMA_WMARK is explained in PAS.
   0: P-Port has fewer than <watermark> words left (can be 0)
   1: P-Port is ready for transfer and at least <watermark> words remain
 */
#define CY_U3P_PIB_DMA_WMARK_EV                             (1u << 11) /* <11:11> RW:R:0:No */


/*
   Usage of DMA_READY is explained in PAS.
   0: P-port not ready for data transfer
   1: P-port ready for data transfer
 */
#define CY_U3P_PIB_DMA_READY_EV                             (1u << 12) /* <12:12> RW:R:0:No */


/*
   1: RD Mailbox is full - message must be read
 */
#define CY_U3P_PIB_RD_MB_FULL                               (1u << 13) /* <13:13> RW:R:0:No */


/*
   1: WR Mailbox is empty - message can be written
   This field is cleared by PIB when message is written to MBX, but can also
   be cleared by AP when used as interrupt.  This field is set by PIB only
   once when MBX is emptied by firmware.
 */
#define CY_U3P_PIB_WR_MB_EMPTY                              (1u << 14) /* <14:14> RW:R:1:No */


/*
   0: No wakeup event
   1: FX3 returned from standby mode, signal must be cleared by software
 */
#define CY_U3P_PIB_WAKEUP                                   (1u << 15) /* <15:15> RW:R:0:No */



/*
   P-Port Read (Egress) Mailbox Registers
 */
#define CY_U3P_PIB_PP_RD_MAILBOX0_ADDRESS                   (0xe0017e4c)
#define CY_U3P_PIB_PP_RD_MAILBOX0                           (*(uvint32_t *)(0xe0017e4c))
#define CY_U3P_PIB_PP_RD_MAILBOX0_DEFAULT                   (0x00000000)

/*
   Read mailbox message to AP
 */
#define CY_U3P_PIB_RD_MAILBOX_L_MASK                        (0xffffffff) /* <0:31> RW:R:0:No */
#define CY_U3P_PIB_RD_MAILBOX_L_POS                         (0)



/*
   P-Port Read (Egress) Mailbox Registers
 */
#define CY_U3P_PIB_PP_RD_MAILBOX1_ADDRESS                   (0xe0017e50)
#define CY_U3P_PIB_PP_RD_MAILBOX1                           (*(uvint32_t *)(0xe0017e50))
#define CY_U3P_PIB_PP_RD_MAILBOX1_DEFAULT                   (0x00000000)

/*
   Read mailbox message to AP
 */
#define CY_U3P_PIB_RD_MAILBOX_H_MASK                        (0xffffffff) /* <0:31> RW:R:0:No */
#define CY_U3P_PIB_RD_MAILBOX_H_POS                         (0)



/*
   P-Port Socket Status Register
 */
#define CY_U3P_PIB_PP_SOCK_STAT_ADDRESS                     (0xe0017e54)
#define CY_U3P_PIB_PP_SOCK_STAT                             (*(uvint32_t *)(0xe0017e54))
#define CY_U3P_PIB_PP_SOCK_STAT_DEFAULT                     (0x00000000)

/*
   For socket <x>, bit <x> indicates:
   0: Socket has no active descriptor or descriptor is not available (empty
   for write, occupied for read)
   1: Socket is available for reading or writing
 */
#define CY_U3P_PIB_SOCK_STAT_MASK                           (0xffffffff) /* <0:31> RW:R:0:No */
#define CY_U3P_PIB_SOCK_STAT_POS                            (0)



/*
   Block Identification and Version Number
 */
#define CY_U3P_PIB_ID_ADDRESS                               (0xe0017f00)
#define CY_U3P_PIB_ID                                       (*(uvint32_t *)(0xe0017f00))
#define CY_U3P_PIB_ID_DEFAULT                               (0x00010001)

/*
   A unique number identifying the IP in the memory space.
 */
#define CY_U3P_PIB_BLOCK_ID_MASK                            (0x0000ffff) /* <0:15> R:R:0x0001:N/A */
#define CY_U3P_PIB_BLOCK_ID_POS                             (0)


/*
   Version number for the IP.
 */
#define CY_U3P_PIB_BLOCK_VERSION_MASK                       (0xffff0000) /* <16:31> R:R:0x0001:N/A */
#define CY_U3P_PIB_BLOCK_VERSION_POS                        (16)



/*
   Power, clock and reset control
 */
#define CY_U3P_PIB_POWER_ADDRESS                            (0xe0017f04)
#define CY_U3P_PIB_POWER                                    (*(uvint32_t *)(0xe0017f04))
#define CY_U3P_PIB_POWER_DEFAULT                            (0x00000000)

/*
   For blocks that must perform initialization after reset before becoming
   operational, this signal will remain de-asserted until initialization
   is complete.  In other words reading active=1 indicates block is initialized
   and ready for operation.
 */
#define CY_U3P_PIB_ACTIVE                                   (1u << 0) /* <0:0> W:R:0:No */


/*
   Active LOW reset signal for all logic in the block.  Note that reset is
   active on all flops in the block when either system reset is asserted
   (RESET# pin or SYSTEM_POWER.RESETN is asserted) or this signal is active.
   After setting this bit to 1, firmware shall poll and wait for the ‘active’
   bit to assert.  Reading ‘1’ from ‘resetn’ does not indicate the block
   is out of reset – this may take some time depending on initialization
   tasks and clock frequencies.
   This reset causes PMMC to get into PRE_IDLE state.
 */
#define CY_U3P_PIB_RESETN                                   (1u << 31) /* <31:31> R:RW:0:No */



/*
   Descriptor Chain Pointer
 */
#define CY_U3P_PIB_SCK_DSCR_ADDRESS(n)                      (0xe0018000 + ((n) * (0x0080)))
#define CY_U3P_PIB_SCK_DSCR(n)                              (*(uvint32_t *)(0xe0018000 + ((n) * 0x0080)))
#define CY_U3P_PIB_SCK_DSCR_DEFAULT                         (0x00000000)

/*
   Descriptor number of currently active descriptor.  A value of 0xFFFF designates
   no (more) active descriptors available.  When activating a socket CPU
   shall write number of first descriptor in here. Only modify this field
   when go_suspend=1 or go_enable=0
 */
#define CY_U3P_PIB_DSCR_NUMBER_MASK                         (0x0000ffff) /* <0:15> RW:RW:X:N/A */
#define CY_U3P_PIB_DSCR_NUMBER_POS                          (0)


/*
   Number of descriptors still left to process.  This value is unrelated
   to actual number of descriptors in the list.  It is used only to generate
   an interrupt to the CPU when the value goes low or zero (or both).  When
   this value reaches 0 it will wrap around to 255.  The socket will not
   suspend or be otherwise affected unless the descriptor chains ends with
   0xFFFF descriptor number.
 */
#define CY_U3P_PIB_DSCR_COUNT_MASK                          (0x00ff0000) /* <16:23> RW:RW:X:N/A */
#define CY_U3P_PIB_DSCR_COUNT_POS                           (16)


/*
   The low watermark for dscr_count.  When dscr_count is equal or less than
   dscr_low the status bit dscr_is_low is set and an interrupt can be generated
   (depending on int mask).
 */
#define CY_U3P_PIB_DSCR_LOW_MASK                            (0xff000000) /* <24:31> R:RW:X:N/A */
#define CY_U3P_PIB_DSCR_LOW_POS                             (24)



/*
   Transfer Size Register
 */
#define CY_U3P_PIB_SCK_SIZE_ADDRESS(n)                      (0xe0018004 + ((n) * (0x0080)))
#define CY_U3P_PIB_SCK_SIZE(n)                              (*(uvint32_t *)(0xe0018004 + ((n) * 0x0080)))
#define CY_U3P_PIB_SCK_SIZE_DEFAULT                         (0x00000000)

/*
   The number of bytes or buffers (depends on unit bit in SCK_STATUS) that
   are part of this transfer.  A value of 0 signals an infinite/undetermined
   transaction size.
   Valid data bytes remaining in the last buffer beyond the transfer size
   will be read by socket and passed on to the core. FW must ensure that
   no additional bytes beyond the transfer size are present in the last buffer.
 */
#define CY_U3P_PIB_TRANS_SIZE_MASK                          (0xffffffff) /* <0:31> R:RW:X:N/A */
#define CY_U3P_PIB_TRANS_SIZE_POS                           (0)



/*
   Transfer Count Register
 */
#define CY_U3P_PIB_SCK_COUNT_ADDRESS(n)                     (0xe0018008 + ((n) * (0x0080)))
#define CY_U3P_PIB_SCK_COUNT(n)                             (*(uvint32_t *)(0xe0018008 + ((n) * 0x0080)))
#define CY_U3P_PIB_SCK_COUNT_DEFAULT                        (0x00000000)

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
#define CY_U3P_PIB_TRANS_COUNT_MASK                         (0xffffffff) /* <0:31> RW:RW:X:N/A */
#define CY_U3P_PIB_TRANS_COUNT_POS                          (0)



/*
   Socket Status Register
 */
#define CY_U3P_PIB_SCK_STATUS_ADDRESS(n)                    (0xe001800c + ((n) * (0x0080)))
#define CY_U3P_PIB_SCK_STATUS(n)                            (*(uvint32_t *)(0xe001800c + ((n) * 0x0080)))
#define CY_U3P_PIB_SCK_STATUS_DEFAULT                       (0x04e00000)

/*
   Number of available (free for ingress, occupied for egress) descriptors
   beyond the current one.  This number is incremented by the adapter whenever
   an event is received on this socket and decremented whenever it activates
   a new descriptor. This value is used to create a signal to the IP Cores
   that indicates at least one buffer is available beyond the current one
   (sck_more_buf_avl).
 */
#define CY_U3P_PIB_AVL_COUNT_MASK                           (0x0000001f) /* <0:4> RW:RW:0:N/A */
#define CY_U3P_PIB_AVL_COUNT_POS                            (0)


/*
   Minimum number of available buffers required by the adapter before activating
   a new one.  This can be used to guarantee a minimum number of buffers
   available with old data to implement rollback.  If AVL_ENABLE, the socket
   will remain in STALL state until AVL_COUNT>=AVL_MIN.
 */
#define CY_U3P_PIB_AVL_MIN_MASK                             (0x000003e0) /* <5:9> R:RW:0:N/A */
#define CY_U3P_PIB_AVL_MIN_POS                              (5)


/*
   Enables the functioning of AVL_COUNT and AVL_MIN. When 0, it will disable
   both stalling on AVL_MIN and generation of the sck_more_buf_avl signal
   described above.
 */
#define CY_U3P_PIB_AVL_ENABLE                               (1u << 10) /* <10:10> R:RW:0:N/A */


/*
   Internal operating state of the socket.  This field is used for debugging
   and to safely modify active sockets (see go_suspend).
 */
#define CY_U3P_PIB_STATE_MASK                               (0x00038000) /* <15:17> RW:R:0:N/A */
#define CY_U3P_PIB_STATE_POS                                (15)

/*
   Descriptor state. This is the default initial state indicating the descriptor
   registers are NOT valid in the Adapter. The Adapter will start loading
   the descriptor from memory if the socket becomes enabled and not suspended.
   Suspend has no effect on any other state.
 */
#define CY_U3P_PIB_STATE_DESCR                              (0)
/*
   Stall state. Socket is stalled waiting for data to be loaded into the
   Fetch Queue or waiting for an event.
 */
#define CY_U3P_PIB_STATE_STALL                              (1)
/*
   Active state. Socket is available for core data transfers.
 */
#define CY_U3P_PIB_STATE_ACTIVE                             (2)
/*
   Event state. Core transfer is done. Descriptor is being written back to
   memory and an event is being generated if enabled.
 */
#define CY_U3P_PIB_STATE_EVENT                              (3)
/*
   Check states. An active socket gets here based on the core’s EOP request
   to check the Transfer size and determine whether the buffer should be
   wrapped up. Depending on result, socket will either go back to Active
   state or move to the Event state.
 */
#define CY_U3P_PIB_STATE_CHECK1                             (4)
/*
   Socket is suspended
 */
#define CY_U3P_PIB_STATE_SUSPENDED                          (5)
/*
   Check states. An active socket gets here based on the core’s EOP request
   to check the Transfer size and determine whether the buffer should be
   wrapped up. Depending on result, socket will either go back to Active
   state or move to the Event state.
 */
#define CY_U3P_PIB_STATE_CHECK2                             (6)
/*
   Waiting for confirmation that event was sent.
 */
#define CY_U3P_PIB_STATE_WAITING                            (7)

/*
   Indicates the socket received a ZLP
 */
#define CY_U3P_PIB_ZLP_RCVD                                 (1u << 18) /* <18:18> RW:R:0:N/A */


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
#define CY_U3P_PIB_SUSPENDED                                (1u << 19) /* <19:19> RW:R:0:N/A */


/*
   Indicates the socket is currently enabled when asserted.  After go_enable
   is changed, it may take some time for enabled to make the same change.
    This value can be polled to determine this fact.
 */
#define CY_U3P_PIB_ENABLED                                  (1u << 20) /* <20:20> RW:R:0:N/A */


/*
   Enable (1) or disable (0) truncating of BYTE_COUNT when TRANS_COUNT+BYTE_COUNT>=TRANS_SIZE.
    When enabled, ensures that an ingress transfer never contains more bytes
   then allowed.  This function is needed to implement burst-based prototocols
   that can only transmit full bursts of more than 1 byte.
 */
#define CY_U3P_PIB_TRUNCATE                                 (1u << 21) /* <21:21> R:RW:1:N/A */


/*
   Enable (1) or disable (0) sending of produce events from any descriptor
   in this socket.  If 0, events will be suppressed, and the descriptor will
   not be copied back into memory when completed.
 */
#define CY_U3P_PIB_EN_PROD_EVENTS                           (1u << 22) /* <22:22> R:RW:1:N/A */


/*
   Enable (1) or disable (0) sending of consume events from any descriptor
   in this socket.  If 0, events will be suppressed, and the descriptor will
   not be copied back into memory when completed.
 */
#define CY_U3P_PIB_EN_CONS_EVENTS                           (1u << 23) /* <23:23> R:RW:1:N/A */


/*
   When set, the socket will suspend before activating a descriptor with
   BYTE_COUNT<BUFFER_SIZE.
   This is relevant for egress sockets only.
 */
#define CY_U3P_PIB_SUSP_PARTIAL                             (1u << 24) /* <24:24> R:RW:0:N/A */


/*
   When set, the socket will suspend before activating a descriptor with
   TRANS_COUNT+BUFFER_SIZE>=TRANS_SIZE.  This is relevant for both ingress
   and egress sockets.
 */
#define CY_U3P_PIB_SUSP_LAST                                (1u << 25) /* <25:25> R:RW:0:N/A */


/*
   When set, the socket will suspend when trans_count >= trans_size.  This
   equation is checked (and hence the socket will suspend) only at the boundary
   of buffers and packets (ie. buffer wrapup or EOP assertion).
 */
#define CY_U3P_PIB_SUSP_TRANS                               (1u << 26) /* <26:26> R:RW:1:N/A */


/*
   When set, the socket will suspend after wrapping up the first buffer with
   dscr.eop=1.  Note that this function will work the same for both ingress
   and egress sockets.
 */
#define CY_U3P_PIB_SUSP_EOP                                 (1u << 27) /* <27:27> R:RW:0:N/A */


/*
   Setting this bit will forcibly wrap-up a socket, whether it is out of
   data or not.  This option is intended mainly for ingress sockets, but
   works also for egress sockets.  Any remaining data in fetch buffers is
   ignored, in write buffers is flushed.  Transaction and buffer counts are
   updated normally, and suspend behavior also happens normally (depending
   on various other settings in this register).G45
 */
#define CY_U3P_PIB_WRAPUP                                   (1u << 28) /* <28:28> RW0C:RW1S:0:N/A */


/*
   Indicates whether descriptors (1) or bytes (0) are counted by trans_count
   and trans_size.  Descriptors are counting regardless of whether they contain
   any data or have eop set.
 */
#define CY_U3P_PIB_UNIT                                     (1u << 29) /* <29:29> R:RW:0:N/A */


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
#define CY_U3P_PIB_GO_SUSPEND                               (1u << 30) /* <30:30> R:RW:0:N/A */


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
#define CY_U3P_PIB_GO_ENABLE                                (1u << 31) /* <31:31> R:RW:0:N/A */



/*
   Socket Interrupt Request Register
 */
#define CY_U3P_PIB_SCK_INTR_ADDRESS(n)                      (0xe0018010 + ((n) * (0x0080)))
#define CY_U3P_PIB_SCK_INTR(n)                              (*(uvint32_t *)(0xe0018010 + ((n) * 0x0080)))
#define CY_U3P_PIB_SCK_INTR_DEFAULT                         (0x00000000)

/*
   Indicates that a produce event has been received or transmitted since
   last cleared.
 */
#define CY_U3P_PIB_PRODUCE_EVENT                            (1u << 0) /* <0:0> W1S:RW1C:0:N/A */


/*
   Indicates that a consume event has been received or transmitted since
   last cleared.
 */
#define CY_U3P_PIB_CONSUME_EVENT                            (1u << 1) /* <1:1> W1S:RW1C:0:N/A */


/*
   Indicates that dscr_count has fallen below its watermark dscr_low.  If
   dscr_count wraps around to 255 dscr_is_low will remain asserted until
   cleared by s/w
 */
#define CY_U3P_PIB_DSCR_IS_LOW                              (1u << 2) /* <2:2> W1S:RW1C:0:N/A */


/*
   Indicates the no descriptor is available.  Not available means that the
   current descriptor number is 0xFFFF.  Note that this bit will remain asserted
   until cleared by s/w, regardless of whether a new descriptor number is
   loaded.
 */
#define CY_U3P_PIB_DSCR_NOT_AVL                             (1u << 3) /* <3:3> W1S:RW1C:0:N/A */


/*
   Indicates the socket has stalled, waiting for an event signaling its descriptor
   has become available. Note that this bit will remain asserted until cleared
   by s/w, regardless of whether the socket resumes.
 */
#define CY_U3P_PIB_STALL                                    (1u << 4) /* <4:4> W1S:RW1C:0:N/A */


/*
   Indicates the socket has gone into suspend mode.  This may be caused by
   any hardware initiated condition (e.g. DSCR_NOT_AVL, any of the SUSP_*)
   or by setting GO_SUSPEND=1.  Note that this bit will remain asserted until
   cleared by s/w, regardless of whether the suspend condition is resolved.
   Note that the socket resumes only when SCK_INTR[9:5]=0 and GO_SUSPEND=0.
 */
#define CY_U3P_PIB_SUSPEND                                  (1u << 5) /* <5:5> W1S:RW1C:0:N/A */


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
#define CY_U3P_PIB_ERR                                      (1u << 6) /* <6:6> W1S:RW1C:0:N/A */


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
#define CY_U3P_PIB_TRANS_DONE                               (1u << 7) /* <7:7> W1S:RW1C:0:N/A */


/*
   Indicates that the (egress) socket was suspended because of SUSP_PARTIAL
   condition.  Note that the socket resumes only when SCK_INTR[9:5]=0 and
   GO_SUSPEND=0.
 */
#define CY_U3P_PIB_PARTIAL_BUF                              (1u << 8) /* <8:8> W1S:RW1C:0:N/A */


/*
   Indicates that the socket was suspended because of SUSP_LAST condition.
    Note that the socket resumes only when SCK_INTR[9:5]=0 and GO_SUSPEND=0.
 */
#define CY_U3P_PIB_LAST_BUF                                 (1u << 9) /* <9:9> W1S:RW1C:0:N/A */



/*
   Socket Interrupt Mask Register
 */
#define CY_U3P_PIB_SCK_INTR_MASK_ADDRESS(n)                 (0xe0018014 + ((n) * (0x0080)))
#define CY_U3P_PIB_SCK_INTR_MASK(n)                         (*(uvint32_t *)(0xe0018014 + ((n) * 0x0080)))
#define CY_U3P_PIB_SCK_INTR_MASK_DEFAULT                    (0x00000000)

/*
   1: Report interrupt to CPU
 */
#define CY_U3P_PIB_PRODUCE_EVENT                            (1u << 0) /* <0:0> R:RW:0:N/A */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_PIB_CONSUME_EVENT                            (1u << 1) /* <1:1> R:RW:0:N/A */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_PIB_DSCR_IS_LOW                              (1u << 2) /* <2:2> R:RW:0:N/A */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_PIB_DSCR_NOT_AVL                             (1u << 3) /* <3:3> R:RW:0:N/A */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_PIB_STALL                                    (1u << 4) /* <4:4> R:RW:0:N/A */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_PIB_SUSPEND                                  (1u << 5) /* <5:5> R:RW:0:N/A */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_PIB_ERR                                      (1u << 6) /* <6:6> R:RW:0:N/A */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_PIB_TRANS_DONE                               (1u << 7) /* <7:7> R:RW:0:N/A */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_PIB_PARTIAL_BUF                              (1u << 8) /* <8:8> R:RW:0:N/A */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_PIB_LAST_BUF                                 (1u << 9) /* <9:9> R:RW:0:N/A */



/*
   Descriptor buffer base address register
 */
#define CY_U3P_PIB_DSCR_BUFFER_ADDRESS(n)                   (0xe0018020 + ((n) * (0x0080)))
#define CY_U3P_PIB_DSCR_BUFFER(n)                           (*(uvint32_t *)(0xe0018020 + ((n) * 0x0080)))
#define CY_U3P_PIB_DSCR_BUFFER_DEFAULT                      (0x00000000)

/*
   The base address of the buffer where data is written.  This address is
   not necessarily word-aligned to allow for header/trailer/length modification.
 */
#define CY_U3P_PIB_BUFFER_ADDR_MASK                         (0xffffffff) /* <0:31> RW:RW:X:N/A */
#define CY_U3P_PIB_BUFFER_ADDR_POS                          (0)



/*
   Descriptor synchronization pointers register
 */
#define CY_U3P_PIB_DSCR_SYNC_ADDRESS(n)                     (0xe0018024 + ((n) * (0x0080)))
#define CY_U3P_PIB_DSCR_SYNC(n)                             (*(uvint32_t *)(0xe0018024 + ((n) * 0x0080)))
#define CY_U3P_PIB_DSCR_SYNC_DEFAULT                        (0x00000000)

/*
   The socket number of the consuming socket to which the produce event shall
   be sent.
   If cons_ip and cons_sck matches the socket's IP and socket number then
   the matching socket becomes consuming socket.
 */
#define CY_U3P_PIB_CONS_SCK_MASK                            (0x000000ff) /* <0:7> RW:RW:X:N/A */
#define CY_U3P_PIB_CONS_SCK_POS                             (0)


/*
   The IP number of the consuming socket to which the produce event shall
   be sent.  Use 0x3F to designate ARM CPU (so software) as consumer; the
   event will be lost in this case and an interrupt should also be generated
   to observe this condition.
 */
#define CY_U3P_PIB_CONS_IP_MASK                             (0x00003f00) /* <8:13> RW:RW:X:N/A */
#define CY_U3P_PIB_CONS_IP_POS                              (8)


/*
   Enable sending of a consume events from this descriptor only.  Events
   are sent only if SCK_STATUS.en_consume_ev=1.  When events are disabled,
   the adapter also does not update the descriptor in memory to clear its
   occupied bit.
 */
#define CY_U3P_PIB_EN_CONS_EVENT                            (1u << 14) /* <14:14> RW:RW:X:N/A */


/*
   Enable generation of a consume event interrupt for this descriptor only.
    This interrupt will only be seen by the CPU if SCK_STATUS.int_mask has
   this interrupt enabled as well.  Note that this flag influences the logging
   of the interrupt in SCK_STATUS; it has no effect on the reporting of the
   interrupt to the CPU like SCK_STATUS.int_mask does.
 */
#define CY_U3P_PIB_EN_CONS_INT                              (1u << 15) /* <15:15> RW:RW:X:N/A */


/*
   The socket number of the producing socket to which the consume event shall
   be sent. If prod_ip and prod_sck matches the socket's IP and socket number
   then the matching socket becomes consuming socket.
 */
#define CY_U3P_PIB_PROD_SCK_MASK                            (0x00ff0000) /* <16:23> RW:RW:X:N/A */
#define CY_U3P_PIB_PROD_SCK_POS                             (16)


/*
   The IP number of the producing socket to which the consume event shall
   be sent. Use 0x3F to designate ARM CPU (so software) as producer; the
   event will be lost in this case and an interrupt should also be generated
   to observe this condition.
 */
#define CY_U3P_PIB_PROD_IP_MASK                             (0x3f000000) /* <24:29> RW:RW:X:N/A */
#define CY_U3P_PIB_PROD_IP_POS                              (24)


/*
   Enable sending of a produce events from this descriptor only.  Events
   are sent only if SCK_STATUS.en_produce_ev=1.  If 0, events will be suppressed,
   and the descriptor will not be copied back into memory when completed.
 */
#define CY_U3P_PIB_EN_PROD_EVENT                            (1u << 30) /* <30:30> RW:RW:X:N/A */


/*
   Enable generation of a produce event interrupt for this descriptor only.
   This interrupt will only be seen by the CPU if SCK_STATUS. int_mask has
   this interrupt enabled as well.  Note that this flag influences the logging
   of the interrupt in SCK_STATUS; it has no effect on the reporting of the
   interrupt to the CPU like SCK_STATUS.int_mask does.
 */
#define CY_U3P_PIB_EN_PROD_INT                              (1u << 31) /* <31:31> RW:RW:X:N/A */



/*
   Descriptor Chain Pointers Register
 */
#define CY_U3P_PIB_DSCR_CHAIN_ADDRESS(n)                    (0xe0018028 + ((n) * (0x0080)))
#define CY_U3P_PIB_DSCR_CHAIN(n)                            (*(uvint32_t *)(0xe0018028 + ((n) * 0x0080)))
#define CY_U3P_PIB_DSCR_CHAIN_DEFAULT                       (0x00000000)

/*
   Descriptor number of the next task for consumer. A value of 0xFFFF signals
   end of this list.
 */
#define CY_U3P_PIB_RD_NEXT_DSCR_MASK                        (0x0000ffff) /* <0:15> RW:RW:X:N/A */
#define CY_U3P_PIB_RD_NEXT_DSCR_POS                         (0)


/*
   Descriptor number of the next task for producer. A value of 0xFFFF signals
   end of this list.
 */
#define CY_U3P_PIB_WR_NEXT_DSCR_MASK                        (0xffff0000) /* <16:31> RW:RW:X:N/A */
#define CY_U3P_PIB_WR_NEXT_DSCR_POS                         (16)



/*
   Descriptor Size Register
 */
#define CY_U3P_PIB_DSCR_SIZE_ADDRESS(n)                     (0xe001802c + ((n) * (0x0080)))
#define CY_U3P_PIB_DSCR_SIZE(n)                             (*(uvint32_t *)(0xe001802c + ((n) * 0x0080)))
#define CY_U3P_PIB_DSCR_SIZE_DEFAULT                        (0x00000000)

/*
   A marker that is provided by s/w and can be observed by the IP.  It's
   meaning is defined by the IP that uses it.  This bit has no effect on
   the other DMA mechanisms.
 */
#define CY_U3P_PIB_MARKER                                   (1u << 0) /* <0:0> RW:RW:X:N/A */


/*
   A marker indicating this descriptor refers to the last buffer of a packet
   or transfer. Packets/transfers may span more than one buffer.  The producing
   IP provides this marker by providing the EOP signal to its DMA adapter.
    The consuming IP observes this marker by inspecting its EOP return signal
   from its own DMA adapter. For more information see section on packets,
   buffers and transfers in DMA chapter.
 */
#define CY_U3P_PIB_EOP                                      (1u << 1) /* <1:1> RW:RW:X:N/A */


/*
   Indicates the buffer data is valid (0) or in error (1).
 */
#define CY_U3P_PIB_BUFFER_ERROR                             (1u << 2) /* <2:2> RW:RW:X:N/A */


/*
   Indicates the buffer is in use (1) or empty (0).  A consumer will interpret
   this as:
   0: Buffer is empty, wait until filled.
   1: Buffer has data that can be consumed
   A produce will interpret this as:
   0: Buffer is ready to be filled
   1: Buffer is occupied, wait until empty
 */
#define CY_U3P_PIB_BUFFER_OCCUPIED                          (1u << 3) /* <3:3> RW:RW:X:N/A */


/*
   The size of the buffer in multiples of 16 bytes
 */
#define CY_U3P_PIB_BUFFER_SIZE_MASK                         (0x0000fff0) /* <4:15> RW:RW:X:N/A */
#define CY_U3P_PIB_BUFFER_SIZE_POS                          (4)


/*
   The number of data bytes present in the buffer.  An occupied buffer is
   not always full, in particular when variable length packets are transferred.
 */
#define CY_U3P_PIB_BYTE_COUNT_MASK                          (0xffff0000) /* <16:31> RW:RW:X:N/A */
#define CY_U3P_PIB_BYTE_COUNT_POS                           (16)



/*
   Event Communication Register
 */
#define CY_U3P_PIB_EVENT_ADDRESS(n)                         (0xe001807c + ((n) * (0x0080)))
#define CY_U3P_PIB_EVENT(n)                                 (*(uvint32_t *)(0xe001807c + ((n) * 0x0080)))
#define CY_U3P_PIB_EVENT_DEFAULT                            (0x00000000)

/*
   The active descriptor number for which the event is sent.
 */
#define CY_U3P_EVENT_ACTIVE_DSCR_MASK                       (0x0000ffff) /* <0:15> RW:W:0:N/A */
#define CY_U3P_EVENT_ACTIVE_DSCR_POS                        (0)


/*
   Type of event
   0: Consume event descriptor is marked empty - BUFFER_OCCUPIED=0)
   1: Produce event descriptor is marked full = BUFFER_OCCUPIED=1)
 */
#define CY_U3P_EVENT_EVENT_TYPE                             (1u << 16) /* <16:16> RW:W:0:N/A */



/*
   Socket Interrupt Request Register
 */
#define CY_U3P_PIB_SCK_INTR0_ADDRESS                        (0xe001ff00)
#define CY_U3P_PIB_SCK_INTR0                                (*(uvint32_t *)(0xe001ff00))
#define CY_U3P_PIB_SCK_INTR0_DEFAULT                        (0x00000000)

/*
   Socket <x> asserts interrupt when bit <x> is set in this vector.  Multiple
   bits may be set to 1 simultaneously.
   This register is only as wide as the number of socket in the adapter;
   256 is just the maximum width.  All other bits always return 0.
 */
#define CY_U3P_PIB_SCKINTR_L_MASK                           (0xffffffff) /* <0:31> W:R:0:N/A */
#define CY_U3P_PIB_SCKINTR_L_POS                            (0)



/*
   Socket Interrupt Request Register
 */
#define CY_U3P_PIB_SCK_INTR1_ADDRESS                        (0xe001ff04)
#define CY_U3P_PIB_SCK_INTR1                                (*(uvint32_t *)(0xe001ff04))
#define CY_U3P_PIB_SCK_INTR1_DEFAULT                        (0x00000000)

/*
   Socket <x> asserts interrupt when bit <x> is set in this vector.  Multiple
   bits may be set to 1 simultaneously.
   This register is only as wide as the number of socket in the adapter;
   256 is just the maximum width.  All other bits always return 0.
 */
#define CY_U3P_PIB_SCKINTR_H_L_MASK                         (0xffffffff) /* <0:31> W:R:0:N/A */
#define CY_U3P_PIB_SCKINTR_H_L_POS                          (0)



/*
   Socket Interrupt Request Register
 */
#define CY_U3P_PIB_SCK_INTR2_ADDRESS                        (0xe001ff08)
#define CY_U3P_PIB_SCK_INTR2                                (*(uvint32_t *)(0xe001ff08))
#define CY_U3P_PIB_SCK_INTR2_DEFAULT                        (0x00000000)

/*
   Socket <x> asserts interrupt when bit <x> is set in this vector.  Multiple
   bits may be set to 1 simultaneously.
   This register is only as wide as the number of socket in the adapter;
   256 is just the maximum width.  All other bits always return 0.
 */
#define CY_U3P_PIB_SCKINTR_H_L_MASK                         (0xffffffff) /* <0:31> W:R:0:N/A */
#define CY_U3P_PIB_SCKINTR_H_L_POS                          (0)



/*
   Socket Interrupt Request Register
 */
#define CY_U3P_PIB_SCK_INTR3_ADDRESS                        (0xe001ff0c)
#define CY_U3P_PIB_SCK_INTR3                                (*(uvint32_t *)(0xe001ff0c))
#define CY_U3P_PIB_SCK_INTR3_DEFAULT                        (0x00000000)

/*
   Socket <x> asserts interrupt when bit <x> is set in this vector.  Multiple
   bits may be set to 1 simultaneously.
   This register is only as wide as the number of socket in the adapter;
   256 is just the maximum width.  All other bits always return 0.
 */
#define CY_U3P_PIB_SCKINTR_H_L_MASK                         (0xffffffff) /* <0:31> W:R:0:N/A */
#define CY_U3P_PIB_SCKINTR_H_L_POS                          (0)



/*
   Socket Interrupt Request Register
 */
#define CY_U3P_PIB_SCK_INTR4_ADDRESS                        (0xe001ff10)
#define CY_U3P_PIB_SCK_INTR4                                (*(uvint32_t *)(0xe001ff10))
#define CY_U3P_PIB_SCK_INTR4_DEFAULT                        (0x00000000)

/*
   Socket <x> asserts interrupt when bit <x> is set in this vector.  Multiple
   bits may be set to 1 simultaneously.
   This register is only as wide as the number of socket in the adapter;
   256 is just the maximum width.  All other bits always return 0.
 */
#define CY_U3P_PIB_SCKINTR_H_L_MASK                         (0xffffffff) /* <0:31> W:R:0:N/A */
#define CY_U3P_PIB_SCKINTR_H_L_POS                          (0)



/*
   Socket Interrupt Request Register
 */
#define CY_U3P_PIB_SCK_INTR5_ADDRESS                        (0xe001ff14)
#define CY_U3P_PIB_SCK_INTR5                                (*(uvint32_t *)(0xe001ff14))
#define CY_U3P_PIB_SCK_INTR5_DEFAULT                        (0x00000000)

/*
   Socket <x> asserts interrupt when bit <x> is set in this vector.  Multiple
   bits may be set to 1 simultaneously.
   This register is only as wide as the number of socket in the adapter;
   256 is just the maximum width.  All other bits always return 0.
 */
#define CY_U3P_PIB_SCKINTR_H_L_MASK                         (0xffffffff) /* <0:31> W:R:0:N/A */
#define CY_U3P_PIB_SCKINTR_H_L_POS                          (0)



/*
   Socket Interrupt Request Register
 */
#define CY_U3P_PIB_SCK_INTR6_ADDRESS                        (0xe001ff18)
#define CY_U3P_PIB_SCK_INTR6                                (*(uvint32_t *)(0xe001ff18))
#define CY_U3P_PIB_SCK_INTR6_DEFAULT                        (0x00000000)

/*
   Socket <x> asserts interrupt when bit <x> is set in this vector.  Multiple
   bits may be set to 1 simultaneously.
   This register is only as wide as the number of socket in the adapter;
   256 is just the maximum width.  All other bits always return 0.
 */
#define CY_U3P_PIB_SCKINTR_H_L_MASK                         (0xffffffff) /* <0:31> W:R:0:N/A */
#define CY_U3P_PIB_SCKINTR_H_L_POS                          (0)



/*
   Socket Interrupt Request Register
 */
#define CY_U3P_PIB_SCK_INTR7_ADDRESS                        (0xe001ff1c)
#define CY_U3P_PIB_SCK_INTR7                                (*(uvint32_t *)(0xe001ff1c))
#define CY_U3P_PIB_SCK_INTR7_DEFAULT                        (0x00000000)

/*
   Socket <x> asserts interrupt when bit <x> is set in this vector.  Multiple
   bits may be set to 1 simultaneously.
   This register is only as wide as the number of socket in the adapter;
   256 is just the maximum width.  All other bits always return 0.
 */
#define CY_U3P_PIB_SCKINTR_H_MASK                           (0xffffffff) /* <0:31> W:R:0:N/A */
#define CY_U3P_PIB_SCKINTR_H_POS                            (0)



#endif /* _INCLUDED_PIB_REGS_H_ */

/*[]*/

