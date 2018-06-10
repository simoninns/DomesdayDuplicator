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
Sheet 4 5
Title "Domesday Duplicator"
Date "2018-06-10"
Rev "2.3"
Comp "https://www.domesday86.com"
Comment1 "(c)2018 Simon Inns"
Comment2 "License: Attribution-ShareAlike 4.0 International (CC BY-SA 4.0)"
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L OPA690 U401
U 1 1 59D279FB
P 5350 3200
F 0 "U401" H 5050 2750 60  0000 C CNN
F 1 "OPA690" H 5150 3200 60  0000 C CNN
F 2 "Housings_SOIC:SOIC-8_3.9x4.9mm_Pitch1.27mm" H 5350 3200 60  0001 C CNN
F 3 "" H 5350 3200 60  0001 C CNN
	1    5350 3200
	1    0    0    -1  
$EndComp
$Comp
L C C401
U 1 1 59D27BE7
P 3950 3050
F 0 "C401" H 3975 3150 50  0000 L CNN
F 1 "100nF" H 3975 2950 50  0000 L CNN
F 2 "Capacitors_SMD:C_0805" H 3988 2900 50  0001 C CNN
F 3 "" H 3950 3050 50  0001 C CNN
	1    3950 3050
	0    1    1    0   
$EndComp
$Comp
L R R402
U 1 1 59D27C9C
P 4250 2800
F 0 "R402" V 4330 2800 50  0000 C CNN
F 1 "1K" V 4250 2800 50  0000 C CNN
F 2 "Resistors_SMD:R_0805" V 4180 2800 50  0001 C CNN
F 3 "" H 4250 2800 50  0001 C CNN
	1    4250 2800
	-1   0    0    1   
$EndComp
$Comp
L R R403
U 1 1 59D27D22
P 4250 3300
F 0 "R403" V 4330 3300 50  0000 C CNN
F 1 "1K" V 4250 3300 50  0000 C CNN
F 2 "Resistors_SMD:R_0805" V 4180 3300 50  0001 C CNN
F 3 "" H 4250 3300 50  0001 C CNN
	1    4250 3300
	-1   0    0    1   
$EndComp
Wire Wire Line
	4250 2950 4250 3150
Connection ~ 4250 3050
$Comp
L R R405
U 1 1 59D27FB9
P 4650 4050
F 0 "R405" V 4730 4050 50  0000 C CNN
F 1 "200R" V 4650 4050 50  0000 C CNN
F 2 "Resistors_SMD:R_0805" V 4580 4050 50  0001 C CNN
F 3 "" H 4650 4050 50  0001 C CNN
	1    4650 4050
	-1   0    0    1   
$EndComp
$Comp
L C C402
U 1 1 59D27FF2
P 4650 4450
F 0 "C402" H 4675 4550 50  0000 L CNN
F 1 "100nF" H 4675 4350 50  0000 L CNN
F 2 "Capacitors_SMD:C_0805" H 4688 4300 50  0001 C CNN
F 3 "" H 4650 4450 50  0001 C CNN
	1    4650 4450
	1    0    0    -1  
$EndComp
Wire Wire Line
	4650 3350 4650 3900
Wire Wire Line
	4650 3350 4750 3350
Wire Wire Line
	4650 4200 4650 4300
$Comp
L R R406
U 1 1 59D28163
P 5700 3800
F 0 "R406" V 5780 3800 50  0000 C CNN
F 1 "1K5" V 5700 3800 50  0000 C CNN
F 2 "Resistors_SMD:R_0805" V 5630 3800 50  0001 C CNN
F 3 "" H 5700 3800 50  0001 C CNN
	1    5700 3800
	0    -1   -1   0   
$EndComp
Wire Wire Line
	5550 3800 4650 3800
Connection ~ 4650 3800
Wire Wire Line
	6050 3800 5850 3800
Wire Wire Line
	6050 2850 6050 3800
Wire Wire Line
	5550 2900 5550 2600
Wire Wire Line
	5550 2600 5350 2600
Wire Wire Line
	5350 2450 5350 2800
$Comp
L +5VA #PWR019
U 1 1 59D2836E
P 5350 2450
F 0 "#PWR019" H 5350 2300 50  0001 C CNN
F 1 "+5VA" H 5350 2590 50  0000 C CNN
F 2 "" H 5350 2450 50  0001 C CNN
F 3 "" H 5350 2450 50  0001 C CNN
	1    5350 2450
	1    0    0    -1  
$EndComp
Connection ~ 5350 2600
$Comp
L GND #PWR020
U 1 1 59D283BC
P 5350 4850
F 0 "#PWR020" H 5350 4600 50  0001 C CNN
F 1 "GND" H 5350 4700 50  0000 C CNN
F 2 "" H 5350 4850 50  0001 C CNN
F 3 "" H 5350 4850 50  0001 C CNN
	1    5350 4850
	1    0    0    -1  
$EndComp
Text HLabel 4250 2550 1    60   Input ~ 0
REFT
Text HLabel 4250 3550 3    60   Input ~ 0
REFB
Wire Wire Line
	4250 3550 4250 3450
Wire Wire Line
	4250 2650 4250 2550
Text HLabel 9000 3200 2    60   Output ~ 0
RF_OUT
$Comp
L C C408
U 1 1 59D286FD
P 10150 5800
F 0 "C408" H 10175 5900 50  0000 L CNN
F 1 "100nF" H 10175 5700 50  0000 L CNN
F 2 "Capacitors_SMD:C_0805" H 10188 5650 50  0001 C CNN
F 3 "" H 10150 5800 50  0001 C CNN
	1    10150 5800
	1    0    0    -1  
$EndComp
$Comp
L CP C409
U 1 1 59D2875B
P 10650 5800
F 0 "C409" H 10675 5900 50  0000 L CNN
F 1 "2.2uF Tant" H 10675 5700 50  0000 L CNN
F 2 "Capacitors_Tantalum_SMD:CP_Tantalum_Case-A_EIA-3216-18_Reflow" H 10688 5650 50  0001 C CNN
F 3 "" H 10650 5800 50  0001 C CNN
	1    10650 5800
	1    0    0    -1  
$EndComp
$Comp
L +5VA #PWR021
U 1 1 59D287B5
P 10150 5450
F 0 "#PWR021" H 10150 5300 50  0001 C CNN
F 1 "+5VA" H 10150 5590 50  0000 C CNN
F 2 "" H 10150 5450 50  0001 C CNN
F 3 "" H 10150 5450 50  0001 C CNN
	1    10150 5450
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR022
U 1 1 59D287E7
P 10150 6150
F 0 "#PWR022" H 10150 5900 50  0001 C CNN
F 1 "GND" H 10150 6000 50  0000 C CNN
F 2 "" H 10150 6150 50  0001 C CNN
F 3 "" H 10150 6150 50  0001 C CNN
	1    10150 6150
	1    0    0    -1  
$EndComp
Wire Wire Line
	10150 5450 10150 5650
Wire Wire Line
	10150 5950 10150 6150
Wire Wire Line
	10650 5950 10650 6050
Wire Wire Line
	10650 6050 10150 6050
Connection ~ 10150 6050
Wire Wire Line
	10650 5650 10650 5550
Wire Wire Line
	10650 5550 10150 5550
Connection ~ 10150 5550
$Comp
L BNC_Rosenberger J401
U 1 1 5A0BB714
P 3150 3050
F 0 "J401" H 3000 3300 60  0000 C CNN
F 1 "BNC_Rosenberger" V 2850 2850 60  0000 C CNN
F 2 "BNC_Rosenberger:BNC Socket" H 3150 3050 60  0001 C CNN
F 3 "" H 3150 3050 60  0001 C CNN
	1    3150 3050
	1    0    0    -1  
$EndComp
Wire Wire Line
	3550 3050 3800 3050
Wire Wire Line
	3350 3350 3500 3350
Wire Wire Line
	3500 3350 3500 4700
Wire Wire Line
	5350 3600 5350 4850
Connection ~ 5350 4700
Wire Wire Line
	4650 4700 4650 4600
Connection ~ 4650 4700
Wire Wire Line
	3350 3450 3500 3450
Connection ~ 3500 3450
Wire Wire Line
	3350 3550 3500 3550
Connection ~ 3500 3550
Wire Wire Line
	3350 3650 3500 3650
Connection ~ 3500 3650
Wire Wire Line
	5950 3200 6600 3200
Wire Wire Line
	3500 4700 7500 4700
$Comp
L R R401
U 1 1 5B1B6C41
P 3700 3300
F 0 "R401" V 3780 3300 50  0000 C CNN
F 1 "56R" V 3700 3300 50  0000 C CNN
F 2 "Resistors_SMD:R_0805" V 3630 3300 50  0001 C CNN
F 3 "" H 3700 3300 50  0001 C CNN
	1    3700 3300
	-1   0    0    1   
$EndComp
$Comp
L R R404
U 1 1 5B1B6CE6
P 4500 3050
F 0 "R404" V 4580 3050 50  0000 C CNN
F 1 "100R" V 4500 3050 50  0000 C CNN
F 2 "Resistors_SMD:R_0805" V 4430 3050 50  0001 C CNN
F 3 "" H 4500 3050 50  0001 C CNN
	1    4500 3050
	0    -1   -1   0   
$EndComp
Wire Wire Line
	3700 2850 3700 3150
Connection ~ 3700 3050
Wire Wire Line
	3700 3450 3700 4700
Connection ~ 3700 4700
Wire Wire Line
	4100 3050 4350 3050
Wire Wire Line
	4650 3050 4750 3050
$Comp
L R R407
U 1 1 5B1B6E8C
P 6750 3200
F 0 "R407" V 6830 3200 50  0000 C CNN
F 1 "47R" V 6750 3200 50  0000 C CNN
F 2 "Resistors_SMD:R_0805" V 6680 3200 50  0001 C CNN
F 3 "" H 6750 3200 50  0001 C CNN
	1    6750 3200
	0    -1   -1   0   
$EndComp
Connection ~ 6050 3200
$Comp
L L L401
U 1 1 5B1B7A50
P 7250 3200
F 0 "L401" V 7200 3200 50  0000 C CNN
F 1 "680nH" V 7325 3200 50  0000 C CNN
F 2 "Inductors_SMD:L_0805" H 7250 3200 50  0001 C CNN
F 3 "" H 7250 3200 50  0001 C CNN
	1    7250 3200
	0    1    1    0   
$EndComp
$Comp
L L L402
U 1 1 5B1B7ADD
P 7750 3200
F 0 "L402" V 7700 3200 50  0000 C CNN
F 1 "580nH" V 7825 3200 50  0000 C CNN
F 2 "Inductors_SMD:L_0805" H 7750 3200 50  0001 C CNN
F 3 "" H 7750 3200 50  0001 C CNN
	1    7750 3200
	0    1    1    0   
$EndComp
$Comp
L C C403
U 1 1 5B1B7B48
P 7000 3500
F 0 "C403" H 7025 3600 50  0000 L CNN
F 1 "330pF" H 7025 3400 50  0000 L CNN
F 2 "Capacitors_SMD:C_0805" H 7038 3350 50  0001 C CNN
F 3 "" H 7000 3500 50  0001 C CNN
	1    7000 3500
	1    0    0    -1  
$EndComp
$Comp
L C C405
U 1 1 5B1B7BCA
P 7500 3500
F 0 "C405" H 7525 3600 50  0000 L CNN
F 1 "470pF" H 7525 3400 50  0000 L CNN
F 2 "Capacitors_SMD:C_0805" H 7538 3350 50  0001 C CNN
F 3 "" H 7500 3500 50  0001 C CNN
	1    7500 3500
	1    0    0    -1  
$EndComp
$Comp
L C C407
U 1 1 5B1B7C52
P 8000 3500
F 0 "C407" H 8025 3600 50  0000 L CNN
F 1 "270pF" H 8025 3400 50  0000 L CNN
F 2 "Capacitors_SMD:C_0805" H 8038 3350 50  0001 C CNN
F 3 "" H 8000 3500 50  0001 C CNN
	1    8000 3500
	1    0    0    -1  
$EndComp
$Comp
L C C404
U 1 1 5B1B7CAA
P 7250 2950
F 0 "C404" H 7275 3050 50  0000 L CNN
F 1 "33pF" H 7275 2850 50  0000 L CNN
F 2 "Capacitors_SMD:C_0805" H 7288 2800 50  0001 C CNN
F 3 "" H 7250 2950 50  0001 C CNN
	1    7250 2950
	0    -1   -1   0   
$EndComp
$Comp
L C C406
U 1 1 5B1B7D0D
P 7750 2950
F 0 "C406" H 7775 3050 50  0000 L CNN
F 1 "100pF" H 7775 2850 50  0000 L CNN
F 2 "Capacitors_SMD:C_0805" H 7788 2800 50  0001 C CNN
F 3 "" H 7750 2950 50  0001 C CNN
	1    7750 2950
	0    -1   -1   0   
$EndComp
Wire Wire Line
	7000 3650 7000 3750
Wire Wire Line
	7000 3750 8000 3750
Wire Wire Line
	8000 3750 8000 3650
Wire Wire Line
	7500 4700 7500 3650
Connection ~ 7500 3750
Wire Wire Line
	7000 2950 7000 3350
Wire Wire Line
	6900 3200 7100 3200
Wire Wire Line
	7000 2950 7100 2950
Connection ~ 7000 3200
Wire Wire Line
	7400 3200 7600 3200
Wire Wire Line
	7500 2950 7500 3350
Connection ~ 7500 3200
Wire Wire Line
	7900 3200 8100 3200
Wire Wire Line
	8000 2950 8000 3350
Wire Wire Line
	7900 2950 8000 2950
Connection ~ 8000 3200
Wire Wire Line
	7400 2950 7600 2950
Connection ~ 7500 2950
$Comp
L R R408
U 1 1 5B1B8270
P 8250 3200
F 0 "R408" V 8330 3200 50  0000 C CNN
F 1 "47R" V 8250 3200 50  0000 C CNN
F 2 "Resistors_SMD:R_0805" V 8180 3200 50  0001 C CNN
F 3 "" H 8250 3200 50  0001 C CNN
	1    8250 3200
	0    -1   -1   0   
$EndComp
Wire Notes Line
	6450 4250 6450 2400
Wire Wire Line
	8400 3200 9000 3200
Wire Notes Line
	8550 4250 8550 2400
Wire Notes Line
	8550 4250 6450 4250
Wire Notes Line
	8550 2400 6450 2400
Text Notes 7800 4200 0    60   ~ 0
Filter capacitor\ntolerance 5%
Text Notes 6500 2550 0    60   ~ 0
2N Elliptic filter (14 MHz low-pass)
Text Notes 3000 5950 0    60   ~ 0
Note:\nThe cable terminating resistor should\ntake into account the parallel resistance of\nthe dividing network:\n\n1000 / 2 = 500 Ohms\n\nRtotal = R1×R2/(R1+R2)\n(500 x 56) / (500 + 56) = 50.36 Ohms\n
Text Notes 2500 3950 0    60   ~ 0
50 Ohm impedance\nRF input
$Comp
L TEST TP401
U 1 1 5B1D41F6
P 3700 2850
F 0 "TP401" H 3700 3150 50  0000 C BNN
F 1 "RFin_Test" H 3700 3100 50  0000 C CNN
F 2 "Connectors_TestPoints:Test_Point_Pad_1.5x1.5mm" H 3700 2850 50  0001 C CNN
F 3 "" H 3700 2850 50  0001 C CNN
	1    3700 2850
	1    0    0    -1  
$EndComp
$Comp
L TEST TP402
U 1 1 5B1D42CC
P 6050 2850
F 0 "TP402" H 6050 3150 50  0000 C BNN
F 1 "Gain_Test" H 6050 3100 50  0000 C CNN
F 2 "Connectors_TestPoints:Test_Point_Pad_1.5x1.5mm" H 6050 2850 50  0001 C CNN
F 3 "" H 6050 2850 50  0001 C CNN
	1    6050 2850
	1    0    0    -1  
$EndComp
$Comp
L TEST TP403
U 1 1 5B1D4381
P 8800 2850
F 0 "TP403" H 8800 3150 50  0000 C BNN
F 1 "RFout_Test" H 8800 3100 50  0000 C CNN
F 2 "Connectors_TestPoints:Test_Point_Pad_1.5x1.5mm" H 8800 2850 50  0001 C CNN
F 3 "" H 8800 2850 50  0001 C CNN
	1    8800 2850
	1    0    0    -1  
$EndComp
Wire Wire Line
	8800 2850 8800 3200
Connection ~ 8800 3200
Text Notes 5400 4000 0    60   ~ 0
Gain setting 'A'
Text Notes 4500 4350 1    60   ~ 0
Gain setting 'B'
Text Notes 5600 5550 0    60   ~ 0
Note:\nThere are two gain setting resistors A and\nB.  These must be 200-1500R.  Please see\nproject notes for details about value \nselection
$EndSCHEMATC
