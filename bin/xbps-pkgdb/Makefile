TOPDIR = ../..
-include $(TOPDIR)/config.mk

BIN =	xbps-pkgdb
OBJS =	main.o check.o check_pkg_files.o
OBJS +=	check_pkg_rundeps.o
OBJS +=	check_pkg_symlinks.o check_pkg_unneeded.o
OBJS +=	convert.o

include $(TOPDIR)/mk/prog.mk
