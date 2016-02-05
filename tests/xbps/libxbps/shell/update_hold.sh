#!/usr/bin/env atf-sh

atf_test_case update_hold

update_hold_head() {
	atf_set "descr" "Tests for pkg update: pkg is on hold mode"
}

update_hold_body() {
	mkdir -p repo pkg_A
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root --repository=$PWD/repo -yd A
	atf_check_equal $? 0
	xbps-pkgdb -r root -m hold A
	atf_check_equal $? 0
	out=$(xbps-query -r root -H)
	atf_check_equal $out A-1.0_1
	cd repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	out=$(xbps-install -r root --repository=$PWD/repo -un)
	set -- $out
	exp="$1 $2 $3 $4"
	atf_check_equal "$exp" "A-1.1_1 hold noarch $PWD/repo"
	xbps-install -r root --repository=$PWD/repo -yuvd
	atf_check_equal $? 0
	out=$(xbps-query -r root -p pkgver A)
	atf_check_equal $out A-1.0_1

}

atf_test_case update_pkg_with_held_dep

update_pkg_with_held_dep_head() {
	atf_set "descr" "xbps-install(8): update packages with held dependency (issue #143)"
}

update_pkg_with_held_dep_body() {
	atf_expect_death "Known bug: see https://github.com/voidlinux/xbps/issues/143"
	mkdir -p some_repo pkginst pkgheld pkgdep-21_1 pkgdep-22_1
	touch pkginst/pi00
	touch pkgheld/ph00
	touch pkgdep-21_1/pd21
	touch pkgdep-22_1/pd22

	cd some_repo

	xbps-create \
		-A noarch \
		-n "pkgdep-21_1" \
		-s "pkgdep" \
		../pkgdep-21_1

	atf_check_equal $? 0

	xbps-create \
		-A noarch \
		-n "pkgdep-22_1" \
		-s "pkgdep" \
		../pkgdep-22_1

	atf_check_equal $? 0

	xbps-create \
		-A noarch \
		-n "pkginst-1.0_1" \
		-s "pkginst" \
		-D "pkgdep-22_1" \
		../pkginst

	atf_check_equal $? 0

	xbps-create \
		-A noarch \
		-n "pkgheld-1.17.4_2" \
		-s "pkgheld" \
		-P "pkgdep-21_1" \
		../pkgheld

	atf_check_equal $? 0

	#ls -laR ../

	xbps-rindex -d -a pkgheld*.xbps
	atf_check_equal $? 0

	xbps-install -r root -C empty.conf --repository=$PWD -y pkgheld
	atf_check_equal $? 0
	xbps-pkgdb -r root -m hold pkgheld

	xbps-rindex -d -a pkginst*.xbps
	atf_check_equal $? 0

	xbps-rindex -d -a pkgdep-22*.xbps
	atf_check_equal $? 0

	xbps-install -r root -C empty.conf --repository=$PWD -d -y pkginst >&2
	atf_check_equal $? 0
}

atf_init_test_cases() {
	atf_add_test_case update_hold
	atf_add_test_case update_pkg_with_held_dep
}
