
SHELL       = /bin/sh

TOP         = /usr/local/sce/ee
LIBDIR      = $(TOP)/lib
INCDIR      = $(TOP)/include

TARGET      = main
OBJS        = crt0.o \
              res.o \
              opening.o \
              towerchain.o \
              $(TARGET).o

LCFILE      = $(LIBDIR)/app.cmd
LIBS        = $(LIBDIR)/libgraph.a \
              $(LIBDIR)/libvu0.a \
              $(LIBDIR)/libdma.a \
              $(LIBDIR)/libpkt.a \
              $(LIBDIR)/libdev.a

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
LDFLAGS     = -mno-crt0 -L$(LIBDIR) -lm
TMPFLAGS    =

.SUFFIXES: .c .s .cc .dsm

all: $(TARGET).elf

$(TARGET).elf: $(OBJS) $(LIBS)
	$(LD) -o $@ -T $(LCFILE) $(OBJS) $(LIBS) $(LDFLAGS)

crt0.o: $(LIBDIR)/crt0.s
	$(AS) $(ASFLAGS) $(TMPFLAGS) -o $@ $<

.s.o:
	$(AS) $(ASFLAGS) $(TMPFLAGS) -I$(INCDIR) -o $@ $<

.dsm.o:
	$(DVPASM) $(DVPASMFLAGS) -I$(INCDIR) -o $@ $<

.c.o:
	$(CC) $(CFLAGS) $(TMPFLAGS) -I$(INCDIR) -c $< -o $*.o

.cc.o:
	$(CC) $(CXXFLAGS) $(TMPFLAGS) -I$(INCDIR) -c $< -o $*.o

run: $(TARGET).elf
	$(RUN) $(TARGET).elf

clean:
	$(RM) *.o *.map *.lst core *.dis *.elf
