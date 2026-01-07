# Kas demo

Using to demonstrate various kas build

## Setup

```
pip3 install kas # or
pipx install kas
```

## Demo Projects

### SWUpdate for Nvidia Tegra Orin Nano Devkit

kas build --update kas/jetson-orin-nano-devkit-nvme.yml

### Output Artifacts
Once the build completes, all deployable files are located at: build/tmp/deploy/images/jetson-orin-nano-devkit-nvme/
+ demo-image-base-jetson-orin-nano-devkit-nvme.tegraflash.tar.gz (For flashing)
+ demo-image-base-jetson-orin-nano-devkit-nvme.swu (For SWUpdate)

## TODO: add Beaglebone green, Raspberry, ....
