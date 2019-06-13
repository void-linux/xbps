#!/usr/bin/env atf-sh

atf_test_case update_xbps

update_xbps_head() {
	atf_set "descr" "Tests for pkg updates: xbps autoupdates itself"
}

update_xbps_body() {
	mkdir -p repo xbps
	touch xbps/foo

	cd repo
	xbps-create -A noarch -n xbps-1.0_1 -s "xbps pkg" ../xbps
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yd xbps
	atf_check_equal $? 0

	out=$(xbps-query -r root -p pkgver xbps)
	atf_check_equal "$out" "xbps-1.0_1"

	cd repo
	xbps-create -A noarch -n xbps-1.1_1 -s "xbps pkg" ../xbps
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/xbps-1.1_1.noarch.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yud
	atf_check_equal $? 0

	out=$(xbps-query -r root -p pkgver xbps)
	atf_check_equal "$out" "xbps-1.1_1"
}

atf_test_case update_xbps_with_revdeps

update_xbps_with_revdeps_head() {
	atf_set "descr" "Tests for pkg updates: xbps autoupdates itself with revdeps"
}

update_xbps_with_revdeps_body() {
	mkdir -p repo xbps xbps-dbg
	touch xbps/foo xbps-dbg/foo

	cd repo
	xbps-create -A noarch -n xbps-1.0_1 -s "xbps pkg" ../xbps
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yd xbps-1.0_1
	atf_check_equal $? 0

	cd repo
	xbps-create -A noarch -n xbps-1.1_1 -s "xbps pkg" ../xbps
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0

	xbps-create -A noarch -n xbps-dbg-1.0_1 -s "xbps-dbg pkg" --dependencies "xbps-1.0_1" ../xbps-dbg
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yd xbps-dbg-1.0_1
	atf_check_equal $? 0

	cd repo
	xbps-create -A noarch -n xbps-dbg-1.1_1 -s "xbps-dbg pkg" --dependencies "xbps-1.1_1" ../xbps-dbg
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yud
	atf_check_equal $? 0

	out=$(xbps-query -r root -p pkgver xbps)
	atf_check_equal $out "xbps-1.1_1"

	out=$(xbps-query -r root -p pkgver xbps-dbg)
	atf_check_equal $out "xbps-dbg-1.1_1"
}

atf_test_case update_xbps_with_uptodate_revdeps

update_xbps_with_uptodate_revdeps_head() {
	atf_set "descr" "Tests for pkg updates: xbps autoupdates itself with already up-to-date revdeps"
}

update_xbps_with_uptodate_revdeps_body() {
	mkdir -p repo xbps base-system
	touch xbps/foo base-system/foo

	cd repo
	xbps-create -A noarch -n xbps-1.0_1 -s "xbps pkg" ../xbps
	atf_check_equal $? 0
	xbps-create -A noarch -n base-system-1.0_1 -s "base-system pkg" --dependencies "xbps>=0" ../base-system
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yd base-system
	atf_check_equal $? 0

	out=$(xbps-query -r root -p pkgver xbps)
	atf_check_equal $out "xbps-1.0_1"

	out=$(xbps-query -r root -p pkgver base-system)
	atf_check_equal $out "base-system-1.0_1"

	cd repo
	xbps-create -A noarch -n xbps-1.1_1 -s "xbps pkg" ../xbps
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yud
	atf_check_equal $? 0

	out=$(xbps-query -r root -p pkgver xbps)
	atf_check_equal $out "xbps-1.1_1"

	out=$(xbps-query -r root -p pkgver base-system)
	atf_check_equal $out "base-system-1.0_1"
}

atf_init_test_cases() {
	atf_add_test_case update_xbps
	atf_add_test_case update_xbps_with_revdeps
	atf_add_test_case update_xbps_with_uptodate_revdeps
}
