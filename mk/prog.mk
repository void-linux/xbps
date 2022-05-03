-include $(TOPDIR)/config.mk

OBJS	?= main.o

MANSECTION ?= 1
MAN ?= $(BIN).$(MANSECTION)

BINS = $(BIN)$(if $(BUILD_SHARED),,.static)

ifeq ($(BUILD_SHARED)$(BUILD_STATIC),yesyes)
BINS += $(BIN).static
endif

CFLAGS += -Wno-unused-command-line-argument

.PHONY: all
all: $(BINS)

.PHONY: clean
clean:
	-rm -f $(BIN) $(BIN).static
	-rm -f *.o

.PHONY: install
install: all
	install -d $(DESTDIR)$(SBINDIR)
ifneq ($(filter $(BIN),$(BINS)),)
	install -m 755 $(BIN) $(DESTDIR)$(SBINDIR)
endif
ifneq ($(filter $(BIN).static,$(BINS)),)
	install -m 755 $(BIN).static $(DESTDIR)$(SBINDIR)
endif
ifdef MAN
	install -d $(DESTDIR)$(MANDIR)/man$(MANSECTION)
	install -m 644 $(MAN) $(DESTDIR)$(MANDIR)/man$(MANSECTION)
endif

.PHONY: uninstall
uninstall:
ifneq ($(filter $(BIN),$(BINS)),)
	-rm -f $(DESTDIR)$(SBINDIR)/$(BIN)
endif
ifneq ($(filter $(BIN).static,$(BINS)),)
	-rm -f $(DESTDIR)$(SBINDIR)/$(BIN).static
endif
ifdef MAN
	-rm -f $(DESTDIR)$(MANDIR)/man$(MANSECTION)/$(MAN)
endif

%.o: %.c
	@printf " [CC]\t\t$@\n"
	${SILENT}$(CC) $(CPPFLAGS) $(PROG_CFLAGS) $(CFLAGS) $(EXTRA_CFLAGS) -c $<

$(BIN).static: $(OBJS) $(TOPDIR)/lib/libxbps.a
	@printf " [CCLD]\t\t$@\n"
	${SILENT}$(CC) -static $(OBJS) $(CPPFLAGS) -L$(TOPDIR)/lib \
		$(CFLAGS) $(LDFLAGS) $(PROG_LDFLAGS) $(STATIC_LIBS) -o $@

$(BIN): $(OBJS) $(TOPDIR)/lib/libxbps.so
	@printf " [CCLD]\t\t$@\n"
	${SILENT}$(CC) $^ $(CPPFLAGS) -L$(TOPDIR)/lib \
		$(CFLAGS) $(PROG_CFLAGS) $(LDFLAGS) $(PROG_LDFLAGS) \
		-lxbps -o $@

