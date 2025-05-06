sinclude(../etc/ackernel.m4)dnl

dnl
dnl Check for diva driver in kernel
dnl

AC_DEFUN(AC_CHECK_DIVAS, [
        OLD_CPPFLAGS="$CPPFLAGS"
        CPPFLAGS="-nostdinc -I${CONFIG_KERNELDIR} -I/usr/include"
        have_divas="no"
        AC_MSG_CHECKING([for DIVAS driver in ${CONFIG_KERNELDIR}/drivers/isdn/eicon])
        AC_TRY_COMPILE([#include <linux/types.h>
			#include <drivers/isdn/eicon/xdi_msg.h>],int x = DIVA_XDI_IO_CMD_WRITE_MSG;,have_divas="yes",)
        AC_MSG_RESULT("${have_divas}")
        CPPFLAGS="$OLD_CPPFLAGS"
        if test "$have_divas" != "no" ; then
                AC_DEFINE(HAVE_DIVAS)
        fi
        AC_SUBST(HAVE_DIVAS)
])


