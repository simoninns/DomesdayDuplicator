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
Sheet 5 5
Title "Domesday Duplicator - Power Supply"
Date "2018-06-09"
Rev "3.0"
Comp "https://www.domesday86.com"
Comment1 "(c)2018 Simon Inns"
Comment2 "License: Attribution-ShareAlike 4.0 International (CC BY-SA 4.0)"
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L LM1117-3.3-RESCUE-Domesday_Duplicator U501
U 1 1 5B1B9C8D
P 6050 3700
F 0 "U501" H 6150 3450 50  0000 C CNN
F 1 "LM1117-3.3" H 6050 3950 50  0000 C CNN
F 2 "TO_SOT_Packages_SMD:SOT-223" H 6050 3700 50  0001 C CNN
F 3 "" H 6050 3700 50  0001 C CNN
	1    6050 3700
	1    0    0    -1  
$EndComp
$Comp
L CP C501
U 1 1 5B1B9C94
P 5300 3950
F 0 "C501" H 5325 4050 50  0000 L CNN
F 1 "10uF" H 5325 3850 50  0000 L CNN
F 2 "Capacitors_SMD:CP_Elec_4x5.7" H 5338 3800 50  0001 C CNN
F 3 "" H 5300 3950 50  0001 C CNN
	1    5300 3950
	1    0    0    -1  
$EndComp
$Comp
L C C502
U 1 1 5B1B9C9B
P 5550 3950
F 0 "C502" H 5575 4050 50  0000 L CNN
F 1 "100nF" H 5575 3850 50  0000 L CNN
F 2 "Capacitors_SMD:C_0805" H 5588 3800 50  0001 C CNN
F 3 "" H 5550 3950 50  0001 C CNN
	1    5550 3950
	1    0    0    -1  
$EndComp
$Comp
L CP C503
U 1 1 5B1B9CA2
P 6550 3950
F 0 "C503" H 6575 4050 50  0000 L CNN
F 1 "10uF" H 6575 3850 50  0000 L CNN
F 2 "Capacitors_SMD:CP_Elec_4x5.7" H 6588 3800 50  0001 C CNN
F 3 "" H 6550 3950 50  0001 C CNN
	1    6550 3950
	1    0    0    -1  
$EndComp
$Comp
L C C504
U 1 1 5B1B9CA9
P 6800 3950
F 0 "C504" H 6825 4050 50  0000 L CNN
F 1 "100nF" H 6825 3850 50  0000 L CNN
F 2 "Capacitors_SMD:C_0805" H 6838 3800 50  0001 C CNN
F 3 "" H 6800 3950 50  0001 C CNN
	1    6800 3950
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR503
U 1 1 5B1B9CB0
P 6050 4300
F 0 "#PWR503" H 6050 4050 50  0001 C CNN
F 1 "GND" H 6050 4150 50  0000 C CNN
F 2 "" H 6050 4300 50  0001 C CNN
F 3 "" H 6050 4300 50  0001 C CNN
	1    6050 4300
	1    0    0    -1  
$EndComp
$Comp
L +5VA #PWR502
U 1 1 5B1B9CB6
P 5300 3400
F 0 "#PWR502" H 5300 3250 50  0001 C CNN
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
L +5V #PWR501
U 1 1 5B1B9CC3
P 4550 3600
F 0 "#PWR501" H 4550 3450 50  0001 C CNN
F 1 "+5V" H 4550 3740 50  0000 C CNN
F 2 "" H 4550 3600 50  0001 C CNN
F 3 "" H 4550 3600 50  0001 C CNN
	1    4550 3600
	1    0    0    -1  
$EndComp
$Comp
L +3.3VA #PWR504
U 1 1 5B1B9CC9
P 6800 3600
F 0 "#PWR504" H 6800 3450 50  0001 C CNN
F 1 "+3.3VA" H 6800 3740 50  0000 C CNN
F 2 "" H 6800 3600 50  0001 C CNN
F 3 "" H 6800 3600 50  0001 C CNN
	1    6800 3600
	1    0    0    -1  
$EndComp
$Comp
L PWR_FLAG #FLG501
U 1 1 5B1B9CCF
P 5650 3400
F 0 "#FLG501" H 5650 3475 50  0001 C CNN
F 1 "PWR_FLAG" H 5650 3550 50  0000 C CNN
F 2 "" H 5650 3400 50  0001 C CNN
F 3 "" H 5650 3400 50  0001 C CNN
	1    5650 3400
	1    0    0    -1  
$EndComp
Wire Wire Line
	5300 4100 5300 4200
Wire Wire Line
	5300 4200 6800 4200
Wire Wire Line
	6800 4200 6800 4100
Wire Wire Line
	6550 4100 6550 4200
Connection ~ 6550 4200
Wire Wire Line
	5550 4100 5550 4200
Connection ~ 5550 4200
Wire Wire Line
	6050 4000 6050 4300
Connection ~ 6050 4200
Wire Wire Line
	5300 3400 5300 3800
Wire Wire Line
	5000 3700 5750 3700
Wire Wire Line
	6350 3700 6800 3700
Wire Wire Line
	6800 3600 6800 3800
Wire Wire Line
	6550 3800 6550 3700
Connection ~ 6550 3700
Wire Wire Line
	5550 3800 5550 3700
Connection ~ 5550 3700
Connection ~ 6800 3700
Connection ~ 5300 3700
Wire Wire Line
	4700 3700 4550 3700
Wire Wire Line
	4550 3700 4550 3600
Wire Wire Line
	5650 3400 5650 3500
Wire Wire Line
	5650 3500 5300 3500
Connection ~ 5300 3500
$EndSCHEMATC
