#!/bin/sh

pid=`pidof cyusb`

if [ "$pid" ]; then
    /usr/bin/kill -s SIGUSR1 $pid
fi

