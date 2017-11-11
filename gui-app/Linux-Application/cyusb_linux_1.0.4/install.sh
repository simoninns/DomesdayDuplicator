#!/bin/bash

# This is the main installer for CyUSB Suite for Linux
# Run this script as root (superuser).
# (C) Cypress Semiconductor Company / ATR-LABS (www.atr-labs.com) - 2012
# Author - V. Radhakrishnan
#

CURDIR=`pwd`
echo "Your current directory is $CURDIR. This is where the cyusb_suite software will be installed..."
A=`whoami`

create_udev_rules() {
	echo "# Cypress USB driver for FX2 and FX3 (C) Cypress Semiconductor Corporation / ATR-LABS" > configs/88-cyusb.rules
	echo "# Rules written by V. Radhakrishnan ( rk@atr-labs.com )" >> configs/88-cyusb.rules
	echo "# Cypress USB vendor ID = 0x04b4" >> configs/88-cyusb.rules

	echo 'KERNEL=="*", SUBSYSTEM=="usb", ENV{DEVTYPE}=="usb_device", ACTION=="add", ATTR{idVendor}=="04b4", MODE="666", TAG="cyusb_dev", RUN+="'"/usr/local/bin/cy_renumerate.sh A"'"' >> configs/88-cyusb.rules
	echo 'KERNEL=="*", SUBSYSTEM=="usb", ENV{DEVTYPE}=="usb_device", ACTION=="remove", TAG=="cyusb_dev", RUN+="'"/usr/local/bin/cy_renumerate.sh R"'"' >> configs/88-cyusb.rules
}

if [ $A != 'root' ]; then
   echo "You have to be root to run this script"
   exit 1;
fi

create_udev_rules
cp configs/cyusb.conf /etc/
cp configs/88-cyusb.rules /etc/udev/rules.d/
make

# Remove stale versions of the libcyusb library
rm -f /usr/lib/libcyusb.so* /usr/local/lib/libcyusb.so*

# Copy the libcyusb library into the system library folders.
cp lib/libcyusb.so.1 /usr/local/lib
ln -s /usr/local/lib/libcyusb.so.1 /usr/local/lib/libcyusb.so

cp $CURDIR/configs/cy_renumerate.sh /usr/local/bin
chmod 777 /usr/local/bin/cy_renumerate.sh

# Set the CYUSB_ROOT variable to point to the current directory.
echo "" >> /etc/profile
echo "# Cypress USB Suite: Root directory" >> /etc/profile
echo "export CYUSB_ROOT=$CURDIR" >> /etc/profile
echo "" >> /etc/profile

# Compile and install the cyusb_linux application.
cd $CURDIR/gui_src/
qmake-qt4 
make clean
make
cp $CURDIR/bin/cyusb_linux /usr/local/bin
