OBJS	?= main.o
CFLAGS  += -fPIE
LDFLAGS += -lxbps

.PHONY: all
all: $(BIN) $(BIN).static $(MAN)

.PHONY: clean
clean:
	-rm -f $(BIN) $(MAN)
	-rm -f $(BIN).static
	-rm -f $(OBJS)

.PHONY: install
install: all
	install -d $(SBINDIR)
	install $(INSTALL_STRIPPED) -m 755 $(BIN) $(SBINDIR)
	install $(INSTALL_STRIPPED) -m 755 $(BIN).static $(SBINDIR)
ifdef MAN
	install -d $(MANDIR)
	install -m 644 $(MAN) $(MANDIR)
endif

.PHONY: uninstall
uninstall:
	-rm -f $(SBINDIR)/$(BIN)
	-rm -f $(SBINDIR)/$(BIN).static

ifdef MAN
	-rm -f $(MANDIR)/$(MAN)
endif

%.o: %.c
	@echo "    [CC] $@"
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $<

$(MAN):
	@echo "    [ASCIIDOC] $(MAN)"
	@a2x -f manpage $(MAN).txt

$(BIN).static: $(OBJS)
	@echo "    [CCLD] $@"
	@$(CC) -static $^ $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) \
		$(STATIC_LIBS) -o $@ >/dev/null 2>&1

$(BIN): $(OBJS)
	@echo "    [CCLD] $@"
	@$(CC) $^ $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -pie -o $@

