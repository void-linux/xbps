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
	atf_check -o ignore -- xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" --shlib-provides "foo.so.1" ../pkg_A
	atf_check -o ignore -- xbps-create -A noarch -n bar-1.0_1 -s "foo pkg" --shlib-requires "foo.so.1" ../pkg_B
	atf_check -o ignore -- xbps-create -A noarch -n deleteme-1.0_1 -s "deleteme pkg" ../pkg_B
	atf_check -o ignore -e ignore -- xbps-rindex -a $PWD/*.xbps
	atf_check -o ignore -- xbps-create -A noarch -n deleteme-1.1_1 -s "deleteme pkg" ../pkg_B
	atf_check -o ignore -e ignore -- xbps-rindex -a $PWD/*.xbps
	atf_check -o ignore -- xbps-create -A noarch -n foo-1.1_1 -s "foo pkg" --shlib-provides "foo.so.2" ../pkg_A
	atf_check -o ignore -e ignore -- xbps-rindex -a $PWD/*.xbps
	cd ..
	atf_check -o inline:"    3 $PWD/some_repo (Staged) (RSA unsigned)\n" -- \
		xbps-query -r ../root -i --repository=some_repo -L
	atf_check -o inline:"Removed obsolete package \`deleteme-1.0_1.noarch.xbps'.\n" -- xbps-rindex -r some_repo
	[ -f some_repo/foo-1.0_1* ] || atf_fail "foo-1.0_1 doesn't exist"
	[ -f some_repo/foo-1.1_1* ] || atf_fail "foo-1.1_1 doesn't exist"
	[ -f some_repo/bar-1.0_1* ] || atf_fail "bar-1.0_1 doesn't exist"
	[ -f some_repo/deleteme-1.0_1* ] && atf_fail "deleteme-1.0_1 still exists"
	[ -f some_repo/deleteme-1.1_1* ] || atf_fail "deleteme-1.1_1 doesn't exist"
}

atf_init_test_cases() {
	atf_add_test_case noremove_stage
}
