TOPDIR = ../..
-include $(TOPDIR)/config.mk

BIN =	xbps-rindex
OBJS =	main.o index-add.o index-clean.o register-outmoded.o remove-obsoletes.o repoflush.o sign.o

include $(TOPDIR)/mk/prog.mk

LDFLAGS += -lcrypto
