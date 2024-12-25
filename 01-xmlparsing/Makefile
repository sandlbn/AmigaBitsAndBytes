# Makefile for Radio XML Parser and Search programs

# Compiler settings
CC = m68k-amigaos-gcc
CPU_FLAGS = -mnobitfield -fno-peephole2

# Directories
LIBDIR = /opt/amiga/m68k-amigaos/lib
SDKDIR = /opt/amiga/m68k-amigaos/sys-include
NDKDIR = /opt/amiga/m68k-amigaos/ndk-include
INCDIR = /opt/amiga/m68k-amigaos/include
BUILDDIR = build
OUTDIR = out

# Program names
PARSER_NAME = radioparser
SEARCH_NAME = radiosearch

# Source files
PARSER_SRC = xml_parser.c
SEARCH_SRC = radio_search.c

# Object files
PARSER_OBJ = $(BUILDDIR)/xml_parser.o
SEARCH_OBJ = $(BUILDDIR)/radio_search.o

# Compiler flags
BASE_CCFLAGS = -Wall -Wextra \
    -fno-lto -noixemul \
    -fbaserel $(CPU_FLAGS) \
    -I$(INCDIR) -I$(SDKDIR) -I$(NDKDIR)

ifdef DEBUG
CCFLAGS = $(BASE_CCFLAGS) -DDEBUG_BUILD -O0 -g
else
CCFLAGS = $(BASE_CCFLAGS) -O2 -fomit-frame-pointer
endif

LDFLAGS = -Wl,-Map=$(OUTDIR)/$(notdir $@).map,-L$(LIBDIR),-lamiga

# Targets
.PHONY: all clean debug release dirs

all: dirs $(OUTDIR)/$(PARSER_NAME) $(OUTDIR)/$(SEARCH_NAME)

debug: CCFLAGS += -DDEBUG
debug: all

release: all

dirs:
	mkdir -p $(BUILDDIR) $(OUTDIR)

$(BUILDDIR)/xml_parser.o: xml_parser.c
	$(CC) $(CCFLAGS) -c $< -o $@

$(BUILDDIR)/radio_search.o: radio_search.c
	$(CC) $(CCFLAGS) -c $< -o $@

$(OUTDIR)/$(PARSER_NAME): $(PARSER_OBJ)
	$(CC) $(CCFLAGS) $^ -o $@ $(LDFLAGS)

$(OUTDIR)/$(SEARCH_NAME): $(SEARCH_OBJ)
	$(CC) $(CCFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILDDIR)/* $(OUTDIR)/*