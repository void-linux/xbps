OBJS	?= main.o
CFLAGS  += -fPIE
LDFLAGS += -lxbps

.PHONY: all
all: $(BIN) $(BIN).static

.PHONY: clean
clean:
	-rm -f $(BIN) $(BIN).static
	-rm -f $(OBJS)

.PHONY: install
install: all
	install -d $(DESTDIR)$(SBINDIR)
	install $(INSTALL_STRIPPED) -m 755 $(BIN) $(DESTDIR)$(SBINDIR)
	install $(INSTALL_STRIPPED) -m 755 $(BIN).static $(DESTDIR)$(SBINDIR)
ifdef MAN
	install -d $(DESTDIR)$(MANDIR)/man8
	install -m 644 $(MAN) $(DESTDIR)$(MANDIR)/man8
endif

.PHONY: uninstall
uninstall:
	-rm -f $(DESTDIR)$(SBINDIR)/$(BIN)
	-rm -f $(DESTDIR)$(SBINDIR)/$(BIN).static
ifdef MAN
	-rm -f $(DESTDIR)$(MANDIR)/man8/$(MAN)
endif

%.o: %.c
	@printf " [CC]\t\t$@\n"
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $<

$(BIN).static: $(OBJS)
	@printf " [CCLD]\t\t$@\n"
	@$(CC) -static $^ $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) \
		$(STATIC_LIBS) -o $@ >/dev/null 2>&1

$(BIN): $(OBJS)
	@printf " [CCLD]\t\t$@\n"
	@$(CC) $^ $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -pie -o $@

