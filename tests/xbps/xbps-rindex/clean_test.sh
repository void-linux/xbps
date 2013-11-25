#! /usr/bin/env atf-sh
# Test that xbps-rindex(8) -c (clean mode) works as expected.

# 1st test: make sure that nothing is removed if there are no changes.
atf_test_case noremove

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

# 2nd test: make sure that entries are also removed from index-files.
atf_test_case filesclean

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

# 3nd test: make sure that entries are removed from index-files on updates.
atf_test_case filesclean2

filesclean2_head() {
	atf_set "descr" "xbps-rindex(8) -c: index-files clean test on updates"
}

filesclean2_body() {
	mkdir -p some_repo pkg_A
	touch -f pkg_A/file00
	cd some_repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex  -a *.xbps
	atf_check_equal $? 0
	xbps-create -A noarch -n foo-1.1_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex  -a *.xbps
	atf_check_equal $? 0
	cd ..
	xbps-rindex -c some_repo
	atf_check_equal $? 0
	result="$(xbps-query --repository=some_repo -o \*)"
	expected="foo-1.1_1: /file00 (some_repo)"
	rv=0
	if [ "$result" != "$expected" ]; then
		rv=1
	fi
	atf_check_equal $rv 0

}

# 4th test: xbps issue #19.
# How to reproduce it:
#	Generate pkg foo-1.0_1.
#	Add it to the index of a local repo.
#	Remove the pkg file from the repo.
#	Run xbps-rindex -c on the repo.

atf_test_case issue19

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

atf_init_test_cases() {
	atf_add_test_case noremove
	atf_add_test_case filesclean
	atf_add_test_case filesclean2
	atf_add_test_case issue19
}
