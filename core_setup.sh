#!/bin/bash
[ "$UID" -eq 0 ] || exec sudo -E bash "$0" "$@"
modprobe libcomposite
modprobe usb_f_fs
cd /sys/kernel/config/usb_gadget
#if [ -d "/sys/kernel/config/usb_gadget/fakejoycon" ]
#then
#    echo "Already setup"
#    exit 0
#fi
mkdir -p fakejoycon; cd fakejoycon
echo "0x20d6" > idVendor  # put actual vendor ID here
echo "0xa711" > idProduct # put actual product ID here
echo "0x0200" > bcdDevice # put actual product ID here
echo "0x0200" > bcdUSB # put actual product ID here
echo "0" > bDeviceClass
echo "0" > bDeviceSubClass
echo "0" > bDeviceProtocol
echo "0x40" > bMaxPacketSize0

mkdir -p configs/c.1
echo "500" configs/c.1/MaxPower
mkdir -p configs/c.1/strings/0x409
echo "fakejoycon" > configs/c.1/strings/0x409/configuration
mkdir -p strings/0x409
echo "Bensussen Deutsch & Associates,Inc.(BDA)" > strings/0x409/manufacturer
echo "Core (Plus) Wired Controller" > strings/0x409/product
echo "000000000001" > strings/0x409/serialnumber
mkdir -p functions/ffs.hid
ln -s functions/ffs.hid configs/c.1/
mkdir -p /tmp/mount_point
mount hid -t functionfs /tmp/mount_point
