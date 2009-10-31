TOPDIR = ../..
include $(TOPDIR)/vars.mk

CFLAGS += -Wno-unused-macros -Wno-conversion
CPPFLAGS += -DFTP_COMBINE_CWDS -DNETBSD -I$(TOPDIR)/include

ifdef WITH_INET6
CPPFLAGS += -DINET6
endif

ifdef WITH_SSL
CPPFLAGS += -DWITH_SSL
endif

ifdef STATIC
CFLAGS += -static
endif

OBJS= fetch.o common.o ftp.o http.o file.o
INCS= common.h
GEN = ftperr.h httperr.h

.PHONY: all
all: $(OBJS)

%.o: %.c $(INCS) $(GEN)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -c $<

ftperr.h: ftp.errors
	./errlist.sh ftp_errlist FTP ftp.errors > $@

httperr.h: http.errors
	./errlist.sh http_errlist HTTP http.errors > $@

.PHONY: clean
clean:
	-rm -f $(GEN) $(OBJS)
