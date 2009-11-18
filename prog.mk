OBJS ?=	main.o
MAN ?= $(BIN).8

LDFLAGS += -lxbps

ifdef STATIC
all: $(BIN).static
MAN =
else
all: $(BIN) $(MAN)
endif

.PHONY: all

$(MAN):
	a2x -f manpage $(MAN).txt

$(BIN).static: $(OBJS)
	$(CC) $^ $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) \
		$(STATIC_LIBS) -o $@

$(BIN): $(OBJS)
	$(CC) $^ $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

.PHONY: clean
clean:
	-rm -f $(BIN) $(MAN)
	-rm -f $(BIN).static
	-rm -f $(OBJS)

.PHONY: install
install: all
	install -d $(SBINDIR)
ifndef STATIC
	install $(INSTALL_STRIPPED) -m 755 $(BIN) $(SBINDIR)
else
	install $(INSTALL_STRIPPED) -m 755 $(BIN).static $(SBINDIR)
endif
ifdef MAN
	install -d $(MANDIR)
	install -m 644 $(MAN) $(MANDIR)
endif

.PHONY: uninstall
uninstall:
	-rm -f $(SBINDIR)/$(BIN)
	-rm -f $(SBINDIR)/$(BIN).static
