TOPDIR = ../..
-include $(TOPDIR)/config.mk

BIN =	xbps-rindex
OBJS =	main.o index-add.o index-clean.o remove-obsoletes.o repoflush.o
OBJS += readpassphrase.o sign.o
EXTRA_CFLAGS = -Wno-unused-result

include $(TOPDIR)/mk/prog.mk
