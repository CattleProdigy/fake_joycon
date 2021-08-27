#!/bin/bash
[ "$UID" -eq 0 ] || exec sudo -E bash "$0" "$@"
echo "" > /sys/kernel/config/usb_gadget/fakejoycon/UDC
