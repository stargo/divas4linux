# DIVAS4LINUX

Copyright 2006-2013 Cytronics & Melware (www.melware.net)

## About

This package is a repackaging of the source-level-RPM packages by
Dialogic based on the work by Cytronics & Melware.

Support for newer kernels (tested up to 6.12 (Debian trixie)) has
been added.

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

    sudo cp divas4linux.rules /etc/udes/rules.d/

## Card status

A tool is provided to show the current status of the installed DIVA Sever
cards:

    /usr/lib/divas/divas_status

## AT command set

The virtual ports /dev/ttyds* support the AT command set. For a list of
supported commands, take a look at [AT.txt](AT.txt)

## License

The license is GPL (read the file LICENSE), but some files
like config scripts and the firmware binaries are not GPL and
under the copyright of Eicon Networks / Dialogic.
The included dialog utility is GPL.
All files are for the exclusive use with Eicon DIVA ISDN adapters only.
