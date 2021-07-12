TOPDIR = ../..
-include $(TOPDIR)/config.mk

BIN =	xbps-rindex
OBJS =	main.o index-add.o index-clean.o index-remove.o remove-obsoletes.o sign.o

include $(TOPDIR)/mk/prog.mk

LDFLAGS += -lcrypto
