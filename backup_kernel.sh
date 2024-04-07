#!/bin/bash

# Check if user is root
if [ "$(id -u)" -ne 0 ]; then
    echo "This script must be run as root" 1>&2
    exit 1
fi

# Define variables
KERNEL_VERSION=$(uname -r)
BACKUP_DIR="/boot/kernel_backups"
GRUB_CFG="/boot/grub/grub.cfg"

# Ensure backup directory exists
mkdir -p $BACKUP_DIR

# Backup current kernel
echo "Backing up current kernel..."
cp /boot/vmlinuz-$KERNEL_VERSION $BACKUP_DIR
cp /boot/initrd.img-$KERNEL_VERSION $BACKUP_DIR

# Update Grub configuration
echo "Updating Grub configuration..."
cat << EOF >> $GRUB_CFG
menuentry "Previous Linux Kernel ($KERNEL_VERSION)" {
    set root=(hd0,1) # Adjust this according to your system
    linux /boot/vmlinuz-$KERNEL_VERSION root=UUID=$(blkid -s UUID -o value /dev/sda1) ro quiet splash # Adjust root partition accordingly
    initrd /boot/initrd.img-$KERNEL_VERSION
}
EOF

# Update Grub
update-grub

echo "Backup and Grub configuration complete."