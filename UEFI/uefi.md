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
```bash
cd UEFI/edk2
ln -sf ../QualgoChainPkg
build -p QualgoChainPkg/QualgoChainPkg.dsc -a AARCH64 -t GCC5 -b RELEASE
```

# Create esp partition
```bash
dd if=/dev/zero of=fatimg.img bs=1M count=100
mkfs.vfat -F 32 fatimg.img
mkdir -p /mnt/fatimg
sudo mount -o loop fatimg.img /mnt/fatimg
mkdir -p /mnt/fatimg/EFI/BOOT
cp Build/QualgoChainPkg/RELEASE_GCC5/AARCH64/*.efi /mnt/fatimg/EFI/BOOT
sudo python3 aes256gcm_encrypt.py /mnt/fatimg/EFI/BOOT/Uefi2.efi /mnt/fatimg/EFI/BOOT/secondLoader.enc
sudo umount /mnt/fatimg
```

# Run Qemu
```bash
qemu-system-aarch64 -M virt -cpu cortex-a57 -m 1024 \
    -bios /home/hao-nna/onboard-markdown/UEFI/edk2/Build/ArmVirtQemu-AArch64/RELEASE_GCC5/FV/QEMU_EFI.fd \
    -drive file=fatimg.img,if=none,id=drive0 -device virtio-blk-device,drive=drive0 -nographic
```

# Test yocto on QEMU
```bash
qemu-system-aarch64 -M virt -cpu cortex-a57 -m 4096 \
    -bios /home/hao-nna/onboard-markdown/UEFI/edk2/Build/ArmVirtQemu-AArch64/RELEASE_GCC5/FV/QEMU_EFI.fd \
    -drive file=tegra-espimage-jetson-orin-nano-devkit-nvme.esp,format=raw,if=none,id=drive0 \
    -device virtio-blk-device,drive=drive0 \
    -drive file=/media/sshfs/core-image-full-cmdline-jetson-orin-nano-devkit-nvme.rootfs.ext4,format=raw,if=virtio -nographic
```

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

### How to check secureboot is enabled or not via command line

```bash
$ efivar -n 8be4df61-93ca-11d2-aa0d-00e098032b8c-SecureBoot
```
```
Value:
00000000  01
```
+ 00: not enabled
+ 01: enabled


## EDK2 Build intructions
NVIDIA documents two build flows (with and without Docker). In our experience, the official NVIDIA Docker image is not stable on Ubuntu 24.04, so we build using our own Ubuntu 22.04-based Docker image that follows the “no-docker” toolchain requirements from NVIDIA.
A reference Dockerfile is attached.

1) Build the Docker image
```bash
docker build -t ubuntu22-edk2-nvidia:latest -f Dockerfile .
```

2) Configure edk2_docker for convenient usage
Add the following to your shell config (e.g. ~/.bashrc).
This mounts your current working directory into the container at /work and persists edkrepo config in ~/.edkrepo.

``` bash
export EDK2_DEV_IMAGE="ubuntu22-edk2-nvidia:latest"
export EDK2_BUILD_ROOT="$PWD"
export EDK2_BUILDROOT_ARGS="-v ${EDK2_BUILD_ROOT}:${EDK2_BUILD_ROOT}"
export EDK2_USER_ARGS="-v ${HOME}:${HOME} \
-e EDK2_DOCKER_USER_HOME=${HOME} \
-v ${HOME}/.edkrepo:/home/edk2/.edkrepo"
alias edk2_docker='docker run --rm -it \
-v "$PWD:/work" -w /work \
${EDK2_BUILDROOT_ARGS} ${EDK2_USER_ARGS} ${EDK2_BUILD_ARGS} \
"$EDK2_DEV_IMAGE"'
```

3) Configure edkrepo to use NVIDIA’s manifest repository

For JetPack 6, we use the tag r36.4.5-updates:
```
edk2_docker edkrepo manifest-repos add nvidia https://github.com/NVIDIA/edk2-edkrepo-manifest.git r36.4.5-updates nvidia
```

4) Build Jetson UEFI
```bash
cd nvidia-uefi
edk2_docker edk2-nvidia/Platform/NVIDIA/Jetson/build.sh --target RELEASE --init-defconfig edk2-nvidia/Platform/NVIDIA/Jetson/Jetson.defconfig
```

5) Output artifacts
Build outputs are generated under the `images/` directory.

6) NVIDIA artifact naming and deployment notes
+ BOOTAA64_Jetson_RELEASE.efi --> bootaa64.efi
+ uefi_Jetson_RELEASE.bin --> uefi_jetson.bin # Using DEBUG if want more log
+ For the efi application we can copy directly to target without reflash.
+ Applying uefi_jetson.bin (reflash required)

To use a newly built uefi_jetson.bin, copy it into the flashing package folder (for example, extracted from tegraflash.tar.gz) and run your normal flash flow.
If you want to skip rootfs write steps during flashing (e.g., when only updating UEFI), you can comment out the rootfs write sections in `initrd-flash
Comment the rootfs write block:

    # if [ $EXTERNAL_ROOTFS_DRIVE -eq 1 ]; then
    #     keep_going=1
    #     step_banner "Writing partitions on external storage device"
    #     if ! write_to_device $ROOTFS_DEVICE external-flash.xml.in 2>&1 | tee -a "$logfile"; then
    # 	echo "ERR: write failure to external storage at $(date -Is)" | tee -a "$logfile"
    # 	if [ $early_final_status -eq 0 ]; then
    # 	    exit 1
    # 	fi
    #     fi
    # else
    #     step_banner "Writing partitions on internal storage device"
    #     if ! write_to_device $ROOTFS_DEVICE flash.xml.in 2>&1 | tee -a "$logfile"; then
    # 	echo "ERR: write failure to internal storage at $(date -Is)" | tee -a "$logfile"
    # 	if [ $early_final_status -eq 0 ]; then
    # 	    exit 1
    # 	fi
    #     fi
    # fi

In generate_flash_package(), comment out command sequence additions related to wiping/exporting devices:

    # echo "extra-pre-wipe" >> "$mnt/flashpkg/conf/command_sequence"

    # if [ $erase_nvme -eq 1 ]; then
    # echo "erase-nvme" >> "$mnt/flashpkg/conf/command_sequence"
    # fi
    # [ $EXTERNAL_ROOTFS_DRIVE -eq 0 -o $NO_INTERNAL_STORAGE -eq 1 ] || echo "erase-mmc" >> "$mnt/flashpkg/conf/command_sequence"
    # echo "export-devices $ROOTFS_DEVICE" >> "$mnt/flashpkg/conf/command_sequence"
