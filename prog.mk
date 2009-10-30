BIN_STATIC  ?= $(BIN).static
OBJS ?=	main.o
MAN ?= $(BIN).8

all: $(BIN) $(BIN_STATIC) $(MAN)
.PHONY: all

$(MAN):
	a2x -f manpage $(MAN).txt

$(BIN_STATIC): $(OBJS)
	$(CC) -static $^ $(LDFLAGS) $(STATIC_LIBS) -o $@

$(BIN): $(OBJS)
	$(CC) $^ $(LDFLAGS) -o $@

.PHONY: clean
clean:
	-rm -f $(BIN) $(BIN_STATIC) $(MAN)
	-rm -f $(OBJS)

.PHONY: install
install: $(BIN) $(BIN_STATIC) $(MAN)
	install -d $(SBINDIR)
	install $(INSTALL_STRIPPED) -m 755 $(BIN) $(SBINDIR)
	install $(INSTALL_STRIPPED) -m 755 $(BIN_STATIC) $(SBINDIR)
ifdef MAN
	install -d $(MANDIR)
	install -m 644 $(MAN) $(MANDIR)
endif

.PHONY: uninstall
uninstall:
	-rm -f $(SBINDIR)/$(BIN) $(SBINDIR)/$(BIN_STATIC)
