#!/usr/bin/env atf-sh

atf_test_case install_with_ignored_dep

install_with_ignored_dep_head() {
	atf_set "descr" "Tests for pkg install: with ignored dependency"
}

install_with_ignored_dep_body() {
	mkdir -p repo root/xbps.d pkg_A pkg_B
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" -D "B-1.0_1" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	echo "ignorepkg=B" > root/xbps.d/ignore.conf
	out=$(xbps-install -r root -C xbps.d --repository=$PWD/repo -n A)
	set -- $out
	exp="$1 $2 $3 $4"
	atf_check_equal "$exp" "A-1.0_1 install noarch $PWD/repo"
	xbps-install -r root -C xbps.d --repository=$PWD/repo -yd A
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
	mkdir -p repo root/xbps.d pkg_A pkg_B
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" -D "B-1.0_1" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	echo "ignorepkg=B" > root/xbps.d/ignore.conf
	xbps-install -r root -C xbps.d --repository=$PWD/repo -yd A
	atf_check_equal $? 0
	xbps-query -r root B
	atf_check_equal $? 2
	cd repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" -D "B-1.0_1" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	out=$(xbps-install -r root -C xbps.d --repository=$PWD/repo -un)
	set -- $out
	exp="$1 $2 $3 $4"
	atf_check_equal "$exp" "A-1.1_1 update noarch $PWD/repo"
	xbps-install -r root -C xbps.d --repository=$PWD/repo -yuvd
	atf_check_equal $? 0
	out=$(xbps-query -r root -p pkgver A)
	atf_check_equal $out A-1.1_1
	xbps-query -r root B
	atf_check_equal $? 2
}

atf_test_case remove_with_ignored_dep

remove_with_ignored_dep_head() {
	atf_set "descr" "Tests for pkg remove: with ignored dependency"
}

remove_with_ignored_dep_body() {
	mkdir -p repo root/xbps.d pkg_A pkg_B
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" -D "B-1.0_1" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	echo "ignorepkg=B" > root/xbps.d/ignore.conf
	xbps-install -r root -C xbps.d --repository=$PWD/repo -yd A
	atf_check_equal $? 0
	xbps-query -r root B
	atf_check_equal $? 2
	out=$(xbps-remove -r root -C xbps.d -Rn A)
	set -- $out
	exp="$1 $2 $3 $4"
	atf_check_equal "$exp" "A-1.0_1 remove noarch $PWD/repo"
	xbps-remove -r root -C xbps.d -Ryvd A
	atf_check_equal $? 0
	xbps-query -r root A
	atf_check_equal $? 2
	xbps-query -r root B
	atf_check_equal $? 2
}

atf_test_case remove_ignored_dep

remove_ignored_dep_head() {
	atf_set "descr" "Tests for pkg remove: pkg is dependency but ignored"
}

remove_ignored_dep_body() {
	mkdir -p repo root/xbps.d pkg_A pkg_B
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" -D "B-1.0_1" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root -C xbps.d --repository=$PWD/repo -yd A
	atf_check_equal $? 0
	echo "ignorepkg=B" > root/xbps.d/ignore.conf
	out=$(xbps-remove -r root -C xbps.d -Rn B)
	set -- $out
	exp="$1 $2 $3 $4"
	atf_check_equal "$exp" "B-1.0_1 remove noarch $PWD/repo"
	xbps-remove -r root -C xbps.d -Ryvd B
	atf_check_equal $? 0
	xbps-query -r root A
	atf_check_equal $? 0
	xbps-query -r root B
	atf_check_equal $? 2
}

atf_init_test_cases() {
	atf_add_test_case install_with_ignored_dep
	atf_add_test_case update_with_ignored_dep
	atf_add_test_case remove_with_ignored_dep
	atf_add_test_case remove_ignored_dep
}
