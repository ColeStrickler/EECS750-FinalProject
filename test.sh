#!/bin/bash

# Check if script is run as root
if [ "$EUID" -ne 0 ]
then
  echo "This script must be run as root" 
  exit 1
fi


ulimit -n 1000000
make clean
make driver
make remove
make install

for ((i =  1; i < 100; i += 1)); do
    ./io_driver_com 100 0
done
