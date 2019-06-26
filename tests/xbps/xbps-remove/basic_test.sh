#! /usr/bin/env atf-sh

atf_test_case remove_directory

remoe_directory_head() {
	atf_set "descr" "xbps-remove(1): remove nested directories"
}

remove_directory_body() {
	mkdir -p some_repo pkg_A/B/C
	touch pkg_A/B/C/file00
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -y A
	atf_check_equal $? 0
	xbps-remove -r root -C empty.conf -y A
	atf_check_equal $? 0
	test -d root/B
	atf_check_equal $? 1
}

atf_test_case remove_orphans

remove_orphans_head() {
	atf_set "descr" "xbps-remove(1): remove orphaned packages"
}

remove_orphans_body() {
	mkdir -p some_repo pkg_A/B/C
	touch pkg_A/
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -yA A
	atf_check_equal $? 0
	xbps-remove -r root -C empty.conf -yvdo
	atf_check_equal $? 0
	xbps-query A
	atf_check_equal $? 2
}

atf_init_test_cases() {
	atf_add_test_case remove_directory
	atf_add_test_case remove_orphans
}
