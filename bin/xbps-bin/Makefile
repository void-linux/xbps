TOPDIR = ../..
-include $(TOPDIR)/config.mk

BIN = xbps-bin
OBJS =  transaction.o main.o show-deps.o
OBJS += show-info-files.o util.o find-files.o
OBJS += question.o fetch_cb.o state_cb.o
OBJS += check.o check_pkg_automatic.o check_pkg_files.o
OBJS += check_pkg_rundeps.o check_pkg_symlinks.o
OBJS += check_pkg_requiredby.o
OBJS += show-orphans.o unpack_cb.o list.o
MAN = $(BIN).8

include $(TOPDIR)/mk/prog.mk
