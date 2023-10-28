#! /usr/bin/env atf-sh
# Test that xbps-rindex(1) -c (clean mode) works as expected.

# 1st test: make sure that nothing is removed if there are no changes.
atf_test_case noremove

noremove_head() {
	atf_set "descr" "xbps-rindex(1) -c: dont removing anything test"
}

noremove_body() {
	mkdir -p some_repo pkg_A
	cd some_repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-rindex -c some_repo
	atf_check_equal $? 0
	result=$(xbps-query -r root -C empty.conf --repository=some_repo -s foo|wc -l)
	atf_check_equal ${result} 1
}

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
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	rm some_repo/*.xbps
	xbps-rindex -c some_repo
	atf_check_equal $? 0
	result=$(xbps-query -r root -C empty.conf --repository=some_repo -s foo)
	test -z "${result}"
	atf_check_equal $? 0
}

atf_test_case remove_from_stage

remove_from_stage_head() {
	atf_set "descr" "xbps-rindex(1) -r: don't removing if there's staging test"
}

remove_from_stage_body() {
	mkdir -p some_repo pkg_A pkg_B
	cd some_repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" --shlib-provides "foo.so.1" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n bar-1.0_1 -s "foo pkg" --shlib-requires "foo.so.1" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	xbps-create -A noarch -n foo-1.1_1 -s "foo pkg" --shlib-provides "foo.so.2" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check -o inline:"    2 $PWD (Staged) (RSA unsigned)\n" -- \
		xbps-query -r ../root -i --repository=$PWD -L
	atf_check_equal $? 0
	rm foo-1.1_1*
	xbps-rindex -c .
	atf_check -o inline:"    1 $PWD (RSA unsigned)\n" -- \
		xbps-query -r ../root -i --repository=$PWD -L
	cd ..
}

atf_init_test_cases() {
	atf_add_test_case noremove
	atf_add_test_case issue19
	atf_add_test_case remove_from_stage
}
