#! /usr/bin/env atf-sh

# xbps issue #19.
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

issue19_cleanup() {
	rm -rf some_repo
}

atf_init_test_cases() {
	atf_add_test_case issue19
}
