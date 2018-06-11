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
Sheet 5 5
Title "Domesday Duplicator - Power Supply"
Date "2018-06-10"
Rev "2.3"
Comp "https://www.domesday86.com"
Comment1 "(c)2018 Simon Inns"
Comment2 "License: Attribution-ShareAlike 4.0 International (CC BY-SA 4.0)"
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L CP C501
U 1 1 5B1B9C94
P 5300 3950
F 0 "C501" H 5325 4050 50  0000 L CNN
F 1 "10uF TANT" H 5325 3850 50  0000 L CNN
F 2 "Capacitors_Tantalum_SMD:CP_Tantalum_Case-A_EIA-3216-18_Reflow" H 5338 3800 50  0001 C CNN
F 3 "" H 5300 3950 50  0001 C CNN
	1    5300 3950
	1    0    0    -1  
$EndComp
$Comp
L C C502
U 1 1 5B1B9C9B
P 5800 3950
F 0 "C502" H 5825 4050 50  0000 L CNN
F 1 "100nF" H 5825 3850 50  0000 L CNN
F 2 "Capacitors_SMD:C_0805" H 5838 3800 50  0001 C CNN
F 3 "" H 5800 3950 50  0001 C CNN
	1    5800 3950
	1    0    0    -1  
$EndComp
$Comp
L CP C503
U 1 1 5B1B9CA2
P 6800 3950
F 0 "C503" H 6825 4050 50  0000 L CNN
F 1 "10uF TANT" H 6825 3850 50  0000 L CNN
F 2 "Capacitors_Tantalum_SMD:CP_Tantalum_Case-A_EIA-3216-18_Reflow" H 6838 3800 50  0001 C CNN
F 3 "" H 6800 3950 50  0001 C CNN
	1    6800 3950
	1    0    0    -1  
$EndComp
$Comp
L C C504
U 1 1 5B1B9CA9
P 7300 3950
F 0 "C504" H 7325 4050 50  0000 L CNN
F 1 "100nF" H 7325 3850 50  0000 L CNN
F 2 "Capacitors_SMD:C_0805" H 7338 3800 50  0001 C CNN
F 3 "" H 7300 3950 50  0001 C CNN
	1    7300 3950
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR026
U 1 1 5B1B9CB0
P 6300 4300
F 0 "#PWR026" H 6300 4050 50  0001 C CNN
F 1 "GND" H 6300 4150 50  0000 C CNN
F 2 "" H 6300 4300 50  0001 C CNN
F 3 "" H 6300 4300 50  0001 C CNN
	1    6300 4300
	1    0    0    -1  
$EndComp
$Comp
L +5VA #PWR027
U 1 1 5B1B9CB6
P 5300 3400
F 0 "#PWR027" H 5300 3250 50  0001 C CNN
F 1 "+5VA" H 5300 3540 50  0000 C CNN
F 2 "" H 5300 3400 50  0001 C CNN
F 3 "" H 5300 3400 50  0001 C CNN
	1    5300 3400
	1    0    0    -1  
$EndComp
$Comp
L L L501
U 1 1 5B1B9CBC
P 4850 3700
F 0 "L501" V 4800 3700 50  0000 C CNN
F 1 "100uH" V 4925 3700 50  0000 C CNN
F 2 "Inductors:Inductor_Bourns-SRU8043" H 4850 3700 50  0001 C CNN
F 3 "" H 4850 3700 50  0001 C CNN
	1    4850 3700
	0    1    1    0   
$EndComp
$Comp
L +5V #PWR028
U 1 1 5B1B9CC3
P 4550 3600
F 0 "#PWR028" H 4550 3450 50  0001 C CNN
F 1 "+5V" H 4550 3740 50  0000 C CNN
F 2 "" H 4550 3600 50  0001 C CNN
F 3 "" H 4550 3600 50  0001 C CNN
	1    4550 3600
	1    0    0    -1  
$EndComp
$Comp
L +3.3VA #PWR029
U 1 1 5B1B9CC9
P 7300 3600
F 0 "#PWR029" H 7300 3450 50  0001 C CNN
F 1 "+3.3VA" H 7300 3740 50  0000 C CNN
F 2 "" H 7300 3600 50  0001 C CNN
F 3 "" H 7300 3600 50  0001 C CNN
	1    7300 3600
	1    0    0    -1  
$EndComp
$Comp
L PWR_FLAG #FLG030
U 1 1 5B1B9CCF
P 5550 3250
F 0 "#FLG030" H 5550 3325 50  0001 C CNN
F 1 "PWR_FLAG" H 5550 3400 50  0000 C CNN
F 2 "" H 5550 3250 50  0001 C CNN
F 3 "" H 5550 3250 50  0001 C CNN
	1    5550 3250
	1    0    0    -1  
$EndComp
Wire Wire Line
	5300 4100 5300 4200
Wire Wire Line
	5300 4200 7300 4200
Wire Wire Line
	7300 4200 7300 4100
Wire Wire Line
	6800 4100 6800 4200
Connection ~ 6800 4200
Wire Wire Line
	5800 4100 5800 4200
Connection ~ 5800 4200
Wire Wire Line
	6300 4000 6300 4300
Connection ~ 6300 4200
Wire Wire Line
	5300 3400 5300 3800
Wire Wire Line
	5000 3700 6000 3700
Wire Wire Line
	6600 3700 7600 3700
Wire Wire Line
	7300 3600 7300 3800
Wire Wire Line
	6800 3800 6800 3700
Connection ~ 6800 3700
Wire Wire Line
	5800 3800 5800 3700
Connection ~ 5800 3700
Connection ~ 7300 3700
Connection ~ 5300 3700
Wire Wire Line
	4700 3700 4550 3700
Wire Wire Line
	4550 3700 4550 3600
Wire Wire Line
	5550 3500 5550 3250
Wire Wire Line
	5100 3500 5550 3500
Connection ~ 5300 3500
$Comp
L TEST TP501
U 1 1 5B1D6A1F
P 5100 3400
F 0 "TP501" H 5100 3700 50  0000 C BNN
F 1 "5V" H 5100 3650 50  0000 C CNN
F 2 "Connectors_TestPoints:Test_Point_Pad_1.5x1.5mm" H 5100 3400 50  0001 C CNN
F 3 "" H 5100 3400 50  0001 C CNN
	1    5100 3400
	1    0    0    -1  
$EndComp
$Comp
L TEST TP503
U 1 1 5B1D6A51
P 7600 3700
F 0 "TP503" H 7600 4000 50  0000 C BNN
F 1 "3V3" H 7600 3950 50  0000 C CNN
F 2 "Connectors_TestPoints:Test_Point_Pad_1.5x1.5mm" H 7600 3700 50  0001 C CNN
F 3 "" H 7600 3700 50  0001 C CNN
	1    7600 3700
	1    0    0    -1  
$EndComp
$Comp
L TEST TP502
U 1 1 5B1D6A8D
P 6550 4300
F 0 "TP502" H 6550 4600 50  0000 C BNN
F 1 "GND" H 6550 4550 50  0000 C CNN
F 2 "Connectors_TestPoints:Test_Point_Pad_1.5x1.5mm" H 6550 4300 50  0001 C CNN
F 3 "" H 6550 4300 50  0001 C CNN
	1    6550 4300
	-1   0    0    1   
$EndComp
Wire Wire Line
	6550 4300 6550 4200
Connection ~ 6550 4200
Wire Wire Line
	5100 3400 5100 3500
$Comp
L LM1117-3.3 U501
U 1 1 5B1D87AA
P 6300 3700
F 0 "U501" H 6150 3825 50  0000 C CNN
F 1 "LM1117-3.3" H 6300 3825 50  0000 L CNN
F 2 "TO_SOT_Packages_SMD:SOT-223-3_TabPin2" H 6300 3700 50  0001 C CNN
F 3 "" H 6300 3700 50  0001 C CNN
	1    6300 3700
	1    0    0    -1  
$EndComp
$EndSCHEMATC
