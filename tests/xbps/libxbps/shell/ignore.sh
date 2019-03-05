#!/usr/bin/env atf-sh

atf_test_case install_with_ignored_dep

install_with_ignored_dep_head() {
	atf_set "descr" "Tests for pkg install: with ignored dependency"
}

install_with_ignored_dep_body() {
	mkdir -p repo pkg_A pkg_B
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	echo "ignorepkg=B" > ignore.conf
	out=$(xbps-install -r root -C ignore.conf --repository=$PWD/repo -n A)
	set -- $out
	exp="$1 $2 $3 $4"
	atf_check_equal "$exp" "A-1.0_1 install noarch $PWD/repo"
	xbps-install -r root -C ignore.conf --repository=$PWD/repo -yd A
	atf_check_equal $? 0
	xbps-query -r root A
	atf_check_equal $? 0
	xbps-query -r root B
	atf_check_equal $? 2
}

atf_test_case update_with_ignored_dep

update_with_ignored_dep_head() {
	atf_set "descr" "Tests for pkg update: with ignored dependency"
}

update_with_ignored_dep_body() {
	mkdir -p repo pkg_A pkg_B
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	echo "ignorepkg=B" > ignore.conf
	xbps-install -r root -C ignore.conf --repository=$PWD/repo -yd A
	atf_check_equal $? 0
	cd repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	out=$(xbps-install -r root -C ignore.conf --repository=$PWD/repo -un)
	set -- $out
	exp="$1 $2 $3 $4"
	atf_check_equal "$exp" "A-1.1_1 update noarch $PWD/repo"
	xbps-install -r root --repository=$PWD/repo -yuvd
	atf_check_equal $? 0
	out=$(xbps-query -r root -p pkgver A)
	atf_check_equal $out A-1.1_1
}

atf_init_test_cases() {
	atf_add_test_case install_with_ignored_dep
	atf_add_test_case update_with_ignored_dep
}
