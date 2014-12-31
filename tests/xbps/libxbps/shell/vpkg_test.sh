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

atf_test_case vpkg00

vpkg00_head() {
	atf_set "descr" "Tests for virtual pkgs: don't update vpkg"
}

vpkg00_body() {
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

	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -dy A
	atf_check_equal $? 0
	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -dy C
	atf_check_equal $? 0
}

atf_test_case vpkg01

vpkg01_head() {
	atf_set "descr" "Tests for virtual pkgs: commit ebc0f27ae1c"
}

vpkg01_body() {
	mkdir some_repo
	mkdir -p pkg_{A,B,C,D}/usr/bin
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "A>=0" ../pkg_B
	atf_check_equal $? 0
	xbps-create -A noarch -n C-1.0_1 -s "C pkg" --provides "A-1.0_1" --replaces="A>=0" ../pkg_C
	atf_check_equal $? 0
	xbps-create -A noarch -n D-1.0_1 -s "D pkg" --dependencies "C>=0" ../pkg_D
	atf_check_equal $? 0

	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -dy B
	atf_check_equal $? 0
	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -dy D
	atf_check_equal $? 0

	out=$(xbps-query -C empty.conf -r root -l|awk '{print $2}'|tr -d '\n')
	exp="B-1.0_1C-1.0_1D-1.0_1"
	echo "out: $out"
	echo "exp: $exp"
	atf_check_equal $out $exp
}

atf_test_case vpkg02

vpkg02_head() {
	atf_set "descr" "Tests for virtual pkgs: provider in transaction"
}

vpkg02_body() {
	mkdir some_repo
	mkdir -p pkg_{gawk,base-system,busybox}
	cd some_repo
	xbps-create -A noarch -n gawk-1.0_1 -s "gawk pkg" ../pkg_gawk
	atf_check_equal $? 0
	xbps-create -A noarch -n base-system-1.0_1 -s "base-system pkg" --dependencies "gawk>=0" ../pkg_base-system
	atf_check_equal $? 0

	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -dy base-system
	atf_check_equal $? 0

	cd some_repo

	xbps-create -A noarch -n gawk-1.1_1 -s "gawk pkg" --provides "awk-1.0_1" --replaces "awk>=0" ../pkg_gawk
	atf_check_equal $? 0

	xbps-create -A noarch -n busybox-1.0_1 -s "busybox awk" --provides "awk-1.0_1" --replaces "awk>=0" ../pkg_busybox
	atf_check_equal $? 0

	xbps-create -A noarch -n base-system-1.1_1 -s "base-system pkg" --dependencies "awk>=0" ../pkg_base-system
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -dyu
	atf_check_equal $? 0

	out=$(xbps-query -C empty.conf -r root -l|awk '{print $2}'|tr -d '\n')
	exp="base-system-1.1_1gawk-1.1_1"
	echo "out: $out"
	echo "exp: $exp"
	atf_check_equal $out $exp
}

atf_test_case vpkg03

vpkg03_head() {
	atf_set "descr" "Tests for virtual pkgs: provider in configuration file"
}

vpkg03_body() {
	mkdir some_repo
	mkdir -p pkg_gawk
	cd some_repo
	xbps-create -A noarch -n gawk-1.0_1 -s "gawk pkg" ../pkg_gawk
	atf_check_equal $? 0

	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0

	cd ..
	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -dy gawk
	atf_check_equal $? 0

	mkdir -p root/xbps.d

	echo "virtualpkg=awk:gawk" > root/xbps.d/awk.conf
	out=$(xbps-query -C xbps.d -r root --repository=$PWD/some_repo -ppkgver awk)
	exp="gawk-1.0_1"
	echo "out: $out"
	echo "exp: $exp"
	atf_check_equal $out $exp

	echo "virtualpkg=awk-0_1:gawk" > root/xbps.d/awk.conf
	out=$(xbps-query -C xbps.d -r root --repository=$PWD/some_repo -ppkgver awk)
	exp="gawk-1.0_1"
	echo "out: $out"
	echo "exp: $exp"
	atf_check_equal $out $exp
}

atf_init_test_cases() {
	atf_add_test_case vpkg00
	atf_add_test_case vpkg01
	atf_add_test_case vpkg02
	atf_add_test_case vpkg03
}
