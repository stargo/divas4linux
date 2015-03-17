              DIVAS4LINUX
              ===========

Copyright 2006-2013 Cytronics & Melware (www.melware.net)

This package is repackaged by Cytronics & Melware based
on the source-level-RPM packages by Dialogic.

The license is GPL (read the file LICENSE), but some files
like config scripts and the firmware binaries are not GPL and
under the copyright of Eicon Networks / Dialogic.
All files are for the exclusive use with Eicon DIVA ISDN adapters only.

Eicon DIVA Server PCI cards are supported by this package for the
use with linux kernels >= 2.6.
However, there is no guarantee that every kernelversion will work.

For compilation and installation, see file INSTALL.

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



