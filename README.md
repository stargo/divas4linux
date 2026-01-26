# DIVAS4LINUX

Copyright 2006-2013 Cytronics & Melware (www.melware.net)

## About

This package is a repackaging of the source-level-RPM packages by
Dialogic based on the work by Cytronics & Melware. It's currently
based on version 9.6LINSU8 (9.6-124-26) from Sangoma.

Support for newer kernels (tested up to 6.12 (Debian trixie), compiles
up to at least 6.17) has been added.

## Supported hardware

The following cards should be supported by this driver:

* BRI:
  * Diva BRI-CTI PCI v2
  * Diva BRI-2FX PCI v2
  * Diva BRI-2M PCI v1 (tested, 800-201-01)
  * Diva BRI-2M PCI v2 (tested, 803-007-01, 800-683-01)
  * Diva BRI-2M PCIe v2 (tested, 803-040-01)
  * Diva 4BRI-8M PCI v1
  * Diva 4BRI-8M PCI v2 (tested, 803-008-03)
  * Diva 4BRI-8M PCIe v2 (tested, 803-031-01, 50-0447-01E)
  * Diva V-4BRI-8M PCI v1[^1]
  * Diva UM-BRI-2 PCI v2[^2]
  * Diva UM-BRI-2 PCIe v2[^2] (tested, 813-087-01)
  * Diva UM-4BRI-8 PCI v2[^2]
  * Diva UM-4BRI-8 PCIe v2[^2]
* PRI:
  * Diva PRI/E1/T1-CTI PCI v3
  * Diva PRI/E1/T1-CTI PCIe v3
  * Diva PRI/E1/T1-8 PCI v3
  * Diva PRI/T1-24 PCI v3
  * Diva PRI/T1-24 PCIe v3
  * Diva PRI/E1-30 PCI v1
  * Diva PRI/E1-30 PCI v2
  * Diva PRI/E1-30 PCI v3 (tested, 800-810-01)
  * Diva PRI/E1-30 PCIe v3 (tested, 803-023-01)
  * Diva UM-PRI/T1-24 PCI v3[^2]
  * Diva UM-PRI/T1-24 PCIe v3[^2]
  * Diva UM-PRI/E1-30 PCI v3[^2]
  * Diva UM-PRI/E1-30 PCIe v3[^2]
  * Diva V-PRI/T1-24 PCI v3[^1]
  * Diva V-PRI/T1-24 PCIe v3[^1]
  * Diva V-PRI/E1-30 PCI v2[^1]
  * Diva V-PRI/E1-30 PCI v3[^1]
  * Diva V-PRI/E1-30 PCIe v3[^1] (tested, 813-061-01)
  * Diva V-2PRI/T1-48 PCI v1[^1]
  * Diva V-2PRI/E1-60 PCI v1[^1]
  * Diva V-4PRI/T1-96 PCI v1[^1]
  * Diva V-PRI/E1-120 PCI v1[^1]
  * Diva V-1PRI/E1/T1-30 PCIe HS v1[^1]
  * Diva V-2PRI/E1/T1-60 PCIe HS v1[^1]
  * Diva V-4PRI/E1/T1-120 PCIe HS v1[^1]
  * Diva V-4PRI/E1/T1-120 PCIe FS v1[^1]
  * Diva V-8PRI/E1/T1-240 PCIe FS v1[^1]
* Analog:
  * Diva Analog-2 PCI v1
  * Diva Analog-2 PCIe v1
  * Diva Analog-4 PCI v1
  * Diva Analog-4 PCIe v1
  * Diva Analog-8 PCI v1
  * Diva Analog-8 PCIe v1 (tested, 50-0424-01)
  * Diva UM-Analog-4 PCI v1[^2]
  * Diva UM-Analog-4 PCIe v1[^2]
  * Diva UM-Analog-8 PCI v1[^2]
  * Diva UM-Analog-8 PCIe v1[^2]

[^1]: Voice only card, no support for modem and fax
[^2]: Unified messaging card, full modem support, support for fax only on half of the available channels

## Installation

To compile the kernel modules and the divactrl utility just type

    make

and after successful compilation, use

    sudo make install

to install the files on your system.

Note:
--------------------
You can choose between the optimized Diva CAPI driver, or an own version
of common kernelcapi. (you will be asked when doing 'make')
The optimized Diva CAPI makes sense when using Diva cards with CAPI only.

The 'divactrl' tool and all other tools,
scripts and the firmware will be installed to

    /usr/lib/divas

The compilation needs a configured kernel source at

    /lib/modules/`uname -r`/build

If your kernel resides in another location, you need to specify
this location with KDIR= , e.g.:

    make KDIR=/path/to/kernelsrc
    make KDIR=/path/to/kernelsrc install

In case you want to install into a special destination directory, use

    make KDIR=/path/to/kernelsrc DESTDIR=/targetdir install

to install all files to /targetdir/usr/lib/divas
Note: this is for e.g. cross-installation only. The path
/usr/lib/divas for running and using the installation must be used.

## Configuration

After successful installation, use

    /usr/lib/divas/Config

to create the configuration file for your Eicon DIVA Server cards.
This configuration is dialog based.

After saving your configuration, the script

    /usr/lib/divas/divas_cfg.rc

will be created.
Run this script to start the Eicon DIVA Server cards on your system.
This can be done manually or by some init scripts at boot time, but
it must be run as root.

What is this script doing?
Well, it loads the diva kernel modules, configures the cards and loads the
firmware.

To stop the cards and unload the diva modules, run the script

    /usr/lib/divas/divas_stop.rc

## Systemd integration

A systemd unit is provided for automatic startup of the cards.
To install it, use

    sudo cp divas4linux.service /etc/systemd/system/
    sudo systemctl daemon-reload
    sudo systemctl enable divas4linux

## udev rules for tty devices /dev/ttyds*

udev rules are provided for managing the permissions of the
tty devices created by Divatty. To install them, use

    sudo cp divas4linux.rules /etc/udev/rules.d/

## Card status

A tool is provided to show the current status of the installed DIVA Sever
cards:

    /usr/lib/divas/divas_status

You might need to install the perl module `Text::Table` for this to work,
on debian it is packaged as `libtext-table-perl`.

## AT command set

The virtual ports /dev/ttyds* support the AT command set. For a list of
supported commands, take a look at [AT.txt](AT.txt)

## License

The license is GPL (read the file LICENSE), but some files
like config scripts and the firmware binaries are not GPL and
under the copyright of Eicon Networks / Dialogic.
The included dialog utility is GPL.
All files are for the exclusive use with Eicon DIVA ISDN adapters only.
