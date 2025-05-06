ifdef CONFIG_SMP
	EXTRA_CFLAGS += -DDIVA_SMP=1
endif
EXTRA_CFLAGS += $(DIVA_CFLAGS) -DDIVA_BUILD=\"124-45\" -DDIVA_GPL=1 -DUNIX -DLINUX -D__KERNEL_VERSION_GT_2_4__=1

obj-m := Divatty.o
Divatty-y := ser_lnx.o atp.o port.o isdn.o fax.o fax1.o fax2.o queue.o ppp.o rna.o btx.o diva_mem.o ser_dbg.o dlist.o dpc.o hdlc.o v110.o piafs.o manage1.o drv_man1.o tty_man.o fax_stat.o
