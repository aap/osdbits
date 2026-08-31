
SHELL       = /bin/sh

# freesce (the sdk-2.4-branch build installed in /usr/local/freesce) is
# the primary SDK; the original SDK 2.4 install fills in what freesce
# doesn't have yet (libpkt, and any header freesce lacks - freesce's
# include dir is searched first).  "make TOP=/usr/local/sce/ee
# SCETOP=/usr/local/sce/ee" relinks the plain sce 3.0.3 SDK.
TOP         = /usr/local/freesce/ee
SCETOP      = /usr/local/sce_24/ee
LIBDIR      = $(TOP)/lib
INCDIR      = $(TOP)/include
INCS        = -I$(INCDIR) -I$(SCETOP)/include

TARGET      = main
OBJS        = crt0.o \
              res.o \
              opening.o \
              menu.o \
              menuback.o \
              menutext.o \
              pad.o \
              towerchain.o \
              $(TARGET).o

LCFILE      = $(LIBDIR)/app.cmd
# libpkt and libdev come from the original SDK until freesce has them
# (freesce's libdev is missing the vif1/vu1/gif resets).  libvu0 too:
# freesce's libvu0.c loses the branch-delay-slot instructions (no .set
# noreorder around the asm blocks), so e.g. sceVu0MulMatrix writes all
# four columns to column 0 - flip back to $(LIBDIR) once that's fixed.
# libopad is the SDK 2.0 libpad and the only one that accepts the boot
# ROM's padman 3.6 (rom0:XPADMAN) - it goes with pad.c's rom0: module
# loading and with nothing else.  freesce is the only tree that has it.
LIBS        = $(LIBDIR)/libgraph.a \
              $(SCETOP)/lib/libvu0.a \
              $(LIBDIR)/libdma.a \
              $(SCETOP)/lib/libpkt.a \
              $(SCETOP)/lib/libdev.a \
              $(LIBDIR)/libopad.a

PREFIX      = ee
AS          = $(PREFIX)-gcc
CC          = $(PREFIX)-gcc
LD          = $(PREFIX)-gcc
DVPASM      = $(PREFIX)-dvp-as
OBJDUMP     = $(PREFIX)-objdump
RUN         = dsedb -r run
RM          = /bin/rm -f

CFLAGS      = -O2 -Wall -fno-common -fno-strict-aliasing
CXXFLAGS    = -O2 -Wall -fno-exceptions -fno-common -fno-strict-aliasing
ASFLAGS     = -c -xassembler-with-cpp
DVPASMFLAGS = 
# freesce's app.cmd has no GROUP(-lc -lkernl -lgcc) like the sce one, so
# name them explicitly; libkernl (freesce klib) and libc are mutually
# recursive, hence the group
LDFLAGS     = -mno-crt0 -L$(LIBDIR) \
              -Wl,--start-group -lkernl -lc -lgcc -Wl,--end-group -lm
TMPFLAGS    =

.SUFFIXES: .c .s .cc .dsm

all: $(TARGET).elf

$(TARGET).elf: $(OBJS) $(LIBS)
	$(LD) -o $@ -T $(LCFILE) $(OBJS) $(LIBS) $(LDFLAGS)

# freesce installs a prebuilt crt0.o (no crt0.s in the prefix)
crt0.o: $(LIBDIR)/crt0.o
	cp $< $@

.s.o:
	$(AS) $(ASFLAGS) $(TMPFLAGS) $(INCS) -o $@ $<

.dsm.o:
	$(DVPASM) $(DVPASMFLAGS) $(INCS) -o $@ $<

.c.o:
	$(CC) $(CFLAGS) $(TMPFLAGS) $(INCS) -c $< -o $*.o

.cc.o:
	$(CC) $(CXXFLAGS) $(TMPFLAGS) $(INCS) -c $< -o $*.o

run: $(TARGET).elf
	$(RUN) $(TARGET).elf

clean:
	$(RM) *.o *.map *.lst core *.dis *.elf
