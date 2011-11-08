TOPDIR = ../..
-include $(TOPDIR)/config.mk

BIN =	xbps-repo
OBJS =	main.o index.o repository.o find-files.o
OBJS += ../xbps-bin/fetch_cb.o ../xbps-bin/util.o
OBJS += ../xbps-bin/trans_cb.o
MAN = $(BIN).8

include $(TOPDIR)/bin/prog.mk
