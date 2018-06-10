EESchema Schematic File Version 2
LIBS:Domesday Duplicator-rescue
LIBS:power
LIBS:device
LIBS:transistors
LIBS:conn
LIBS:linear
LIBS:regul
LIBS:74xx
LIBS:cmos4000
LIBS:adc-dac
LIBS:memory
LIBS:xilinx
LIBS:microcontrollers
LIBS:dsp
LIBS:microchip
LIBS:analog_switches
LIBS:motorola
LIBS:texas
LIBS:intel
LIBS:audio
LIBS:interface
LIBS:digital-audio
LIBS:philips
LIBS:display
LIBS:cypress
LIBS:siliconi
LIBS:opto
LIBS:atmel
LIBS:contrib
LIBS:valves
LIBS:ads825
LIBS:de0-nano_gpio
LIBS:opa690
LIBS:gpifii_j7
LIBS:gpifii_j6
LIBS:bnc_rosenberger
LIBS:74xgxx
LIBS:ac-dc
LIBS:actel
LIBS:allegro
LIBS:Altera
LIBS:analog_devices
LIBS:battery_management
LIBS:bbd
LIBS:bosch
LIBS:brooktre
LIBS:cmos_ieee
LIBS:dc-dc
LIBS:diode
LIBS:elec-unifil
LIBS:ESD_Protection
LIBS:ftdi
LIBS:gennum
LIBS:graphic
LIBS:graphic_symbols
LIBS:hc11
LIBS:infineon
LIBS:intersil
LIBS:ir
LIBS:Lattice
LIBS:leds
LIBS:LEM
LIBS:logic_programmable
LIBS:logo
LIBS:maxim
LIBS:mechanical
LIBS:microchip_dspic33dsc
LIBS:microchip_pic10mcu
LIBS:microchip_pic12mcu
LIBS:microchip_pic16mcu
LIBS:microchip_pic18mcu
LIBS:microchip_pic24mcu
LIBS:microchip_pic32mcu
LIBS:modules
LIBS:motor_drivers
LIBS:motors
LIBS:msp430
LIBS:nordicsemi
LIBS:nxp
LIBS:nxp_armmcu
LIBS:onsemi
LIBS:Oscillators
LIBS:Power_Management
LIBS:powerint
LIBS:pspice
LIBS:references
LIBS:relays
LIBS:rfcom
LIBS:RFSolutions
LIBS:sensors
LIBS:silabs
LIBS:stm8
LIBS:stm32
LIBS:supertex
LIBS:switches
LIBS:transf
LIBS:triac_thyristor
LIBS:ttl_ieee
LIBS:video
LIBS:wiznet
LIBS:Worldsemi
LIBS:Xicor
LIBS:zetex
LIBS:Zilog
LIBS:Domesday Duplicator-cache
EELAYER 25 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 5
Title "Domesday Duplicator"
Date "2018-06-10"
Rev "2.3"
Comp "https://www.domesday86.com"
Comment1 "(c)2018 Simon Inns"
Comment2 "License: Attribution-ShareAlike 4.0 International (CC BY-SA 4.0)"
Comment3 ""
Comment4 ""
$EndDescr
$Sheet
S 2850 2900 900  3350
U 59D0E979
F0 "Cypress Superspeed Interface" 60
F1 "superspeedinf.sch" 60
F2 "DATA0" B R 3750 3000 60 
F3 "DATA1" B R 3750 3100 60 
F4 "DATA2" B R 3750 3200 60 
F5 "DATA3" B R 3750 3300 60 
F6 "DATA4" B R 3750 3400 60 
F7 "DATA5" B R 3750 3500 60 
F8 "DATA6" B R 3750 3600 60 
F9 "DATA7" B R 3750 3700 60 
F10 "DATA8" B R 3750 3800 60 
F11 "DATA9" B R 3750 3900 60 
F12 "DATA10" B R 3750 4000 60 
F13 "DATA11" B R 3750 4100 60 
F14 "DATA12" B R 3750 4200 60 
F15 "DATA13" B R 3750 4300 60 
F16 "DATA14" B R 3750 4400 60 
F17 "DATA15" B R 3750 4500 60 
F18 "CTL0" B L 2850 3000 60 
F19 "CTL1" B L 2850 3100 60 
F20 "CTL2" B L 2850 3200 60 
F21 "CTL3" B L 2850 3300 60 
F22 "CTL4" B L 2850 3400 60 
F23 "CTL5" B L 2850 3500 60 
F24 "CTL6" B L 2850 3600 60 
F25 "CTL7" B L 2850 3700 60 
F26 "CTL8" B L 2850 3800 60 
F27 "CTL9" B L 2850 3900 60 
F28 "CTL10" B L 2850 4000 60 
F29 "CTL11" B L 2850 4100 60 
F30 "CTL12" B L 2850 4200 60 
F31 "PCLK" B L 2850 4400 60 
F32 "DATA16" B R 3750 4650 60 
F33 "DATA17" B R 3750 4750 60 
F34 "DATA18" B R 3750 4850 60 
F35 "DATA19" B R 3750 4950 60 
F36 "DATA20" B R 3750 5050 60 
F37 "DATA21" B R 3750 5150 60 
F38 "DATA22" B R 3750 5250 60 
F39 "DATA23" B R 3750 5350 60 
F40 "DATA24" B R 3750 5450 60 
F41 "DATA25" B R 3750 5550 60 
F42 "DATA26" B R 3750 5650 60 
F43 "DATA27" B R 3750 5750 60 
F44 "DATA28" B R 3750 5850 60 
F45 "DATA29" B R 3750 5950 60 
F46 "DATA30" B R 3750 6050 60 
F47 "DATA31" B R 3750 6150 60 
$EndSheet
$Comp
L DE0-Nano_GPIO J102
U 1 1 59D13A5F
P 8800 4950
F 0 "J102" H 8400 3850 60  0000 C CNN
F 1 "DE0-Nano_GPIO0" H 8700 6050 60  0000 C CNN
F 2 "Socket_Strips:Socket_Strip_Straight_2x20_Pitch2.54mm" H 8800 4950 60  0001 C CNN
F 3 "" H 8800 4950 60  0001 C CNN
	1    8800 4950
	1    0    0    -1  
$EndComp
Text GLabel 7750 3000 0    60   Input ~ 0
USB_DATA0
Text GLabel 7750 2900 0    60   Input ~ 0
USB_DATA1
Text GLabel 7750 2800 0    60   Input ~ 0
USB_DATA2
Text GLabel 7750 2700 0    60   Input ~ 0
USB_DATA3
Text GLabel 7750 2600 0    60   Input ~ 0
USB_DATA4
Text GLabel 7750 2400 0    60   Input ~ 0
USB_DATA5
Text GLabel 7750 2300 0    60   Input ~ 0
USB_DATA6
Text GLabel 7750 2200 0    60   Input ~ 0
USB_DATA7
Text GLabel 7750 2100 0    60   Input ~ 0
USB_DATA8
Text GLabel 7750 2000 0    60   Input ~ 0
USB_DATA9
Text GLabel 7750 1900 0    60   Input ~ 0
USB_DATA10
Text GLabel 7750 1800 0    60   Input ~ 0
USB_DATA11
Text GLabel 7750 1700 0    60   Input ~ 0
USB_DATA12
Text GLabel 7750 1500 0    60   Input ~ 0
USB_DATA13
Text GLabel 7750 1400 0    60   Input ~ 0
USB_DATA14
Text GLabel 7750 1300 0    60   Input ~ 0
USB_DATA15
Text GLabel 9800 2700 2    60   Input ~ 0
USB_CTL0
Text GLabel 9800 2600 2    60   Input ~ 0
USB_CTL1
Text GLabel 9800 2400 2    60   Input ~ 0
USB_CTL2
Text GLabel 9800 2300 2    60   Input ~ 0
USB_CTL3
Text GLabel 9800 2200 2    60   Input ~ 0
USB_CTL4
Text GLabel 9800 2100 2    60   Input ~ 0
USB_CTL5
Text GLabel 9800 2000 2    60   Input ~ 0
USB_CTL6
Text GLabel 9800 1900 2    60   Input ~ 0
USB_CTL7
Text GLabel 9800 1800 2    60   Input ~ 0
USB_CTL8
Text GLabel 9800 1700 2    60   Input ~ 0
USB_CTL9
Text GLabel 9800 1500 2    60   Input ~ 0
USB_CTL10
Text GLabel 9800 1400 2    60   Input ~ 0
USB_CTL11
Text GLabel 9800 1300 2    60   Input ~ 0
USB_CTL12
Text GLabel 9800 2900 2    60   Input ~ 0
USB_PCLK
Wire Wire Line
	9600 1600 9600 3200
Connection ~ 9600 2500
$Comp
L GND #PWR01
U 1 1 59D160F9
P 9600 3200
F 0 "#PWR01" H 9600 2950 50  0001 C CNN
F 1 "GND" H 9600 3050 50  0000 C CNN
F 2 "" H 9600 3200 50  0001 C CNN
F 3 "" H 9600 3200 50  0001 C CNN
	1    9600 3200
	1    0    0    -1  
$EndComp
Text GLabel 2750 3000 0    60   Input ~ 0
USB_CTL0
Text GLabel 2750 3100 0    60   Input ~ 0
USB_CTL1
Text GLabel 2750 3200 0    60   Input ~ 0
USB_CTL2
Text GLabel 2750 3300 0    60   Input ~ 0
USB_CTL3
Text GLabel 2750 3400 0    60   Input ~ 0
USB_CTL4
Text GLabel 2750 3500 0    60   Input ~ 0
USB_CTL5
Text GLabel 2750 3600 0    60   Input ~ 0
USB_CTL6
Text GLabel 2750 3700 0    60   Input ~ 0
USB_CTL7
Text GLabel 2750 3800 0    60   Input ~ 0
USB_CTL8
Text GLabel 2750 3900 0    60   Input ~ 0
USB_CTL9
Text GLabel 2750 4000 0    60   Input ~ 0
USB_CTL10
Text GLabel 2750 4100 0    60   Input ~ 0
USB_CTL11
Text GLabel 2750 4200 0    60   Input ~ 0
USB_CTL12
Text GLabel 2750 4400 0    60   Input ~ 0
USB_PCLK
Text GLabel 3850 3000 2    60   Input ~ 0
USB_DATA0
Text GLabel 3850 3100 2    60   Input ~ 0
USB_DATA1
Text GLabel 3850 3200 2    60   Input ~ 0
USB_DATA2
Text GLabel 3850 3300 2    60   Input ~ 0
USB_DATA3
Text GLabel 3850 3400 2    60   Input ~ 0
USB_DATA4
Text GLabel 3850 3500 2    60   Input ~ 0
USB_DATA5
Text GLabel 3850 3600 2    60   Input ~ 0
USB_DATA6
Text GLabel 3850 3700 2    60   Input ~ 0
USB_DATA7
Text GLabel 3850 3800 2    60   Input ~ 0
USB_DATA8
Text GLabel 3850 3900 2    60   Input ~ 0
USB_DATA9
Text GLabel 3850 4000 2    60   Input ~ 0
USB_DATA10
Text GLabel 3850 4100 2    60   Input ~ 0
USB_DATA11
Text GLabel 3850 4200 2    60   Input ~ 0
USB_DATA12
Text GLabel 3850 4300 2    60   Input ~ 0
USB_DATA13
Text GLabel 3850 4400 2    60   Input ~ 0
USB_DATA14
Text GLabel 3850 4500 2    60   Input ~ 0
USB_DATA15
Wire Wire Line
	2750 3000 2850 3000
Wire Wire Line
	2750 3100 2850 3100
Wire Wire Line
	2750 3200 2850 3200
Wire Wire Line
	2750 3300 2850 3300
Wire Wire Line
	2750 3400 2850 3400
Wire Wire Line
	2750 3500 2850 3500
Wire Wire Line
	2750 3600 2850 3600
Wire Wire Line
	2750 3700 2850 3700
Wire Wire Line
	2750 3800 2850 3800
Wire Wire Line
	2750 3900 2850 3900
Wire Wire Line
	2750 4000 2850 4000
Wire Wire Line
	2750 4100 2850 4100
Wire Wire Line
	2750 4200 2850 4200
Wire Wire Line
	2750 4400 2850 4400
Wire Wire Line
	3850 4500 3750 4500
Wire Wire Line
	3850 4400 3750 4400
Wire Wire Line
	3850 4300 3750 4300
Wire Wire Line
	3850 4200 3750 4200
Wire Wire Line
	3850 4100 3750 4100
Wire Wire Line
	3850 4000 3750 4000
Wire Wire Line
	3850 3900 3750 3900
Wire Wire Line
	3850 3800 3750 3800
Wire Wire Line
	3850 3700 3750 3700
Wire Wire Line
	3850 3600 3750 3600
Wire Wire Line
	3850 3500 3750 3500
Wire Wire Line
	3850 3400 3750 3400
Wire Wire Line
	3850 3300 3750 3300
Wire Wire Line
	3850 3200 3750 3200
Wire Wire Line
	3850 3100 3750 3100
Wire Wire Line
	3850 3000 3750 3000
$Sheet
S 2950 950  750  1300
U 59D1E34E
F0 "ADC" 60
F1 "ADC.sch" 60
F2 "DATA0" O R 3700 1050 60 
F3 "DATA1" O R 3700 1150 60 
F4 "DATA2" O R 3700 1250 60 
F5 "DATA3" O R 3700 1350 60 
F6 "DATA4" O R 3700 1450 60 
F7 "DATA5" O R 3700 1550 60 
F8 "DATA6" O R 3700 1650 60 
F9 "DATA7" O R 3700 1750 60 
F10 "DATA8" O R 3700 1850 60 
F11 "DATA9" O R 3700 1950 60 
F12 "CLK" I R 3700 2150 60 
F13 "IN" I L 2950 1050 60 
F14 "REFT" O L 2950 1250 60 
F15 "REFB" O L 2950 1350 60 
$EndSheet
$Sheet
S 1650 950  800  300 
U 59D276FC
F0 "RF Amplifier" 60
F1 "RF Amplifier.sch" 60
F2 "REFT" I L 1650 1050 60 
F3 "REFB" I L 1650 1150 60 
F4 "RF_OUT" O R 2450 1050 60 
$EndSheet
Wire Wire Line
	2450 1050 2950 1050
Wire Wire Line
	2950 1250 2700 1250
Wire Wire Line
	2700 1250 2700 1450
Wire Wire Line
	2700 1450 1550 1450
Wire Wire Line
	1550 1450 1550 1050
Wire Wire Line
	1550 1050 1650 1050
Wire Wire Line
	2950 1350 2750 1350
Wire Wire Line
	2750 1350 2750 1500
Wire Wire Line
	2750 1500 1500 1500
Wire Wire Line
	1500 1500 1500 1150
Wire Wire Line
	1500 1150 1650 1150
$Comp
L DE0-Nano_GPIO J101
U 1 1 59D2B222
P 8800 2050
F 0 "J101" H 8400 950 60  0000 C CNN
F 1 "DE0-Nano_GPIO1" H 8700 3150 60  0000 C CNN
F 2 "Socket_Strips:Socket_Strip_Straight_2x20_Pitch2.54mm" H 8800 2050 60  0001 C CNN
F 3 "" H 8800 2050 60  0001 C CNN
	1    8800 2050
	1    0    0    -1  
$EndComp
Text GLabel 7650 5900 0    60   Input ~ 0
ADC_DATA0
Text GLabel 9700 5800 2    60   Input ~ 0
ADC_DATA1
Text GLabel 7650 5800 0    60   Input ~ 0
ADC_DATA2
Text GLabel 9700 5700 2    60   Input ~ 0
ADC_DATA3
Text GLabel 7650 5700 0    60   Input ~ 0
ADC_DATA4
Text GLabel 9700 5600 2    60   Input ~ 0
ADC_DATA5
Text GLabel 7650 5600 0    60   Input ~ 0
ADC_DATA6
Text GLabel 9700 5500 2    60   Input ~ 0
ADC_DATA7
Text GLabel 7650 5500 0    60   Input ~ 0
ADC_DATA8
Text GLabel 9700 5300 2    60   Input ~ 0
ADC_DATA9
Wire Wire Line
	9550 4500 9550 6100
$Comp
L +5V #PWR02
U 1 1 59D2E5E1
P 7800 3750
F 0 "#PWR02" H 7800 3600 50  0001 C CNN
F 1 "+5V" H 7800 3890 50  0000 C CNN
F 2 "" H 7800 3750 50  0001 C CNN
F 3 "" H 7800 3750 50  0001 C CNN
	1    7800 3750
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR03
U 1 1 59D2E990
P 9550 6100
F 0 "#PWR03" H 9550 5850 50  0001 C CNN
F 1 "GND" H 9550 5950 50  0000 C CNN
F 2 "" H 9550 6100 50  0001 C CNN
F 3 "" H 9550 6100 50  0001 C CNN
	1    9550 6100
	1    0    0    -1  
$EndComp
Wire Wire Line
	7800 4500 7800 3750
Text GLabel 9700 5900 2    60   Input ~ 0
ADC_CLK
Connection ~ 9550 5400
Text GLabel 3850 1050 2    60   Input ~ 0
ADC_DATA0
Text GLabel 3850 1150 2    60   Input ~ 0
ADC_DATA1
Text GLabel 3850 1350 2    60   Input ~ 0
ADC_DATA3
Text GLabel 3850 1550 2    60   Input ~ 0
ADC_DATA5
Text GLabel 3850 1750 2    60   Input ~ 0
ADC_DATA7
Text GLabel 3850 1950 2    60   Input ~ 0
ADC_DATA9
Text GLabel 3850 1250 2    60   Input ~ 0
ADC_DATA2
Text GLabel 3850 1450 2    60   Input ~ 0
ADC_DATA4
Text GLabel 3850 1650 2    60   Input ~ 0
ADC_DATA6
Text GLabel 3850 1850 2    60   Input ~ 0
ADC_DATA8
Text GLabel 3850 2150 2    60   Input ~ 0
ADC_CLK
Wire Wire Line
	3700 1050 3850 1050
Wire Wire Line
	3700 1150 3850 1150
Wire Wire Line
	3700 1250 3850 1250
Wire Wire Line
	3700 1350 3850 1350
Wire Wire Line
	3700 1450 3850 1450
Wire Wire Line
	3700 1550 3850 1550
Wire Wire Line
	3700 1650 3850 1650
Wire Wire Line
	3700 1750 3850 1750
Wire Wire Line
	3700 1850 3850 1850
Wire Wire Line
	3700 1950 3850 1950
Wire Wire Line
	3700 2150 3850 2150
$Comp
L +5V #PWR04
U 1 1 59D16023
P 7950 900
F 0 "#PWR04" H 7950 750 50  0001 C CNN
F 1 "+5V" H 7950 1040 50  0000 C CNN
F 2 "" H 7950 900 50  0001 C CNN
F 3 "" H 7950 900 50  0001 C CNN
	1    7950 900 
	1    0    0    -1  
$EndComp
Wire Wire Line
	7950 1600 7950 900 
Text GLabel 3850 4650 2    60   Input ~ 0
USB_DATA16
Text GLabel 3850 4750 2    60   Input ~ 0
USB_DATA17
Text GLabel 3850 4850 2    60   Input ~ 0
USB_DATA18
Text GLabel 3850 4950 2    60   Input ~ 0
USB_DATA19
Text GLabel 3850 5050 2    60   Input ~ 0
USB_DATA20
Text GLabel 3850 5150 2    60   Input ~ 0
USB_DATA21
Text GLabel 3850 5250 2    60   Input ~ 0
USB_DATA22
Text GLabel 3850 5350 2    60   Input ~ 0
USB_DATA23
Text GLabel 3850 5450 2    60   Input ~ 0
USB_DATA24
Text GLabel 3850 5550 2    60   Input ~ 0
USB_DATA25
Text GLabel 3850 5650 2    60   Input ~ 0
USB_DATA26
Text GLabel 3850 5750 2    60   Input ~ 0
USB_DATA27
Text GLabel 3850 5850 2    60   Input ~ 0
USB_DATA28
Text GLabel 3850 5950 2    60   Input ~ 0
USB_DATA29
Text GLabel 3850 6050 2    60   Input ~ 0
USB_DATA30
Text GLabel 3850 6150 2    60   Input ~ 0
USB_DATA31
Wire Wire Line
	3750 4650 3850 4650
Wire Wire Line
	3750 4750 3850 4750
Wire Wire Line
	3750 4850 3850 4850
Wire Wire Line
	3750 4950 3850 4950
Wire Wire Line
	3750 5050 3850 5050
Wire Wire Line
	3750 5150 3850 5150
Wire Wire Line
	3750 5250 3850 5250
Wire Wire Line
	3750 5350 3850 5350
Wire Wire Line
	3750 5450 3850 5450
Wire Wire Line
	3750 5550 3850 5550
Wire Wire Line
	3750 5650 3850 5650
Wire Wire Line
	3750 5750 3850 5750
Wire Wire Line
	3750 5850 3850 5850
Wire Wire Line
	3750 5950 3850 5950
Wire Wire Line
	3750 6050 3850 6050
Wire Wire Line
	3750 6150 3850 6150
Text GLabel 7650 4200 0    60   Input ~ 0
USB_DATA16
Text GLabel 9700 4200 2    60   Input ~ 0
USB_DATA17
Text GLabel 7650 4300 0    60   Input ~ 0
USB_DATA18
Text GLabel 9700 4300 2    60   Input ~ 0
USB_DATA19
Text GLabel 7650 4400 0    60   Input ~ 0
USB_DATA20
Text GLabel 9700 4400 2    60   Input ~ 0
USB_DATA21
Text GLabel 7650 4800 0    60   Input ~ 0
USB_DATA22
Text GLabel 9700 4800 2    60   Input ~ 0
USB_DATA23
Text GLabel 7650 4900 0    60   Input ~ 0
USB_DATA24
Text GLabel 9700 4900 2    60   Input ~ 0
USB_DATA25
Text GLabel 7650 5000 0    60   Input ~ 0
USB_DATA26
Text GLabel 9700 5000 2    60   Input ~ 0
USB_DATA27
Text GLabel 7650 5100 0    60   Input ~ 0
USB_DATA28
Text GLabel 9700 5100 2    60   Input ~ 0
USB_DATA29
Text GLabel 7650 5200 0    60   Input ~ 0
USB_DATA30
Text GLabel 9700 5200 2    60   Input ~ 0
USB_DATA31
Wire Wire Line
	7950 1600 8150 1600
Wire Wire Line
	9400 1600 9600 1600
Wire Wire Line
	9400 2500 9600 2500
Wire Wire Line
	7800 4500 8150 4500
Wire Wire Line
	9400 4500 9550 4500
Wire Wire Line
	9400 5400 9550 5400
NoConn ~ 8150 4000
NoConn ~ 8150 4100
NoConn ~ 8150 5400
NoConn ~ 8150 1100
NoConn ~ 8150 1200
NoConn ~ 8150 2500
NoConn ~ 9400 4100
NoConn ~ 9400 4000
Wire Wire Line
	8150 1300 7750 1300
Wire Wire Line
	7750 1400 8150 1400
Wire Wire Line
	8150 1500 7750 1500
Wire Wire Line
	7750 1700 8150 1700
Wire Wire Line
	7750 1800 8150 1800
Wire Wire Line
	7750 1900 8150 1900
Wire Wire Line
	7750 2000 8150 2000
Wire Wire Line
	7750 2100 8150 2100
Wire Wire Line
	7750 2200 8150 2200
Wire Wire Line
	7750 2300 8150 2300
Wire Wire Line
	7750 2400 8150 2400
Wire Wire Line
	8150 2600 7750 2600
Wire Wire Line
	7750 2700 8150 2700
Wire Wire Line
	7750 2800 8150 2800
Wire Wire Line
	7750 2900 8150 2900
Wire Wire Line
	7750 3000 8150 3000
Wire Wire Line
	9400 1300 9800 1300
Wire Wire Line
	9800 1400 9400 1400
Wire Wire Line
	9400 1500 9800 1500
Wire Wire Line
	9400 1700 9800 1700
Wire Wire Line
	9400 1800 9800 1800
Wire Wire Line
	9800 1900 9400 1900
Wire Wire Line
	9400 2000 9800 2000
Wire Wire Line
	9800 2100 9400 2100
Wire Wire Line
	9400 2200 9800 2200
Wire Wire Line
	9400 2300 9800 2300
Wire Wire Line
	9800 2400 9400 2400
Wire Wire Line
	9400 2600 9800 2600
Wire Wire Line
	9800 2700 9400 2700
NoConn ~ 9400 2800
NoConn ~ 9400 1200
NoConn ~ 9400 1100
Wire Wire Line
	7650 4200 8150 4200
Wire Wire Line
	8150 4300 7650 4300
Wire Wire Line
	7650 4400 8150 4400
Wire Wire Line
	7650 4800 8150 4800
Wire Wire Line
	8150 4900 7650 4900
Wire Wire Line
	7650 5000 8150 5000
Wire Wire Line
	8150 5100 7650 5100
Wire Wire Line
	7650 5200 8150 5200
Wire Wire Line
	9400 4400 9700 4400
Wire Wire Line
	9700 4300 9400 4300
Wire Wire Line
	9400 4200 9700 4200
NoConn ~ 9400 4600
NoConn ~ 8150 4600
NoConn ~ 8150 4700
Wire Wire Line
	9400 4800 9700 4800
Wire Wire Line
	9700 4900 9400 4900
Wire Wire Line
	9400 5000 9700 5000
Wire Wire Line
	9700 5100 9400 5100
Wire Wire Line
	9400 5200 9700 5200
NoConn ~ 9400 4700
NoConn ~ 9400 3000
Wire Wire Line
	9400 2900 9800 2900
Wire Wire Line
	9400 5300 9700 5300
Wire Wire Line
	9400 5500 9700 5500
Wire Wire Line
	9700 5600 9400 5600
Wire Wire Line
	9700 5700 9400 5700
Wire Wire Line
	9700 5800 9400 5800
Wire Wire Line
	9700 5900 9400 5900
Wire Wire Line
	7650 5500 8150 5500
Wire Wire Line
	7650 5600 8150 5600
Wire Wire Line
	7650 5700 8150 5700
Wire Wire Line
	7650 5800 8150 5800
Wire Wire Line
	8150 5900 7650 5900
NoConn ~ 8150 5300
$Sheet
S 5500 6750 900  750 
U 5B1B9700
F0 "Power Supply" 60
F1 "Power Supply.sch" 60
$EndSheet
Text Notes 800  7450 0    60   ~ 0
Note:\nAll resistors 1% tolerance.\nAll capacitors 10% tolerance.\nUnless otherwise noted
$EndSCHEMATC
