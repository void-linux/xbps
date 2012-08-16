TOPDIR = ../..
-include $(TOPDIR)/config.mk

BIN =	xbps-repo
OBJS =	main.o index.o show.o find-files.o list.o
OBJS += index-files.o clean.o common.o
OBJS += remove-obsoletes.o
OBJS += ../xbps-bin/fetch_cb.o ../xbps-bin/util.o
OBJS += ../xbps-bin/state_cb.o ../xbps-bin/list.o
MAN = $(BIN).8

include $(TOPDIR)/mk/prog.mk
