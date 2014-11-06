#! /usr/bin/env atf-sh
# Test that xbps-query(8) remote modes work as expected

# 1st test: test that -Rf does not segfault when binpkg is not available
atf_test_case remote_files

remote_files_head() {
	atf_set "descr" "xbps-query(8) -Rf: binpkg files test"
}

remote_files_body() {
	mkdir -p some_repo pkg_A/bin
	touch pkg_A/bin/file
	cd some_repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	rm -f *.xbps
	cd ..
	xbps-query -C empty.conf --repository=some_repo -f foo-1.0_1
	# ENOENT is ok because binpkg does not exist
	atf_check_equal $? 2
}

atf_init_test_cases() {
	atf_add_test_case remote_files
}
