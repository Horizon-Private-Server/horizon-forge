EE_LIBS = -ldl
EE_OBJS = ##EEOBJS##
EE_ELF = out.elf
EE_BIN = out.bin
OUT_BUILD_DIR = $(CURDIR)/../../build/dl
OUT_CODE_DIR = $(CURDIR)/../code
OUT_MAPNAME = ##MAPNAME##
EE_INCS := -I$(CURDIR)/include
EE_LDFLAGS := ##EELDFLAGS## -fno-builtin -nostdlib -nostartfiles -L.

# set to DEBUG or RELEASE
BUILD_CONFIG := ##EEBUILD##

# build flags
# 	DEBUG									Enables debug strings and additional misc debug logic.

ifeq ($(BUILD_CONFIG),DEBUG)
EE_DEFS := -DDEBUG
else ifeq ($(BUILD_CONFIG),RELEASE)
EE_DEFS := -DRELEASE
endif

all: $(EE_BIN) clean
	# payload and definition
	cp -rf "$(CURDIR)/out.bin" "$(OUT_CODE_DIR)/code.0008.bin"
	cp -rf "$(CURDIR)/hook.bin" "$(OUT_CODE_DIR)/code.0009.bin"
	bin2codedef "$(OUT_CODE_DIR)/code.0008.bin" 01EF0000 "$(OUT_CODE_DIR)/code.0008.def" 004A7D98
	bin2codedef "$(OUT_CODE_DIR)/code.0009.bin" 00598BA0 "$(OUT_CODE_DIR)/code.0009.def" 004A7D98
	rm -rf "$(OUT_CODE_DIR)/$(OUT_MAPNAME).code"
	
clean:
#	rm -rf *.o
	find . -name "*.o" -delete

include $(CURDIR)/Makefile.pref
include $(CURDIR)/Makefile.eeglobal
include $(CURDIR)/Makefile.rules
