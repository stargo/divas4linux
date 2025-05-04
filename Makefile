#
# Makefile for divas4linux-melware
#

#default kernel source $KDIR
KERNELDIR=$(shell uname -r)
KDIR=/lib/modules/$(KERNELDIR)/build

#for installation in another directory
DESTDIR=

EICONDIR=/usr/lib/divas

.PHONY: all clean tty_module kernel

all: kernel/divas.ko tty_module/Divatty.ko divactrl/divactrl
	@echo
	@echo "Build of divas4linux-melware complete."
	@echo
	@echo "You may run 'make install' now."
	@echo

tty_module/Divatty.ko: $(KDIR)/.config tty_module

tty_module: $(KDIR)/.config kernel
	@echo "Building tty kernel module using kernel from $(KDIR) ..."
	@echo
	@rm -f tty_module/divainclude
	@ln -s `pwd`/kernel tty_module/divainclude
	@make DIVA_CFLAGS="-I`pwd`/tty_module -I`pwd`/kernel" -C $(KDIR) M=`pwd`/tty_module KBUILD_EXTRA_SYMBOLS="`pwd`/kernel/Module.symvers" modules -j $(nproc)
	@echo

kernel/divas.ko: $(KDIR)/.config config.h kernel

kernel: $(KDIR)/.config config.h
	@echo "Building divas4linux kernel modules using kernel from $(KDIR) ..."
	@echo
	@make -C $(KDIR) M=`pwd`/kernel modules -j $(nproc)
	@echo

divactrl/divactrl:
	@echo
	@echo "Build divactrl utility ..."
	@(	\
	  cd divactrl;	\
	  rm -rf ./dlinux;		\
	  	mkdir -p ./dlinux/drivers/isdn/hardware;	\
	  	ln -s `pwd`/../kernel ./dlinux/drivers/isdn/hardware/eicon;	\
	  ./configure --prefix=/		\
	 	--with-firmware=$(EICONDIR)	\
		--with-kernel=`pwd`/dlinux		\
		|| exit 1;		\
	  make -j $(nproc) || exit 1;	\
	  rm -rf ./dlinux;		\
	)

$(KDIR)/.config:
	@echo "Unable to find configured kernel in $(KDIR)."
	@echo "Please provide the path to your kernel with KDIR=/path/to/kernelsources"
	@exit 1

config.h:
	@(	\
	  	echo ;	\
		if ! grep -q "CONFIG_ISDN_CAPI=" $(KDIR)/.config >/dev/null 2>&1; then \
			echo "This kernel doesn't seem to have CAPI support."; \
			echo "The only way is to choose Diva optimized CAPI."; \
	  		echo ;	\
		fi; \
	  	echo "Do you want to use the Diva optimized CAPI interface?";	\
	  	echo " Note: if you say 'y' here, the common kernelcapi is not used";	\
	  	echo " and therefore other CAPI cards than Diva are not usable.";	\
		echo " If you use Diva cards only, you can say 'y'.";	\
		echo -e "Your selection (y/n)[y]: \c";	\
		read optimized;	\
		if [ "$$optimized" = "n" ]; then	\
			echo "No, using common kernelcapi drivers.";	\
			cat kernel/platform.h | sed "s,.*DIVA_EICON_CAPI.*,#undef DIVA_EICON_CAPI,g" > config.h;	\
		else	\
			echo "Yes, using optimized Diva CAPI.";	\
			cat kernel/platform.h | sed "s,.*DIVA_EICON_CAPI.*,#define DIVA_EICON_CAPI 1,g" > config.h;	\
		fi;	\
	  	cp config.h kernel/platform.h;	\
	 )

install: all 
	@install -d $(DESTDIR)$(EICONDIR)
	@echo "Installing divactrl utility to $(DESTDIR)$(EICONDIR) ..."
	@install -m 0755 divactrl/divactrl $(DESTDIR)$(EICONDIR)/divactrl 
	@echo "Installing firmware to $(DESTDIR)$(EICONDIR) ..."
	@install -m 0644 firmware/* $(DESTDIR)$(EICONDIR)/.
	@echo "Installing scripts to $(DESTDIR)$(EICONDIR) ..."
	@if [ `id -u` -eq 0 ]; then chown 0.0 scripts/*; fi
	@cp -p scripts/* $(DESTDIR)$(EICONDIR)/.
	@echo "Installing diva modules to $(DESTDIR)$(EICONDIR) ..."
	@install -m 0644 kernel/*.ko $(DESTDIR)$(EICONDIR)/.
	@install -m 0644 tty_module/*.ko $(DESTDIR)$(EICONDIR)/.
	@echo
	@echo "Installation of divas4linux-melware complete."
	@echo
	@echo "See file README for description and how to use."
	@echo

clean:
	@rm -rf divactrl/dlinux
	@rm -f config.h
	@(cd divactrl; make distclean >/dev/null 2>&1; exit 0)
	@rm -f kernel/*.o
	@rm -f kernel/*.o.d
	@rm -f kernel/*.ko
	@rm -f kernel/*.mod.o
	@rm -f kernel/*.mod.c
	@rm -f kernel/.*.cmd
	@rm -f kernel/Module.symvers
	@rm -rf kernel/.tmp_versions
	@rm -f tty_module/*.o
	@rm -f tty_module/*.o.d
	@rm -f tty_module/*.ko
	@rm -f tty_module/*.mod.o
	@rm -f tty_module/*.mod.c
	@rm -f tty_module/.*.cmd
	@rm -f tty_module/Module.symvers
	@rm -rf tty_module/.tmp_versions
	@rm -f tty_module/divainclude
	@echo
	@echo "Cleaned."
	@echo

