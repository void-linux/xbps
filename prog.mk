BIN_STATIC  ?= $(BIN).static
OBJS ?=	main.o

all: $(BIN) $(BIN_STATIC)
.PHONY: all

$(BIN_STATIC): $(OBJS)
	$(CC) -static $^ $(LDFLAGS) $(STATIC_LIBS) -o $@

$(BIN): $(OBJS)
	$(CC) $^ $(LDFLAGS) -o $@

.PHONY: clean
clean: clean-bins clean-objs

clean-bins:
	-rm -f $(BIN) $(BIN_STATIC)

clean-objs:
	-rm -f $(OBJS)

.PHONY: install
install: $(BIN) $(BIN_STATIC)
	install -d $(SBINDIR)
	install $(INSTALL_STRIPPED) -m 755 $(BIN) $(SBINDIR)
	install $(INSTALL_STRIPPED) -m 755 $(BIN_STATIC) $(SBINDIR)
