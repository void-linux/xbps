#! /usr/bin/env atf-sh
# Test that xbps-rindex(1) -c (clean mode) works as expected.

# 1st test: make sure that nothing is removed if there are no changes.
atf_test_case noremove_stage

noremove_stage_head() {
	atf_set "descr" "xbps-rindex(1) -r: don't removing if there's staging test"
}

noremove_stage_body() {
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
	atf_check_equal $? 0
	atf_check -o inline:"    2 $PWD (Staged) (RSA unsigned)\n" -- \
		xbps-query -r ../root -i --repository=$PWD -L
	xbps-rindex -r some_repo
	atf_check_equal $? 0
	[ -f foo-1.0_1* ]
	atf_check_equal $? 0
	[ -f foo-1.1_1* ]
	atf_check_equal $? 0
	[ -f bar-1.0_1* ]
	atf_check_equal $? 0
}

atf_init_test_cases() {
	atf_add_test_case noremove_stage
}
