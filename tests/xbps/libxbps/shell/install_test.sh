#!/usr/bin/env atf-sh

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

	xbps-rindex -a *.xbps
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

	xbps-rindex -a *.xbps
	atf_check_equal $? 0
	cd ..

	echo -e "D-1.0_1\nC-1.0_1\nA-1.0_1\nB-1.0_1\n" > exp
	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -yn E|awk '{print $1}' > out
	echo >> out
	echo "exp: '$(cat exp)'" >&2
	echo "out: '$(cat out)'" >&2
	cmp exp out
	atf_check_equal $? 0
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

	xbps-rindex -a *.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -y A
	atf_check_equal $? 0

	cd some_repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0

	xbps-rindex -a *.xbps
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

	xbps-rindex -a *.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -yu A
	atf_check_equal $? 0

	pkgver=$(xbps-query -r root -ppkgver A)
	atf_check_equal $pkgver A-1.0_1
}

atf_init_test_cases() {
	atf_add_test_case install_with_deps
	atf_add_test_case install_with_vpkg_deps
	atf_add_test_case install_if_not_installed_on_update
	atf_add_test_case update_if_installed
}
