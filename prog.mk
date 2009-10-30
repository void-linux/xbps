OBJS ?=	main.o
MAN ?= $(BIN).8

ifdef STATIC
all: $(BIN).static
else
LDFLAGS = -lxbps
all: $(BIN) $(MAN)
endif

.PHONY: all

$(MAN):
	a2x -f manpage $(MAN).txt

$(BIN).static: $(OBJS)
	$(CC) $^ -static -lxbps $(LDFLAGS) -o $@

$(BIN): $(OBJS)
	$(CC) $^ $(LDFLAGS) -o $@

.PHONY: clean
clean:
	-rm -f $(BIN) $(MAN)
	-rm -f $(BIN).static
	-rm -f $(OBJS)

.PHONY: install
install: $(BIN) $(MAN)
	install -d $(SBINDIR)
	install $(INSTALL_STRIPPED) -m 755 $(BIN) $(SBINDIR)
ifdef STATIC
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
