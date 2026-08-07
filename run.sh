#!/bin/sh
# Run LunariaOS in QEMU. Usage: ./run.sh [disk.img]
IMG="${1:-disk.img}"
exec qemu-system-x86_64 -no-reboot -drive format=raw,file="$IMG"
