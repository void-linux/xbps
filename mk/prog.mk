-include $(TOPDIR)/config.mk

OBJS	?= main.o

BINS = $(BIN)
MAN ?= $(BIN).8

ifdef BUILD_STATIC
BINS += $(BIN).static
endif

.PHONY: all
all: $(BINS)

.PHONY: clean
clean:
	-rm -f $(BIN) $(BIN).static
	-rm -f *.o

.PHONY: install
install: all
	install -d $(DESTDIR)$(SBINDIR)
	install -m 755 $(BIN) $(DESTDIR)$(SBINDIR)
ifdef BUILD_STATIC
	install -m 755 $(BIN).static $(DESTDIR)$(SBINDIR)
endif
ifdef MAN
	install -d $(DESTDIR)$(MANDIR)/man8
	install -m 644 $(MAN) $(DESTDIR)$(MANDIR)/man8
endif

.PHONY: uninstall
uninstall:
	-rm -f $(DESTDIR)$(SBINDIR)/$(BIN)
ifdef BUILD_STATIC
	-rm -f $(DESTDIR)$(SBINDIR)/$(BIN).static
endif
ifdef MAN
	-rm -f $(DESTDIR)$(MANDIR)/man8/$(MAN)
endif

%.o: %.c
	@printf " [CC]\t\t$@\n"
	${SILENT}$(CC) $(CPPFLAGS) $(CFLAGS) -c $<

$(BIN).static: $(OBJS)
	@printf " [CCLD]\t\t$@\n"
	${SILENT}$(CC) -static $^ $(CPPFLAGS) -L$(TOPDIR)/lib \
		$(CFLAGS) $(LDFLAGS) $(STATIC_LIBS) -o $@

$(BIN): $(OBJS)
	@printf " [CCLD]\t\t$@\n"
	${SILENT}$(CC) $^ $(CPPFLAGS) -L$(TOPDIR)/lib \
		$(CFLAGS) $(PROG_CFLAGS) $(LDFLAGS) $(PROG_LDFLAGS) \
		-lxbps -o $@

