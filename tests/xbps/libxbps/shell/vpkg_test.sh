#!/usr/bin/env atf-sh
#

# This test case is a bit special because it stresses how virtual packages
# are handled in xbps.
#
# - A-1.0 is installed, provides vpkg libEGL-1.0.
# - B-1.0 is installed and depends on A.
# - C-1.0 is installed and depends on libEGL>=2.0.
# - D-1.0 is installed as dependency of C, and provides libEGL-2.0.
# - A should not be updated to D.
#
# D should replace A only if it has "replaces" property on A. The result should be
# that D must be installed and A being as is.

atf_test_case vpkg_noupdate

vpkg_noupdate_head() {
	atf_set "descr" "Tests for virtual pkgs: don't update vpkg"
}

vpkg_noupdate_body() {
	mkdir some_repo
	mkdir -p pkg_{A,B,C,D}/usr/bin
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" --provides "libEGL-1.0_1" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "A>=0" ../pkg_B
	atf_check_equal $? 0
	xbps-create -A noarch -n C-1.0_1 -s "C pkg" --dependencies "libEGL>=2.0" ../pkg_C
	atf_check_equal $? 0
	xbps-create -A noarch -n D-1.0_1 -s "D pkg" --provides "libEGL-2.0_1" ../pkg_D
	atf_check_equal $? 0

	xbps-rindex -a *.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -dy A
	atf_check_equal $? 0
	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -dy C
	atf_check_equal $? 0
}

atf_init_test_cases() {
	atf_add_test_case vpkg_noupdate
}
