TOPDIR = ../../../..
-include $(TOPDIR)/config.mk

TESTSSUBDIR = xbps/libxbps/shell
TESTSHELL = conf_files_test issue6_test issue18_test issue20_test remove_test
TESTSHELL+= replace_test installmode_test obsoletefiles_test
TESTSHELL+= issue31_test scripts_test incorrect_deps_test
TESTSHELL+= vpkg_test install_test preserve_files_test configure_test
TESTSHELL+= update_shlibs_test update_hold_test update_repolock_test
TESTSHELL+= cyclic_deps_test conflicts_test update_itself_test
TESTSHELL+= hold_test ignore_test preserve_test repo_test
TESTSHELL+= noextract_files_test orphans_test transaction_check_revdeps_test
TESTSHELL+= hooks_test
EXTRA_FILES = Kyuafile

include $(TOPDIR)/mk/test.mk
