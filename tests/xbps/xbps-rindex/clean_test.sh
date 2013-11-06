#! /usr/bin/env atf-sh
# Test that xbps-rindex(8) -c (clean mode) works as expected.

common_cleanup() {
	rm -rf some_repo
}

# 1st test: make sure that nothing is removed if there are no changes.
atf_test_case noremove cleanup

noremove_head() {
	atf_set "descr" "xbps-rindex(8) -c: dont removing anything test"
}

noremove_body() {
	mkdir -p some_repo pkg_A
	cd some_repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -a *.xbps
	atf_check_equal $? 0
	cd ..
	xbps-rindex -c some_repo
	atf_check_equal $? 0
	result=$(xbps-query --repository=some_repo -s foo|wc -l)
	atf_check_equal ${result} 1
}

noremove_cleanup() {
	common_cleanup
}

# 2nd test: make sure that entries are also removed from index-files.
atf_test_case filesclean cleanup

filesclean_head() {
	atf_set "descr" "xbps-rindex(8) -c: index-files clean test"
}

filesclean_body() {
	mkdir -p some_repo pkg_A
	touch -f pkg_A/file00
	cd some_repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex  -a *.xbps
	atf_check_equal $? 0
	rm *.xbps
	cd ..
	xbps-rindex -c some_repo
	atf_check_equal $? 0
	result=$(xbps-query --repository=some_repo -o \*)
	test -z "${result}"
	atf_check_equal $? 0
}

filesclean_cleanup() {
	common_cleanup
}

# 3rd test: xbps issue #19.
# How to reproduce it:
#	Generate pkg foo-1.0_1.
#	Add it to the index of a local repo.
#	Remove the pkg file from the repo.
#	Run xbps-rindex -c on the repo.

atf_test_case issue19 cleanup

issue19_head() {
	atf_set "descr" "xbps issue #19 (https://github.com/xtraeme/xbps/issues/19)"
}

issue19_body() {
	mkdir -p some_repo
	cd some_repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" .
	atf_check_equal $? 0
	xbps-rindex -a *.xbps
	atf_check_equal $? 0
	cd ..
	rm some_repo/*.xbps
	xbps-rindex -c some_repo
	atf_check_equal $? 0
	result=$(xbps-query --repository=some_repo -s foo)
	test -z "${result}"
	atf_check_equal $? 0
}

issue19_cleanup() {
	common_cleanup
}

atf_init_test_cases() {
	atf_add_test_case noremove
	atf_add_test_case filesclean
	atf_add_test_case issue19
}
