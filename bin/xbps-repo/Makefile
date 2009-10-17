TOPDIR = ../..
include $(TOPDIR)/vars.mk

BIN =	xbps-repo
OBJS =	main.o util.o index.o
STATIC_LIBS += -lz -lbz2 -lacl -llzma

include $(TOPDIR)/prog.mk
