#!/usr/bin/env atf-sh

atf_test_case install_empty

install_empty_head() {
	atf_set "descr" "Tests for pkg installations: install pkg with no files"
}

install_empty_body() {
	mkdir -p repo pkg_A
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -C empty.conf -r root --repository=$PWD/repo -yd A
	atf_check_equal $? 0
	rv=0
	if [ -e root/var/db/xbps/.A-files.plist ]; then
		rv=1
	fi
	atf_check_equal $rv 0
}

atf_test_case install_with_deps

install_with_deps_head() {
	atf_set "descr" "Tests for pkg installations: install with deps in proper order"
}

install_with_deps_body() {
	# Proper order: A B C D
	mkdir some_repo
	mkdir -p pkg_{A,B,C,D}/usr/bin
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "A>=0" ../pkg_B
	atf_check_equal $? 0
	xbps-create -A noarch -n C-1.0_1 -s "C pkg" --dependencies "A>=0 B>=0" ../pkg_C
	atf_check_equal $? 0
	xbps-create -A noarch -n D-1.0_1 -s "D pkg" --dependencies "C>=0 B>=0" ../pkg_D
	atf_check_equal $? 0

	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	echo -e "A-1.0_1\nB-1.0_1\nC-1.0_1\nD-1.0_1\n" > exp
	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -yn D|awk '{print $1}' > out
	echo >> out
	echo "exp: '$(cat exp)'" >&2
	echo "out: '$(cat out)'" >&2
	cmp exp out
	atf_check_equal $? 0
}

atf_test_case install_with_vpkg_deps

install_with_vpkg_deps_head() {
	atf_set "descr" "Tests for pkg installations: install with virtual pkg updates in proper order"
}

install_with_vpkg_deps_body() {
	# Proper order: D C A B
	mkdir some_repo
	mkdir -p pkg_{A,B,C,D}/usr/bin
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "A>=0" ../pkg_B
	atf_check_equal $? 0
	xbps-create -A noarch -n C-1.0_1 -s "C pkg" --provides="E-1.0_1" --dependencies "D>=0" ../pkg_C
	atf_check_equal $? 0
	xbps-create -A noarch -n D-1.0_1 -s "D pkg" --dependencies "E>=0 B>=0" ../pkg_D
	atf_check_equal $? 0

	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	echo -e "A-1.0_1\nB-1.0_1\nD-1.0_1\nC-1.0_1\n" > exp
	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -yn E|awk '{print $1}' > out
	echo >> out
	echo "exp: '$(cat exp)'" >&2
	echo "out: '$(cat out)'" >&2
	cmp exp out
	atf_check_equal $? 0
}

atf_test_case update_to_empty_pkg

update_to_empty_pkg_head() {
	atf_set "descr" "Tests for pkg installations: update pkg with files to another version without"
}

update_to_empty_pkg_body() {
	mkdir -p repo pkg_A/usr/bin
	touch pkg_A/usr/bin/blah
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/repo -yd A
	atf_check_equal $? 0

	cd repo
	rm -rf ../pkg_A/*
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/repo -yud
	atf_check_equal $? 0
	rv=0
	if [ -e root/usr/bin/blah -a -e root/var/db/xbps/.A-files.plist ]; then
		rv=1
	fi
	atf_check_equal $rv 0
}

atf_test_case update_if_installed

update_if_installed_head() {
	atf_set "descr" "Tests for pkg installations: update if installed (issue #35)"
}

update_if_installed_body() {
	mkdir some_repo
	mkdir -p pkg_A/usr/bin
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0

	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -y A
	atf_check_equal $? 0

	cd some_repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0

	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -y A
	atf_check_equal $? 0
	pkgver=$(xbps-query -r root -ppkgver A)
	atf_check_equal $pkgver A-1.1_1
}

atf_test_case install_if_not_installed_on_update

install_if_not_installed_on_update_head() {
	atf_set "descr" "Tests for pkg installations: install if not installed on update (issue #35)"
}

install_if_not_installed_on_update_body() {
	mkdir some_repo
	mkdir -p pkg_A/usr/bin
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0

	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -yu A
	atf_check_equal $? 0

	pkgver=$(xbps-query -r root -ppkgver A)
	atf_check_equal $pkgver A-1.0_1
}

atf_test_case install_dups

install_dups_head() {
	atf_set "descr" "Tests for pkg installations: install multiple times a pkg"
}

install_dups_body() {
	mkdir some_repo
	mkdir -p pkg_A/usr/bin
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0

	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	out=$(xbps-install -C empty.conf -r root --repository=$PWD/some_repo -ynd A A A A|wc -l)
	atf_check_equal $out 1
}

atf_test_case install_bestmatch

install_bestmatch_head() {
	atf_set "descr" "Tests for pkg installations: install with bestmatching enabled"
}

install_bestmatch_body() {
	mkdir -p repo repo2 pkg_A/usr/bin
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ../repo2
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0

	cd ..
	mkdir -p root/xbps.d
	echo "bestmatching=true" > root/xbps.d/bestmatch.conf
	xbps-install -C xbps.d -r root --repository=$PWD/repo --repository=$PWD/repo2 -yvd A
	atf_check_equal $? 0
	out=$(xbps-query -r root -p pkgver A)
	atf_check_equal $out A-1.1_1
}

atf_test_case install_bestmatch_deps

install_bestmatch_deps_head() {
	atf_set "descr" "Tests for pkg installations: install with bestmatching enabled for deps"
}

install_bestmatch_deps_body() {
	mkdir -p repo repo2 pkg_A/usr/bin pkg_B/usr/bin
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "A>=0" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ../repo2
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.1_1 -s "B pkg" --dependencies "A>=0" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0

	cd ..
	mkdir -p root/xbps.d
	echo "bestmatching=true" > root/xbps.d/bestmatch.conf
	xbps-install -C xbps.d -r root --repository=$PWD/repo --repository=$PWD/repo2 -yvd B
	atf_check_equal $? 0
	out=$(xbps-query -r root -p pkgver A)
	atf_check_equal $out A-1.1_1
	out=$(xbps-query -r root -p pkgver B)
	atf_check_equal $out B-1.1_1
}

atf_test_case install_bestmatch_disabled

install_bestmatch_disabled_head() {
	atf_set "descr" "Tests for pkg installations: install with bestmatching disabled"
}

install_bestmatch_disabled_body() {
	mkdir -p repo repo2 pkg_A/usr/bin
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ../repo2
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0

	cd ..
	mkdir -p root/xbps.d
	echo "bestmatching=false" > root/xbps.d/bestmatch.conf
	xbps-install -C xbps.d -r root --repository=$PWD/repo --repository=$PWD/repo2 -yvd A
	atf_check_equal $? 0
	out=$(xbps-query -r root -p pkgver A)
	atf_check_equal $out A-1.0_1
}

atf_test_case update_file_timestamps

update_file_timestamps_head() {
	atf_set "descr" "Test for pkg updates: update file timestamps"
}

update_file_timestamps_body() {
	mkdir -p repo pkg_A/usr/include/gsm
	echo 123456789 > pkg_A/usr/include/gsm/gsm.h

	cd repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	cd ..
	xbps-rindex -d -a repo/*.xbps
	atf_check_equal $? 0
	xbps-install -r root --repository=repo -yvd foo

	expected=$(stat --printf='%Y' pkg_A/usr/include/gsm/gsm.h)
	result=$(stat --printf='%Y' root/usr/include/gsm/gsm.h)

	atf_check_equal "$expected" "$result"

	sleep 2
	cd repo
	touch -f pkg_A/usr/include/gsm/gsm.h
	xbps-create -A noarch -n foo-1.1_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	cd ..
	xbps-rindex -d -a repo/*.xbps
	atf_check_equal $? 0
	xbps-install -r root --repository=repo -yuvd foo

	expected=$(stat --printf='%Y' pkg_A/usr/include/gsm/gsm.h)
	result=$(stat --printf='%Y' root/usr/include/gsm/gsm.h)

	atf_check_equal "$expected" "$result"
}

atf_init_test_cases() {
	atf_add_test_case install_empty
	atf_add_test_case install_with_deps
	atf_add_test_case install_with_vpkg_deps
	atf_add_test_case install_if_not_installed_on_update
	atf_add_test_case install_dups
	atf_add_test_case install_bestmatch
	atf_add_test_case install_bestmatch_deps
	atf_add_test_case install_bestmatch_disabled
	atf_add_test_case update_if_installed
	atf_add_test_case update_to_empty_pkg
	atf_add_test_case update_file_timestamps
}
