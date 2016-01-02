#! /usr/bin/env atf-sh

atf_test_case install_existent

install_existent_head() {
	atf_set "descr" "xbps-install(8): install multiple existent pkgs (issue #53)"
}

install_existent_body() {
	mkdir -p some_repo pkg_A pkg_B
	touch pkg_A/file00
	touch pkg_B/file00
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.1_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -y A
	atf_check_equal $? 0
	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -y A B
	atf_check_equal $? 0

	rm -r root
	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -y A
	atf_check_equal $? 0
	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -y B A
	atf_check_equal $? 0
}

atf_test_case update_pkg_on_hold

update_pkg_on_hold_head() {
	atf_set "descr" "xbps-install(8): update packages on hold (issue #143)"
}

update_pkg_on_hold_body() {
	mkdir -p some_repo pkg-A pkg-B
	touch pkg-A/file00
	touch pkg-B/file00
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "pkg-A" ../pkg-A
	xbps-create -A noarch -n B-1.0_1 -s "pkg-B" ../pkg-B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -y A B
	atf_check_equal $? 0
	xbps-pkgdb -d -r root -m hold A
	atf_check_equal $? 0
	atf_check_equal "$(xbps-query -H -r root)" "A-1.0_1"
	cd some_repo
	rm *.xbps
	xbps-create -A noarch -n A-1.0_2 -s "pkg-A" ../pkg-A
	xbps-create -A noarch -n B-1.0_2 -s "pkg-B" ../pkg-B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -d -r root -C empty.conf --repository=$PWD/some_repo -y A
	atf_check_equal $? 0
}

atf_init_test_cases() {
	atf_add_test_case install_existent
	atf_add_test_case update_pkg_on_hold
}
