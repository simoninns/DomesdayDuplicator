#!/bin/bash
pid=`pidof cyusb_linux`

if [ "$pid" ]; then
    kill -s SIGUSR1 $pid
fi

