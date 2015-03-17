#! /bin/bash

# ------------------------------------------------------------------------------
#
#  PRODUCT: divas4linux
#  FILE   : divas_sys_info.sh
#
#  Used to write system information in file report.txt
#
#  Copyright 1991-2009 by Dialogic(R) Corporation
#
# ------------------------------------------------------------------------------

if [ -f /usr/lib/divas/divadidd.ko ]
then
diva_dir=/usr/lib/divas
else

if [ -f /usr/lib/divas/divadidd.o ]
then
diva_dir=/usr/lib/divas
else
diva_dir=/usr/lib/isdn/eicon
fi

fi

${diva_dir}/Support
exit $?

