# -----------------------------------------------------------------------------
# *
#  Copyright (c) Eicon Networks, 2000.
# *
#  This source file is supplied for the use with
#  Eicon Networks range of DIVA Server Adapters.
# *
#  Eicon File Revision :    1.9
# *
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2, or (at your option)
#  any later version.
# *
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY OF ANY KIND WHATSOEVER INCLUDING ANY
#  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#  See the GNU General Public License for more details.
# *
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
# *
# -----------------------------------------------------------------------------
#   DivaSTrace API makefile
# -----------------------------------------------------------------------------
INCLUDES=-Ilocalinc -Iinclude -I../kernel
C_DEFINES=-DLINUX
CFLAGS=-c -Wall -O2 $(C_DEFINES)
# ----------------------------------------------------------------------------
OBJ_DIR=src/obj
LIB_DIR=./lib
BIN_DIR=./bin
TARGET=$(LIB_DIR)/libDivaSTrace.a
# ----------------------------------------------------------------------------
LIB_SRC=src/idi.c src/lib_init.c src/lib_init_sip.c src/log_b.c
OS_LIB_SRC=src/linux/os.c
# ----------------------------------------------------------------------------
ALL: make_dirs $(TARGET) bin/tracer divalogd/bin/divalogd

make_dirs: $(OBJ_DIR)/ $(LIB_DIR)/ $(BIN_DIR)/

$(OBJ_DIR)/:
	@mkdir -p $(OBJ_DIR)

$(LIB_DIR)/:
	@mkdir -p $(LIB_DIR)

$(BIN_DIR)/:
	@mkdir -p $(BIN_DIR)
	@mkdir -p divalogd/bin

$(TARGET): $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(LIB_SRC)) \
           $(patsubst src/linux/%.c,$(OBJ_DIR)/%.o,$(OS_LIB_SRC))
	$(AR) r $@ $+

$(OBJ_DIR)/%.o: src/%.c
	$(CC) $(INCLUDES) $(CFLAGS) $< -o $@

$(OBJ_DIR)/%.o: src/linux/%.c
	$(CC) $(INCLUDES) $(CFLAGS) $< -o $@

divalogd/bin/divalogd: $(TARGET)
	cd divalogd && make -j 4

clean:
	@find . -name "*.o" -print -o -name "*.a" -print | xargs rm -f
	@rm -f bin/tracer $(OBJ_DIR)/.depend
	@cd divalogd && make clean

# ----------------------------------------------------------------------------
#  Create example application
# ----------------------------------------------------------------------------
bin/tracer: tracer/tracer.c localinc/platform.h include/st_ifc.h $(TARGET)
	$(CC) $(INCLUDES) tracer/tracer.c $(TARGET) -o $@

# ----------------------------------------------------------------------------
#  Create Dependenties
# ----------------------------------------------------------------------------
depend: $(OBJ_DIR)/
	$(CC) $(INCLUDES) $(CFLAGS) -M $(LIB_SRC) $(OS_LIB_SRC) | \
                     sed -e "s/^.*:/src\/obj\/&/" - > $(OBJ_DIR)/.depend

-include $(OBJ_DIR)/.depend

# ----------------------------------------------------------------------------

