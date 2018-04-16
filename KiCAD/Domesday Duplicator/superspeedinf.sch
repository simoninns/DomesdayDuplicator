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
LIBS:Domesday Duplicator-cache
EELAYER 25 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 2 4
Title "Domesday Duplicator"
Date "2018-04-15"
Rev "2.2"
Comp "http://www.domesday86.com"
Comment1 "(c)2018 Simon Inns"
Comment2 "License: Attribution-ShareAlike 4.0 International (CC BY-SA 4.0)"
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L GPIFII_J6 J4
U 1 1 59D0E982
P 7850 3750
F 0 "J4" H 7300 4800 60  0000 C CNN
F 1 "GPIFII_J6" H 7450 2600 60  0000 C CNN
F 2 "Pin_Headers:Pin_Header_Straight_2x20_Pitch2.54mm" H 7850 3750 60  0001 C CNN
F 3 "" H 7850 3750 60  0001 C CNN
	1    7850 3750
	1    0    0    -1  
$EndComp
$Comp
L GPIFII_J7 J3
U 1 1 59D0E9C8
P 4100 3750
F 0 "J3" H 3400 4800 60  0000 C CNN
F 1 "GPIFII_J7" H 3550 2600 60  0000 C CNN
F 2 "Pin_Headers:Pin_Header_Straight_2x20_Pitch2.54mm" H 4100 3750 60  0001 C CNN
F 3 "" H 4100 3750 60  0001 C CNN
	1    4100 3750
	1    0    0    -1  
$EndComp
Wire Wire Line
	3150 3450 2950 3450
Wire Wire Line
	2950 3450 2950 5400
Wire Wire Line
	2950 5250 8850 5250
Wire Wire Line
	8650 3050 8850 3050
Wire Wire Line
	8850 3050 8850 5250
Wire Wire Line
	5000 4750 5200 4750
Wire Wire Line
	5200 4750 5200 5250
Connection ~ 5200 5250
Wire Wire Line
	3150 4250 2950 4250
Connection ~ 2950 4250
Wire Wire Line
	3150 4750 2950 4750
Connection ~ 2950 4750
$Comp
L GND #PWR05
U 1 1 59D0EAAC
P 2950 5400
F 0 "#PWR05" H 2950 5150 50  0001 C CNN
F 1 "GND" H 2950 5250 50  0000 C CNN
F 2 "" H 2950 5400 50  0001 C CNN
F 3 "" H 2950 5400 50  0001 C CNN
	1    2950 5400
	1    0    0    -1  
$EndComp
Connection ~ 2950 5250
NoConn ~ 3150 2850
NoConn ~ 3150 2950
NoConn ~ 3150 3050
NoConn ~ 3150 3150
NoConn ~ 3150 3250
NoConn ~ 3150 3350
NoConn ~ 3150 3750
NoConn ~ 3150 3850
NoConn ~ 3150 3950
NoConn ~ 3150 4050
NoConn ~ 3150 4150
NoConn ~ 3150 4350
NoConn ~ 3150 4450
NoConn ~ 3150 4550
NoConn ~ 3150 4650
Wire Wire Line
	3150 3550 2950 3550
Connection ~ 2950 3550
Wire Wire Line
	3150 3650 2950 3650
Connection ~ 2950 3650
NoConn ~ 5000 2850
NoConn ~ 5000 2950
NoConn ~ 5000 3450
NoConn ~ 7050 2850
NoConn ~ 8650 2850
Wire Wire Line
	6850 2950 7050 2950
Wire Wire Line
	6850 2300 6850 2950
Wire Wire Line
	6850 2450 8850 2450
Wire Wire Line
	8850 2450 8850 2950
Wire Wire Line
	8850 2950 8650 2950
$Comp
L +5V #PWR06
U 1 1 59D0EC1B
P 6850 2300
F 0 "#PWR06" H 6850 2150 50  0001 C CNN
F 1 "+5V" H 6850 2440 50  0000 C CNN
F 2 "" H 6850 2300 50  0001 C CNN
F 3 "" H 6850 2300 50  0001 C CNN
	1    6850 2300
	1    0    0    -1  
$EndComp
Connection ~ 6850 2450
Text HLabel 9000 4650 2    60   BiDi ~ 0
DATA0
Text HLabel 9000 4550 2    60   BiDi ~ 0
DATA1
Text HLabel 9000 4450 2    60   BiDi ~ 0
DATA2
Text HLabel 9000 4350 2    60   BiDi ~ 0
DATA3
Text HLabel 9000 4250 2    60   BiDi ~ 0
DATA4
Text HLabel 9000 4150 2    60   BiDi ~ 0
DATA5
Text HLabel 9000 4050 2    60   BiDi ~ 0
DATA6
Text HLabel 9000 3950 2    60   BiDi ~ 0
DATA7
Text HLabel 9000 3850 2    60   BiDi ~ 0
DATA8
Text HLabel 9000 3750 2    60   BiDi ~ 0
DATA9
Text HLabel 9000 3650 2    60   BiDi ~ 0
DATA10
Text HLabel 9000 3550 2    60   BiDi ~ 0
DATA11
Text HLabel 9000 3450 2    60   BiDi ~ 0
DATA12
Text HLabel 9000 3350 2    60   BiDi ~ 0
DATA13
Text HLabel 9000 3250 2    60   BiDi ~ 0
DATA14
Text HLabel 9000 3150 2    60   BiDi ~ 0
DATA15
Wire Wire Line
	8650 3150 9000 3150
Wire Wire Line
	8650 3250 9000 3250
Wire Wire Line
	8650 3350 9000 3350
Wire Wire Line
	8650 3450 9000 3450
Wire Wire Line
	8650 3550 9000 3550
Wire Wire Line
	8650 3650 9000 3650
Wire Wire Line
	8650 3750 9000 3750
Wire Wire Line
	8650 3850 9000 3850
Wire Wire Line
	8650 3950 9000 3950
Wire Wire Line
	8650 4050 9000 4050
Wire Wire Line
	8650 4150 9000 4150
Wire Wire Line
	8650 4250 9000 4250
Wire Wire Line
	8650 4350 9000 4350
Wire Wire Line
	8650 4450 9000 4450
Wire Wire Line
	8650 4550 9000 4550
Wire Wire Line
	8650 4650 9000 4650
Text HLabel 9000 4750 2    60   Output ~ 0
SCL
Text HLabel 6650 4750 0    60   BiDi ~ 0
SDA
Text HLabel 6650 4350 0    60   BiDi ~ 0
CTL0
Text HLabel 6650 4250 0    60   BiDi ~ 0
CTL1
Text HLabel 6650 4150 0    60   BiDi ~ 0
CTL2
Text HLabel 6650 4050 0    60   BiDi ~ 0
CTL3
Text HLabel 6650 3950 0    60   BiDi ~ 0
CTL4
Text HLabel 6650 3850 0    60   BiDi ~ 0
CTL5
Text HLabel 6650 3750 0    60   BiDi ~ 0
CTL6
Text HLabel 6650 3650 0    60   BiDi ~ 0
CTL7
Text HLabel 6650 3550 0    60   BiDi ~ 0
CTL8
Text HLabel 6650 3450 0    60   BiDi ~ 0
CTL9
Text HLabel 6650 3350 0    60   BiDi ~ 0
CTL10
Text HLabel 6650 3250 0    60   BiDi ~ 0
CTL11
Text HLabel 6650 3150 0    60   BiDi ~ 0
CTL12
Text HLabel 6650 4550 0    60   BiDi ~ 0
PCLK
Wire Wire Line
	6650 3150 7050 3150
Wire Wire Line
	7050 3250 6650 3250
Wire Wire Line
	6650 3350 7050 3350
Wire Wire Line
	7050 3450 6650 3450
Wire Wire Line
	6650 3550 7050 3550
Wire Wire Line
	7050 3650 6650 3650
Wire Wire Line
	6650 3750 7050 3750
Wire Wire Line
	7050 3850 6650 3850
Wire Wire Line
	6650 3950 7050 3950
Wire Wire Line
	7050 4050 6650 4050
Wire Wire Line
	6650 4150 7050 4150
Wire Wire Line
	7050 4250 6650 4250
Wire Wire Line
	6650 4350 7050 4350
Wire Wire Line
	7050 4550 6650 4550
Wire Wire Line
	7050 4750 6650 4750
Wire Wire Line
	8650 4750 9000 4750
NoConn ~ 7050 3050
Wire Wire Line
	7050 4450 6850 4450
Wire Wire Line
	6850 4450 6850 5250
Connection ~ 6850 5250
Wire Wire Line
	7050 4650 6850 4650
Connection ~ 6850 4650
Text Notes 6650 3050 0    60   ~ 0
Key pin
Text Notes 2700 3100 0    60   ~ 0
Key pin
Text HLabel 5350 4650 2    60   BiDi ~ 0
DATA16
Text HLabel 5350 4550 2    60   BiDi ~ 0
DATA17
Text HLabel 5350 4450 2    60   BiDi ~ 0
DATA18
Text HLabel 5350 4350 2    60   BiDi ~ 0
DATA19
Text HLabel 5350 4250 2    60   BiDi ~ 0
DATA20
Text HLabel 5350 4150 2    60   BiDi ~ 0
DATA21
Text HLabel 5350 4050 2    60   BiDi ~ 0
DATA22
Text HLabel 5350 3950 2    60   BiDi ~ 0
DATA23
Text HLabel 5350 3850 2    60   BiDi ~ 0
DATA24
Text HLabel 5350 3750 2    60   BiDi ~ 0
DATA25
Text HLabel 5350 3650 2    60   BiDi ~ 0
DATA26
Text HLabel 5350 3550 2    60   BiDi ~ 0
DATA27
Text HLabel 5350 3350 2    60   BiDi ~ 0
DATA28
Text HLabel 5350 3250 2    60   BiDi ~ 0
DATA29
Text HLabel 5350 3150 2    60   BiDi ~ 0
DATA30
Text HLabel 5350 3050 2    60   BiDi ~ 0
DATA31
Wire Wire Line
	5000 3050 5350 3050
Wire Wire Line
	5000 3150 5350 3150
Wire Wire Line
	5000 3250 5350 3250
Wire Wire Line
	5000 3350 5350 3350
Wire Wire Line
	5000 3550 5350 3550
Wire Wire Line
	5000 3650 5350 3650
Wire Wire Line
	5000 3750 5350 3750
Wire Wire Line
	5000 3850 5350 3850
Wire Wire Line
	5000 3950 5350 3950
Wire Wire Line
	5000 4050 5350 4050
Wire Wire Line
	5000 4150 5350 4150
Wire Wire Line
	5000 4250 5350 4250
Wire Wire Line
	5000 4350 5350 4350
Wire Wire Line
	5000 4450 5350 4450
Wire Wire Line
	5000 4550 5350 4550
Wire Wire Line
	5000 4650 5350 4650
$Comp
L PWR_FLAG #FLG07
U 1 1 5A0C2855
P 7300 2250
F 0 "#FLG07" H 7300 2325 50  0001 C CNN
F 1 "PWR_FLAG" H 7300 2400 50  0000 C CNN
F 2 "" H 7300 2250 50  0001 C CNN
F 3 "" H 7300 2250 50  0001 C CNN
	1    7300 2250
	1    0    0    -1  
$EndComp
Wire Wire Line
	7300 2250 7300 2450
Connection ~ 7300 2450
Text Notes 7600 2350 0    60   ~ 0
Power is supplied from the VBUS of\nthe USB3 connection, so we flag the\nsupply point here
$Comp
L PWR_FLAG #FLG08
U 1 1 5A0C2E66
P 6000 5000
F 0 "#FLG08" H 6000 5075 50  0001 C CNN
F 1 "PWR_FLAG" H 6000 5150 50  0000 C CNN
F 2 "" H 6000 5000 50  0001 C CNN
F 3 "" H 6000 5000 50  0001 C CNN
	1    6000 5000
	1    0    0    -1  
$EndComp
Wire Wire Line
	6000 5000 6000 5250
Connection ~ 6000 5250
$EndSCHEMATC
