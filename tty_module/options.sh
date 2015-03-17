KSRC="/usr/src/linux"

if [ $(($#)) -lt 1 ]
then
	exit 1
fi

cp $KSRC/Makefile conftest.make
echo -e "conftest.CC:" >>conftest.make
echo -e "\t@echo \$(CC)" >>conftest.make
echo -e "conftest.CFLAGS:" >>conftest.make
echo -e "\t@echo \$(CFLAGS) \$(MODFLAGS)" >>conftest.make
echo -e "conftest.LD:" >>conftest.make
echo -e "\t@echo \$(LD) \$(LDLAGS)" >>conftest.make
echo -e "conftest.PATCHLEVEL:" >>conftest.make
echo -e "\t@echo \$(PATCHLEVEL)" >>conftest.make
echo -e "conftest.SUBLEVEL:" >>conftest.make
echo -e "\t@echo \$(SUBLEVEL)" >>conftest.make
echo -e "conftest.CONFIG_4KSTACKS:" >>conftest.make
echo -e "\t@echo \$(CONFIG_4KSTACKS)" >>conftest.make
echo -e "conftest.CONFIG_PREEMPT:" >>conftest.make
echo -e "\t@echo \$(CONFIG_PREEMPT)" >>conftest.make
echo -e "conftest.CONFIG_MODULES:" >>conftest.make
echo -e "\t@echo \$(CONFIG_MODULES)" >>conftest.make

here=`pwd`
if [ $(($1)) -eq 1 ]
then
  NKCC=`cd $KSRC; make --no-print-directory -f $here/conftest.make conftest.CC`
  echo $NKCC
fi
if [ $(($1)) -eq 2 ]
then
  patchlevel=`cd $KSRC; make --no-print-directory -f $here/conftest.make conftest.PATCHLEVEL`
  sublevel=`cd $KSRC; make --no-print-directory -f $here/conftest.make conftest.SUBLEVEL`
	if [ $((patchlevel)) -gt $((5)) -a $((sublevel)) -gt $((17)) ]
	then
		mkdir test > /dev/null 2>&1

		echo '#include <linux/version.h>' >  test/test.c
		echo '#include <linux/module.h>'  >> test/test.c
		echo 'int __init  test_init(void) { return (0); }' >> test/test.c
		echo 'void __exit test_exit(void) { }'             >> test/test.c
		echo 'module_init(test_init)'    >> test/test.c
		echo 'module_exit(test_exit)'    >> test/test.c

		echo "obj-m := test.o" > test/Makefile
		command=$(cd $KSRC; make --no-print-directory modules SUBDIRS=${here}/test V=1 | grep " \-c " | grep "test\.c" | sed -e "s/gcc//" - | sed -e "s/-I *include/-I\/usr\/src\/linux\/include/g;" - | sed -e "s/-o .*$//" -)
		command="-I/usr/src/linux/drivers/isdn/hardware/eicon $command"
    grep "^ *#define *CONFIG_SMP" $KSRC/include/linux/autoconf.h > /dev/null 2>&1
    if [ $(($?)) -eq 0 ]
    then
    	command="$command -DDIVA_SMP=1"
    fi
		command=$(echo $command | sed -e "s/ -c / /" -)
		command=$(echo $command | sed -e "s/-include include\/linux\/autoconf.h/-include \/usr\/src\/linux\/include\/linux\/autoconf.h/" -)
		command=$(echo $command | sed -e "s/-D\"KBUILD_STR(s)=#s\"//" -)
		command=$(echo $command | sed -e "s/-D\"KBUILD_BASENAME=KBUILD_STR(test)\"//" -)
		command=$(echo $command | sed -e "s/-D\"KBUILD_MODNAME=KBUILD_STR(test)\"//" -)
		echo -D__KERNEL_VERSION_GT_2_4__=1 $command
	else
    NKCFLAGS=`cd $KSRC; make --no-print-directory -f $here/conftest.make conftest.CFLAGS`
    grep "^ *#define *CONFIG_SMP" $KSRC/include/linux/autoconf.h > /dev/null 2>&1
    if [ $(($?)) -eq 0 ]
    then
    	NKCFLAGS="$NKCFLAGS -DDIVA_SMP=1"
    fi


  	if [ $((patchlevel)) -le $((4)) ]
  	then
  		echo -I /usr/src/linux/drivers/isdn/eicon $NKCFLAGS
  	else
  		echo $NKCFLAGS | grep __KERNEL__  > /dev/null 2>&1
  		if [ $(($?)) -ne 0 ]
  		then
  			NKCFLAGS="-D__KERNEL__ $NKCFLAGS"
  		fi
  		echo -nostdinc -iwithprefix include -D__KERNEL_VERSION_GT_2_4__=1 -I/usr/src/linux/drivers/isdn/hardware/eicon $NKCFLAGS | sed -e "s/-I *include/-I\/usr\/src\/linux\/include/g;" -
    fi
	fi
fi
if [ $(($1)) -eq 3 ]
then
  NKCC=`cd $KSRC; make --no-print-directory -f $here/conftest.make conftest.LD`
  echo $NKCC
fi

if [ $(($1)) -eq 4 ]
then
  patchlevel=`cd $KSRC; make --no-print-directory -f $here/conftest.make conftest.PATCHLEVEL`
	if [ $((patchlevel)) -le $((4)) ]
	then
		echo "o"
	else
		echo "ko"
	fi
fi

if [ $(($1)) -eq 5 ]
then
  CONFIG_4KSTACKS=`cd $KSRC; make --no-print-directory -f $here/conftest.make conftest.CONFIG_4KSTACKS`
  if [ "$CONFIG_4KSTACKS" == "y" ]; then echo "1"; else echo "0"; fi
fi

if [ $(($1)) -eq 6 ]
then
  CONFIG_PREEMPT=`cd $KSRC; make --no-print-directory -f $here/conftest.make conftest.CONFIG_PREEMPT`
  if [ "$CONFIG_PREEMPT" == "y" ]; then echo "1"; else echo "0"; fi
fi

if [ $(($1)) -eq 7 ]
then
  CONFIG_MODULES=`cd $KSRC; make --no-print-directory -f $here/conftest.make conftest.CONFIG_MODULES`
  if [ "$CONFIG_MODULES" == "y" ]; then echo "1"; else echo "0"; fi
fi

if [ $(($1)) -eq 8 ]
then
	grep "KBUILD_BASENAME=KBUILD_STR" $KSRC/scripts/Makefile.lib > /dev/null 2>&1
	if [ $(($?)) -ne $((0)) ]
	then
		echo "1"
	else
		echo "2"
	fi
fi

rm -f conftest.make
rm -rf test/*

exit 0
