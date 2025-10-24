# configuration part of the Makefile

# gpm mouse support
HAVE_GPM_SUPPORT=false
# ncurses or just normal curses?
HAVE_NCURSES=true
# support of a configuration file
HAVE_RC_FILE=false
# can be disabled to make dialog a bit smaller (could be done for more things)
HAVE_GAUGE=true

BINDIR = /usr/bin
MANDIR = /usr/man/man1/

CC = gcc
CPP = gcc -E
OPTIM = -O3 -fno-strength-reduce -Wall -Wstrict-prototypes

# end of the configuration part
#----------------------------------------------------------------------------
# do not edit below this line

CFLAGS = $(OPTIM) -DLOCALE
LDFLAGS = -L . -Wl,-warn-common
LDLIBS = -L. -ldialog

OBJS = checklist.o inputbox.o menubox.o msgbox.o \
	 textbox.o util.o yesno.o

ifeq ($(HAVE_GPM_SUPPORT), true)
LDLIBS+=-lgpm
CFLAGS+=-DHAVE_LIBGPM
OBJS+=mouse.o
endif
ifeq ($(HAVE_NCURSES), true)
CFLAGS+=-DHAVE_NCURSES -I/usr/include/ncurses
LDLIBS+=-lncurses
endif
ifeq ($(HAVE_RC_FILE), true)
CFLAGS+=-DHAVE_RC_FILE
OBJS+=rc.o
endif
ifeq ($(HAVE_GAUGE), true)
OBJS+=gauge.o
CFLAGS+=-DHAVE_GAUGE
endif

SRCS = $(OBJS:.o=.c)

all: libdialog.a dialog

libdialog.a: $(OBJS)
	ar rcs libdialog.a $(OBJS)

dialog: dialog.o

clean:
	rm -f core *.o *~ dialog .depend libdialog.a

install: dialog libdialog.a
	install -s dialog $(BINDIR)
	install -m 644 dialog.1 $(MANDIR)
#	install -m 644 libdialog.a /usr/lib/

depend:
	$(CPP) -M $(CFLAGS) $(SRCS) > .depend

-include .depend

