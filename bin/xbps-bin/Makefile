TOPDIR = ../..
-include $(TOPDIR)/config.mk

BIN = xbps-bin
OBJS =  install.o main.o remove.o show-deps.o
OBJS += show-info-files.o util.o find-files.o
OBJS += question.o fetch_cb.o trans_cb.o
OBJS += check.o check_pkg_automatic.o check_pkg_files.o
OBJS += check_pkg_rundeps.o check_pkg_symlinks.o
MAN = $(BIN).8

include $(TOPDIR)/prog.mk
