TOPDIR = ../..
include $(TOPDIR)/vars.mk

BIN =	xbps-repo
OBJS =	main.o util.o index.o
STATIC_LIBS += -lz -lbz2 -lacl -llzmadec

include $(TOPDIR)/prog.mk
