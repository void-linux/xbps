#!/usr/bin/env atf-sh

atf_test_case update_xbps

update_xbps_head() {
	atf_set "descr" "Tests for pkg updates: xbps autoupdates itself"
}

update_xbps_body() {
	mkdir -p repo xbps bar
	touch xbps/foo
	touch bar/bar

	cd repo
	xbps-create -A noarch -n xbps-1.0_1 -s "xbps pkg" ../xbps
	atf_check_equal $? 0
	xbps-create -A noarch -n bar-1.0_1 -s "bar pkg" ../bar
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yd xbps bar
	atf_check_equal $? 0

	out=$(xbps-query -r root -p pkgver xbps)
	atf_check_equal $out xbps-1.0_1

	cd repo
	xbps-create -A noarch -n xbps-1.1_1 -s "xbps pkg" ../xbps
	atf_check_equal $? 0
	xbps-create -A noarch -n bar-1.1_1 -s "bar pkg" ../bar
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	# Ensure warning is printed
	atf_check -s exit:0 -o ignore -e match:"^WARNING: The 'xbps-1\.0_1' package is out of date, 'xbps-1\.1_1' is available\.$" -- xbps-install -r root --repository=$PWD/repo -yd bar

	out=$(xbps-query -r root -p pkgver bar)
	atf_check_equal $out bar-1.1_1

	out=$(xbps-query -r root -p pkgver xbps)
	atf_check_equal $out xbps-1.0_1

	xbps-install -r root --repository=$PWD/repo -yu xbps
	atf_check_equal $? 0

	out=$(xbps-query -r root -p pkgver xbps)
	atf_check_equal $out xbps-1.1_1
}

atf_test_case update_xbps_with_revdeps

update_xbps_with_revdeps_head() {
	atf_set "descr" "Tests for pkg updates: xbps updates itself with revdeps"
}

update_xbps_with_revdeps_body() {
	mkdir -p repo xbps xbps-dbg bar baz
	touch xbps/foo xbps-dbg/bar bar/sailor baz/blah

	cd repo
	xbps-create -A noarch -n xbps-1.0_1 -s "xbps pkg" ../xbps
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yd xbps-1.0_1
	atf_check_equal $? 0

	cd repo
	xbps-create -A noarch -n baz-1.0_1 -s "baz pkg" ../baz
	atf_check_equal $? 0
	xbps-create -A noarch -n bar-1.0_1 -s "bar pkg" ../bar
	atf_check_equal $? 0
	xbps-create -A noarch -n xbps-dbg-1.0_1 -s "xbps-dbg pkg" --dependencies "xbps-1.0_1" ../xbps-dbg
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yd xbps-dbg baz
	atf_check_equal $? 0

	cd repo
	xbps-create -A noarch -n xbps-1.1_1 -s "xbps pkg" ../xbps
	atf_check_equal $? 0
	xbps-create -A noarch -n baz-1.1_1 -s "baz pkg" ../baz
	atf_check_equal $? 0
	xbps-create -A noarch -n xbps-dbg-1.1_1 -s "xbps-dbg pkg" --dependencies "xbps-1.1_1" ../xbps-dbg
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	# first time, warning must be printed
	atf_check -s exit:0 -o ignore -e match:"^WARNING: The 'xbps-1\.0_1' package is out of date, 'xbps-1\.1_1' is available\.$" -- xbps-install -r root --repository=$PWD/repo -yd bar

	# don't print warning while updating xbps
	atf_check -s exit:0 -o ignore -e not-match:"^WARNING: The 'xbps-1\.0_1' package is out of date, 'xbps-1\.1_1' is available\.$" -- xbps-install -r root --repository=$PWD/repo -yu xbps

	out=$(xbps-query -r root -p pkgver xbps)
	atf_check_equal $out xbps-1.1_1

	out=$(xbps-query -r root -p pkgver xbps-dbg)
	atf_check_equal $out xbps-dbg-1.1_1

	out=$(xbps-query -r root -p pkgver baz)
	atf_check_equal $out baz-1.0_1

	# second time, updates everything
	xbps-install -r root --repository=$PWD/repo -yud
	atf_check_equal $? 0

	out=$(xbps-query -r root -p pkgver xbps)
	atf_check_equal $out xbps-1.1_1

	out=$(xbps-query -r root -p pkgver xbps-dbg)
	atf_check_equal $out xbps-dbg-1.1_1

	out=$(xbps-query -r root -p pkgver baz)
	atf_check_equal $out baz-1.1_1
}

atf_test_case update_xbps_with_uptodate_revdeps

update_xbps_with_uptodate_revdeps_head() {
	atf_set "descr" "Tests for pkg updates: xbps updates itself with already up-to-date revdeps"
}

update_xbps_with_uptodate_revdeps_body() {
	mkdir -p repo xbps base-system
	touch xbps/foo base-system/bar

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

	# don't print warning while updating xbps
	atf_check -s exit:0 -o ignore -e not-match:"^WARNING: The 'xbps-1\.0_1' package is out of date, 'xbps-1\.1_1' is available\.$" -- xbps-install -r root --repository=$PWD/repo -yu xbps

	out=$(xbps-query -r root -p pkgver xbps)
	atf_check_equal $out xbps-1.1_1

	out=$(xbps-query -r root -p pkgver base-system)
	atf_check_equal $out base-system-1.0_1
}

atf_test_case update_xbps_with_indirect_revdeps

update_xbps_with_indirect_revdeps_head() {
	atf_set "descr" "Tests for pkg updates: xbps updates itself with indirect revdeps"
}

update_xbps_with_indirect_revdeps_body() {
	mkdir -p repo pkg

	atf_expect_fail "Integrity checks are no longer bypassed for xbps self-update: https://github.com/void-linux/xbps/pull/597"

	cd repo
	xbps-create -A noarch -n xbps-1.0_1 -s "xbps pkg" --dependencies "libcrypto-1.0_1 cacerts>=0" ../pkg
	atf_check_equal $? 0
	xbps-create -A noarch -n libcrypto-1.0_1 -s "libcrypto pkg" ../pkg
	atf_check_equal $? 0
	xbps-create -A noarch -n libressl-1.0_1 -s "libressl pkg" --dependencies "libcrypto-1.0_1" ../pkg
	atf_check_equal $? 0
	xbps-create -A noarch -n cacerts-1.0_1 -s "cacerts pkg" --dependencies "libressl>=0" ../pkg
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yd xbps-1.0_1
	atf_check_equal $? 0

	out=$(xbps-query -r root -p pkgver xbps)
	atf_check_equal "$out" "xbps-1.0_1"

	out=$(xbps-query -r root -p pkgver libcrypto)
	atf_check_equal "$out" "libcrypto-1.0_1"

	out=$(xbps-query -r root -p pkgver libressl)
	atf_check_equal "$out" "libressl-1.0_1"

	out=$(xbps-query -r root -p pkgver cacerts)
	atf_check_equal "$out" "cacerts-1.0_1"

	cd repo
	xbps-create -A noarch -n xbps-1.1_1 -s "xbps pkg" --dependencies "libcrypto-1.1_1 ca-certs>=0" ../pkg
	atf_check_equal $? 0
	xbps-create -A noarch -n libcrypto-1.1_1 -s "libcrypto pkg" ../pkg
	atf_check_equal $? 0
	xbps-create -A noarch -n libressl-1.1_1 -s "libressl pkg" --dependencies "libcrypto-1.1_1" ../pkg
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yu xbps
	atf_check_equal $? 0

	out=$(xbps-query -r root -p pkgver xbps)
	atf_check_equal "$out" "xbps-1.1_1"

	out=$(xbps-query -r root -p pkgver libcrypto)
	atf_check_equal "$out" "libcrypto-1.1_1"

	out=$(xbps-query -r root -p pkgver libressl)
	atf_check_equal "$out" "libressl-1.0_1"

	out=$(xbps-query -r root -p pkgver cacerts)
	atf_check_equal "$out" "cacerts-1.0_1"

	xbps-install -r root --repository=$PWD/repo -yu
	atf_check_equal $? 0

	out=$(xbps-query -r root -p pkgver xbps)
	atf_check_equal "$out" "xbps-1.1_1"

	out=$(xbps-query -r root -p pkgver libcrypto)
	atf_check_equal "$out" "libcrypto-1.1_1"

	out=$(xbps-query -r root -p pkgver libressl)
	atf_check_equal "$out" "libressl-1.1_1"

	out=$(xbps-query -r root -p pkgver cacerts)
	atf_check_equal "$out" "cacerts-1.0_1"
}

atf_init_test_cases() {
	atf_add_test_case update_xbps
	atf_add_test_case update_xbps_with_revdeps
	atf_add_test_case update_xbps_with_indirect_revdeps
	atf_add_test_case update_xbps_with_uptodate_revdeps
}
