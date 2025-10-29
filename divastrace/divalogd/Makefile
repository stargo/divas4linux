CFG_BOARDCONF=y
#----------------------------------------------------------------------

INCLUDES= -I../localinc -I. -I../include -I ../CfgLib/include
OBJ_DIR=obj
BIN_DIR=bin
TARGET=$(BIN_DIR)/divalogd
ifeq ($(CFG_BOARDCONF),y)
CFLAGS=-O2 -Wall -DLINUX=1 -DBOARDCONF=1
else
CFLAGS=-O2 -Wall -DLINUX=1
endif

VPATH=../CfgLib/src
#VPATH+=:../.
#VPATH+=:.


#----------------------------------------------------------------------
LIBS=../lib/libDivaSTrace.a


#----------------------------------------------------------------------

ALL: make_dirs $(TARGET)

#----------------------------------------------------------------------
ifeq ($(CFG_BOARDCONF),y)
 SRC= event.c \
  divalog.c \
  record.c \
  dlist.c \
   um_cfg_lib.c \
   um_cfg_lib_module.c \
   diva_cfg_lib.c \
   diva_malloc.c \
   debug.c \
   divatimer.c
 else
SRC=event.c \
 divalog.c \
 record.c \
 dlist.c
 endif

#----------------------------------------------------------------------
make_dirs: $(OBJ_DIR)/ $(BIN_DIR)/

$(OBJ_DIR)/:
	@mkdir -p $(OBJ_DIR)

$(BIN_DIR)/:
	@mkdir -p $(BIN_DIR)

#----------------------------------------------------------------------

$(OBJ_DIR)/%.o: %.c
	$(CC) -c $(CFLAGS) $(INCLUDES) $< -o $@

#----------------------------------------------------------------------

$(TARGET): $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRC))
	$(CC) -static $^ $(LIBS) -o $@

#--- build version header file ----------------------------------------------#
PRIMARY=CfgLib interface
BUILD=0
REVISION=0
BUILDVERSION=1.0.1.0

do_version:
#{
	@rm -f ../CfgLib/include/BuildVer.h 
	@echo <<../CfgLib/include/BuildVer.h
	
	touch ../CfgLib/include/BuildVer.h
	echo "#define PRIMARY	\"$(PRIMARY)\" " 						>../CfgLib/include/BuildVer.h 
	echo "#define BUILD		\"$(BUILD)\" " 							>>../CfgLib/include/BuildVer.h
	echo "#define REVISION	\"$(REVISION)\" " 						>>../CfgLib/include/BuildVer.h
	echo "#define COPYRIGHT	\"(C) Dialogic Corporation 1991-2008\" " >>../CfgLib/include/BuildVer.h
	echo "#define BUILDVERSIONSTR \"$(BUILDVERSION)\" " 			>>../CfgLib/include/BuildVer.h
#}
#----------------------------------------------------------------------

depend:
	@$(CC) $(INCLUDES) $(CFLAGS) -M $(SRC) | \
          sed -e "s/^.*:/$(OBJ_DIR)\/&/" - > $(OBJ_DIR)/.depend

-include $(OBJ_DIR)/.depend

#----------------------------------------------------------------------

clean:
	@rm -f $(OBJ_DIR)/*.o $(OBJ_DIR)/.depend $(TARGET)

#----------------------------------------------------------------------

