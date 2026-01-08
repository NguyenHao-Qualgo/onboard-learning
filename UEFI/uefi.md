## Requirements and Implementations outlined here: https://aottws.atlassian.net/wiki/x/PADBQw

### Sum up steps to build and run
#### Get EDK2 sources (with submodules like OpenSSL)
git submodule update --init --recursive 
cd edk2

# Build BaseTools and set the EDK2 build env
make -C BaseTools
. edksetup.sh

# Cross toolchain prefix for GCC
export GCC5_AARCH64_PREFIX=aarch64-linux-gnu-

# Build
build -a AARCH64 -t GCC5 -b RELEASE -D SECURE_BOOT_ENABLE=TRUE \
    -D INCLUDE_SHELL=TRUE -p ArmVirtPkg/ArmVirtQemu.dsc

# Build Qualgo application chains
cd UEFI/edk2
ln -sf ../QualgoChainPkg
build -p QualgoChainPkg/QualgoChainPkg.dsc -a AARCH64 -t GCC5 -b RELEASE

# Create esp partition
dd if=/dev/zero of=fatimg.img bs=1M count=100
mkfs.vfat -F 32 fatimg.img
mkdir -p /mnt/fatimg
sudo mount -o loop fatimg.img /mnt/fatimg
mkdir -p /mnt/fatimg/EFI/BOOT
cp Build/QualgoChainPkg/RELEASE_GCC5/AARCH64/*.efi /mnt/fatimg/EFI/BOOT
sudo python3 aes256gcm_encrypt.py /mnt/fatimg/EFI/BOOT/Uefi2.efi /mnt/fatimg/EFI/BOOT/secondLoader.enc
sudo umount /mnt/fatimg

# Run Qemu
qemu-system-aarch64 -M virt -cpu cortex-a57 -m 1024     -bios /home/hao-nna/onboard-markdown/UEFI/edk2/Build/ArmVirtQemu-AArch64/RELEASE_GCC5/FV/QEMU_EFI.fd     -drive file=fatimg.img,if=none,id=drive0     -device virtio-blk-device,drive=drive0     -nographic

# Test yocto on QEMU
qemu-system-aarch64 -M virt -cpu cortex-a57 -m 4096     -bios /home/hao-nna/onboard-markdown/UEFI/edk2/Build/ArmVirtQemu-AArch64/RELEASE_GCC5/FV/QEMU_EFI.fd     -drive file=tegra-espimage-jetson-orin-nano-devkit-nvme.esp,format=raw,if=none,id=drive0     -device virtio-blk-device,drive=drive0     -drive file=/media/sshfs/core-image-full-cmdline-jetson-orin-nano-devkit-nvme.rootfs.ext4,format=raw,if=virtio     -nographic

## Boot flows
firstloader.efi --> secondLoader.enc --> kernel/rootfs --> boot

Jan 8
firstloader.efi --> secondloader.enc --> using enrolled keys to verify kernel/rootfs signature --> boot : DONE

Put esp and rootfs into a single device

## A way to enroll keys?
Manual enroll via Autoenroll.efi --> DONE
Using traditional NVIDIA method --> DONE

### Our way:
We apply a patch to make bootaa64.efi invoke AutoEnroll.efi during the boot flow. AutoEnroll.efi then enrolls PK, KEK, and db from a keys directory on the NVMe rootfs, and finally hands off execution to firstLoader.efi as the primary application.

### NVIDIA traditional way:
Leverage NVIDIA traditional approach: generate a DTBO that embeds PK/KEK/db (and optional dbx) ESL blobs and sets EnrollDefaultSecurityKeys so NVIDIA UEFI auto-enrolls these Secure Boot variables into NVRAM at boot.
Set firstLoader as main application --> rename to bootaa64.efi by define EFI_PROVIDER = "edk2-bootloader-firmware" in local.conf
and rename efi applications

See: [Kas Scripts](../kas/README.md#scripts)