TOPDIR = ../../../..
-include $(TOPDIR)/config.mk

TESTSSUBDIR = xbps/libxbps/shell
TESTSHELL = conf_files_test issue6_test issue18_test issue20_test remove_test
TESTSHELL+= replace_test installmode_test obsoletefiles_test
TESTSHELL+= issue31_test scripts_test incorrect_deps_test
TESTSHELL+= vpkg_test install_test preserve_files_test configure_test
TESTSHELL+= update_shlibs update_hold update_repolock cyclic_deps conflicts
EXTRA_FILES = Kyuafile

include $(TOPDIR)/mk/test.mk
