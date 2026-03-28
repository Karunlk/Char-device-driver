#!/bin/bash
set -e

MODULE="b240213cs_A1"
KEY="MOK.key"
CRT="MOK.crt"
HEADERS="/usr/src/linux-headers-$(uname -r)"

echo "==> Removing old module if loaded..."
sudo rmmod $MODULE 2>/dev/null || true
sudo rm -f /dev/mydev

echo "==> Building..."
make clean && make

echo "==> Signing..."
sudo $HEADERS/scripts/sign-file sha256 $KEY $CRT ${MODULE}.ko

echo "==> Inserting module..."
sudo insmod ${MODULE}.ko kernel_version=6,12 timer_val=60

echo "==> Getting major number..."
sleep 1
MAJOR=$(sudo dmesg | grep "major=" | tail -1 | grep -o 'major=[0-9]*' | cut -d= -f2)
echo "    major = $MAJOR"

echo "==> Creating device node..."
sudo mknod /dev/mydev c $MAJOR 0
sudo chmod 666 /dev/mydev

echo "==> Performing READ..."
cat /dev/mydev

echo "==> Performing WRITE (username: $(whoami))..."
echo $(whoami) > /dev/mydev

echo "==> Removing module..."
sudo rmmod $MODULE

echo "==> Result:"
sudo dmesg | tail -8

echo "==> Cleaning generated files..."
make clean
