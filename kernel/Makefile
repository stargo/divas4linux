# Makefile for the Dialogic DIVA ISDN drivers.

CONFIG_CAPI_EICON=y
CONFIG_ISDN_DIVAS=m
CONFIG_ISDN_DIVAS_BRIPCI=y
CONFIG_ISDN_DIVAS_PRIPCI=y
CONFIG_ISDN_DIVAS_ANALOG=y
CONFIG_ISDN_DIVAS_DIVACAPI=m
CONFIG_ISDN_DIVAS_USERIDI=m
CONFIG_ISDN_DIVAS_MAINT=m
CONFIG_ISDN_DIVAS_HSI=m

NOSTDINC_FLAGS += -I$(M)

# Each configuration option enables a list of files.

obj-$(CONFIG_ISDN_DIVAS)		+= divadidd.o divas.o
obj-$(CONFIG_ISDN_DIVAS_MAINT)		+= diva_mnt.o
obj-$(CONFIG_ISDN_DIVAS_USERIDI)	+= diva_idi.o
obj-$(CONFIG_ISDN_DIVAS_DIVACAPI)	+= divacapi.o
obj-$(CONFIG_ISDN_DIVAS_HSI)      += diva_hsi.o
obj-$(CONFIG_ISDN_DIVAS_DIVA2I4L)	+= diva2i4l.o

# Multipart objects. 

divas-y					:= divasmain.o divasfunc.o di.o io.o \
					   diva.o divasproc.o diva_dma.o divatest.o vidi_di.o dlist.o \
					   fpga_rom.o fpga_rom_xdi.o
divas-$(CONFIG_ISDN_DIVAS_BRIPCI)	+= os_bri.o  s_bri.o os_4bri.o s_4bri.o
divas-$(CONFIG_ISDN_DIVAS_PRIPCI)	+= os_pri.o  s_pri.o os_pri3.o s_pri3.o os_4pri.o s_4pri.o s_4prie.o os_4prie.o
divas-$(CONFIG_ISDN_DIVAS_ANALOG)	+= os_analog.o  s_analog.o

divacapi-y				:= capimain.o capifunc.o message.o capidtmf.o manage.o drv_man.o capi_man.o dlist.o

divadidd-y				:= diva_didd.o diddfunc.o dadapter.o parser.o cfglib.o runtime.o dlist.o diva_cfg_lib.o

diva_mnt-y				:= divamnt.o mntfunc.o debug.o maintidi.o

diva_idi-y				:= divasi.o idifunc.o um_idi.o dqueue.o dlist.o

diva_hsi-y				:= divahsi.o dlist.o

diva2i4l-y				:= i4l_idi.o i4lididrv.o

