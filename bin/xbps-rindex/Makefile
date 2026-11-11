TOPDIR = ../..
-include $(TOPDIR)/config.mk

BIN =	xbps-rindex
OBJS =	main.o index-add.o index-clean.o remove-obsoletes.o repoflush.o sign.o
OBJS +=	index-purge.o

include $(TOPDIR)/mk/prog.mk

LDFLAGS += -lcrypto
