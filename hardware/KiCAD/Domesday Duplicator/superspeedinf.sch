EESchema Schematic File Version 2
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
LIBS:Domesday Duplicator-cache
EELAYER 25 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 2 4
Title "Domesday Duplicator"
Date "2017-11-04"
Rev "1.4"
Comp "http://www.domesday86.com"
Comment1 "(c)2017 Simon Inns"
Comment2 "License: Attribution-ShareAlike 4.0 International (CC BY-SA 4.0)"
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L GPIFII_J6 J4
U 1 1 59D0E982
P 7300 3900
F 0 "J4" H 6750 4950 60  0000 C CNN
F 1 "GPIFII_J6" H 6900 2750 60  0000 C CNN
F 2 "Pin_Headers:Pin_Header_Straight_2x20_Pitch2.54mm" H 7300 3900 60  0001 C CNN
F 3 "" H 7300 3900 60  0001 C CNN
	1    7300 3900
	1    0    0    -1  
$EndComp
$Comp
L GPIFII_J7 J3
U 1 1 59D0E9C8
P 4000 3900
F 0 "J3" H 3300 4950 60  0000 C CNN
F 1 "GPIFII_J7" H 3450 2750 60  0000 C CNN
F 2 "Pin_Headers:Pin_Header_Straight_2x20_Pitch2.54mm" H 4000 3900 60  0001 C CNN
F 3 "" H 4000 3900 60  0001 C CNN
	1    4000 3900
	1    0    0    -1  
$EndComp
Wire Wire Line
	3050 3600 2850 3600
Wire Wire Line
	2850 3600 2850 5550
Wire Wire Line
	2850 5400 8300 5400
Wire Wire Line
	8100 3200 8300 3200
Wire Wire Line
	8300 3200 8300 5400
Wire Wire Line
	4900 4900 5100 4900
Wire Wire Line
	5100 4900 5100 5400
Connection ~ 5100 5400
Wire Wire Line
	3050 4400 2850 4400
Connection ~ 2850 4400
Wire Wire Line
	3050 4900 2850 4900
Connection ~ 2850 4900
$Comp
L GND #PWR05
U 1 1 59D0EAAC
P 2850 5550
F 0 "#PWR05" H 2850 5300 50  0001 C CNN
F 1 "GND" H 2850 5400 50  0000 C CNN
F 2 "" H 2850 5550 50  0001 C CNN
F 3 "" H 2850 5550 50  0001 C CNN
	1    2850 5550
	1    0    0    -1  
$EndComp
Connection ~ 2850 5400
NoConn ~ 3050 3000
NoConn ~ 3050 3100
NoConn ~ 3050 3200
NoConn ~ 3050 3300
NoConn ~ 3050 3400
NoConn ~ 3050 3500
NoConn ~ 3050 3900
NoConn ~ 3050 4000
NoConn ~ 3050 4100
NoConn ~ 3050 4200
NoConn ~ 3050 4300
NoConn ~ 3050 4500
NoConn ~ 3050 4600
NoConn ~ 3050 4700
NoConn ~ 3050 4800
Wire Wire Line
	3050 3700 2850 3700
Connection ~ 2850 3700
Wire Wire Line
	3050 3800 2850 3800
Connection ~ 2850 3800
NoConn ~ 4900 3000
NoConn ~ 4900 3100
NoConn ~ 4900 3200
NoConn ~ 4900 3300
NoConn ~ 4900 3400
NoConn ~ 4900 3500
NoConn ~ 4900 3600
NoConn ~ 4900 3700
NoConn ~ 4900 3800
NoConn ~ 4900 3900
NoConn ~ 4900 4000
NoConn ~ 4900 4100
NoConn ~ 4900 4200
NoConn ~ 4900 4300
NoConn ~ 4900 4400
NoConn ~ 4900 4500
NoConn ~ 4900 4600
NoConn ~ 4900 4700
NoConn ~ 4900 4800
NoConn ~ 6500 3000
NoConn ~ 8100 3000
Wire Wire Line
	6300 3100 6500 3100
Wire Wire Line
	6300 2450 6300 3100
Wire Wire Line
	6300 2600 8300 2600
Wire Wire Line
	8300 2600 8300 3100
Wire Wire Line
	8300 3100 8100 3100
$Comp
L +5V #PWR06
U 1 1 59D0EC1B
P 6300 2450
F 0 "#PWR06" H 6300 2300 50  0001 C CNN
F 1 "+5V" H 6300 2590 50  0000 C CNN
F 2 "" H 6300 2450 50  0001 C CNN
F 3 "" H 6300 2450 50  0001 C CNN
	1    6300 2450
	1    0    0    -1  
$EndComp
Connection ~ 6300 2600
Text HLabel 8450 4800 2    60   BiDi ~ 0
DATA0
Text HLabel 8450 4700 2    60   BiDi ~ 0
DATA1
Text HLabel 8450 4600 2    60   BiDi ~ 0
DATA2
Text HLabel 8450 4500 2    60   BiDi ~ 0
DATA3
Text HLabel 8450 4400 2    60   BiDi ~ 0
DATA4
Text HLabel 8450 4300 2    60   BiDi ~ 0
DATA5
Text HLabel 8450 4200 2    60   BiDi ~ 0
DATA6
Text HLabel 8450 4100 2    60   BiDi ~ 0
DATA7
Text HLabel 8450 4000 2    60   BiDi ~ 0
DATA8
Text HLabel 8450 3900 2    60   BiDi ~ 0
DATA9
Text HLabel 8450 3800 2    60   BiDi ~ 0
DATA10
Text HLabel 8450 3700 2    60   BiDi ~ 0
DATA11
Text HLabel 8450 3600 2    60   BiDi ~ 0
DATA12
Text HLabel 8450 3500 2    60   BiDi ~ 0
DATA13
Text HLabel 8450 3400 2    60   BiDi ~ 0
DATA14
Text HLabel 8450 3300 2    60   BiDi ~ 0
DATA15
Wire Wire Line
	8100 3300 8450 3300
Wire Wire Line
	8100 3400 8450 3400
Wire Wire Line
	8100 3500 8450 3500
Wire Wire Line
	8100 3600 8450 3600
Wire Wire Line
	8100 3700 8450 3700
Wire Wire Line
	8100 3800 8450 3800
Wire Wire Line
	8100 3900 8450 3900
Wire Wire Line
	8100 4000 8450 4000
Wire Wire Line
	8100 4100 8450 4100
Wire Wire Line
	8100 4200 8450 4200
Wire Wire Line
	8100 4300 8450 4300
Wire Wire Line
	8100 4400 8450 4400
Wire Wire Line
	8100 4500 8450 4500
Wire Wire Line
	8100 4600 8450 4600
Wire Wire Line
	8100 4700 8450 4700
Wire Wire Line
	8100 4800 8450 4800
Text HLabel 8450 4900 2    60   Output ~ 0
SCL
Text HLabel 6100 4900 0    60   BiDi ~ 0
SDA
Text HLabel 6100 4500 0    60   BiDi ~ 0
CTL0
Text HLabel 6100 4400 0    60   BiDi ~ 0
CTL1
Text HLabel 6100 4300 0    60   BiDi ~ 0
CTL2
Text HLabel 6100 4200 0    60   BiDi ~ 0
CTL3
Text HLabel 6100 4100 0    60   BiDi ~ 0
CTL4
Text HLabel 6100 4000 0    60   BiDi ~ 0
CTL5
Text HLabel 6100 3900 0    60   BiDi ~ 0
CTL6
Text HLabel 6100 3800 0    60   BiDi ~ 0
CTL7
Text HLabel 6100 3700 0    60   BiDi ~ 0
CTL8
Text HLabel 6100 3600 0    60   BiDi ~ 0
CTL9
Text HLabel 6100 3500 0    60   BiDi ~ 0
CTL10
Text HLabel 6100 3400 0    60   BiDi ~ 0
CTL11
Text HLabel 6100 3300 0    60   BiDi ~ 0
CTL12
Text HLabel 6100 4700 0    60   BiDi ~ 0
PCLK
Wire Wire Line
	6100 3300 6500 3300
Wire Wire Line
	6500 3400 6100 3400
Wire Wire Line
	6100 3500 6500 3500
Wire Wire Line
	6500 3600 6100 3600
Wire Wire Line
	6100 3700 6500 3700
Wire Wire Line
	6500 3800 6100 3800
Wire Wire Line
	6100 3900 6500 3900
Wire Wire Line
	6500 4000 6100 4000
Wire Wire Line
	6100 4100 6500 4100
Wire Wire Line
	6500 4200 6100 4200
Wire Wire Line
	6100 4300 6500 4300
Wire Wire Line
	6500 4400 6100 4400
Wire Wire Line
	6100 4500 6500 4500
Wire Wire Line
	6500 4700 6100 4700
Wire Wire Line
	6500 4900 6100 4900
Wire Wire Line
	8100 4900 8450 4900
NoConn ~ 6500 3200
Wire Wire Line
	6500 4600 6300 4600
Wire Wire Line
	6300 4600 6300 5400
Connection ~ 6300 5400
Wire Wire Line
	6500 4800 6300 4800
Connection ~ 6300 4800
Text Notes 6100 3200 0    60   ~ 0
Key pin
Text Notes 2600 3250 0    60   ~ 0
Key pin
$EndSCHEMATC
