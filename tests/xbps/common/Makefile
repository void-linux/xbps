TOPDIR = ../../..
-include $(TOPDIR)/config.mk

TESTSSUBDIR = xbps

all:

install:
	install -d $(DESTDIR)$(TESTSDIR)/$(TESTSSUBDIR)
	install -m644 ../Kyuafile $(DESTDIR)$(TESTSDIR)/$(TESTSSUBDIR)

uninstall:
	-rm -f $(DESTDIR)$(TESTSDIR)/$(TESTSSUBDIR)/Kyuafile

clean:
