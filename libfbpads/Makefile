CC := cc
CFLAGS += -Wall -O2
LDFLAGS +=

FONTS =ar.tfo ai.tfo ab.tfo
OBJS = fbpads.o term.o pad.o draw.o font.o isdw.o scrsnap.o $(FONTS)
LIB_OBJS = libfbpads.o vtconsole.o term.o pad.o draw.o font.o isdw.o scrsnap.o $(FONTS)

all: fbpads libfbpads.a
fbpad.o: conf.h
term.o: conf.h
pad.o: conf.h
%.tfo: %.tf conf.h
	$(LD) -r -b binary -o $@ $<
%.o: %.c %.h Makefile
	$(CC) -c $(CFLAGS) $<
fbpads: $(OBJS) Makefile
	$(CC) -o $@ $(OBJS) $(LDFLAGS)
libfbpads.a: $(LIB_OBJS)
	$(AR) rcs $@ $^
clean:
	rm -f *.o *.a *.tfo fbpads
