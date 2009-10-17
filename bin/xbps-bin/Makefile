TOPDIR = ../..
include $(TOPDIR)/vars.mk

BIN = xbps-bin
OBJS = check.o install.o main.o remove.o show-deps.o ../xbps-repo/util.o
STATIC_LIBS += -lz -lacl -lbz2 -llzma

include $(TOPDIR)/prog.mk

