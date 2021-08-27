#!/bin/bash
[ "$UID" -eq 0 ] || exec sudo -E bash "$0" "$@"
ls /sys/class/udc > /sys/kernel/config/usb_gadget/fakejoycon/UDC
