#! /usr/bin/env atf-sh
# Test that xbps-rindex(1) -p (purge mode) works as expected.

atf_test_case purge

purge_head() {
	atf_set "descr" "xbps-rindex(1) -p: purge indexed packages test"
}

purge_body() {
	mkdir -p some_repo pkg_A pkg_B pkg_C
	cd some_repo
	atf_check -o ignore -e ignore -- xbps-create -A noarch -n foo-a-1.0_1 -s "foo pkg a" ../pkg_A
	atf_check -o ignore -e ignore -- xbps-create -A noarch -n foo-b-1.0_1 -s "foo pkg b" ../pkg_B
	atf_check -o ignore -e ignore -- xbps-create -A noarch -n foo-c-1.0_1 -s "foo pkg c" ../pkg_C
	atf_check -o ignore -e ignore -- xbps-rindex -d -a $PWD/*.xbps
	atf_check -o ignore -- xbps-rindex -p $PWD/foo-a-1.0_1.noarch.xbps $PWD/foo-c-1.0_1.noarch.xbps
	cd ..
	atf_check -o "inline:[-] foo-b-1.0_1 foo pkg b\n" -- \
		xbps-query -r root -C empty.conf --repository=some_repo -s foo
}

atf_test_case purge_missing_file

purge_missing_file_head() {
	atf_set "descr" "xbps-rindex(1) -p: purge package without its binary package test"
}

purge_missing_file_body() {
	mkdir -p some_repo pkg_A
	cd some_repo
	atf_check -o ignore -e ignore -- xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check -o ignore -e ignore -- xbps-rindex -d -a $PWD/*.xbps
	rm foo-1.0_1.noarch.xbps
	atf_check -o ignore -- xbps-rindex -p $PWD/foo-1.0_1.noarch.xbps
	atf_check -o inline:"    0 $PWD (RSA unsigned)\n" -- \
		xbps-query -r ../root -i --repository=$PWD -L
}

atf_test_case purge_other_version

purge_other_version_head() {
	atf_set "descr" "xbps-rindex(1) -p: don't purge a different indexed version test"
}

purge_other_version_body() {
	mkdir -p some_repo pkg_A
	cd some_repo
	atf_check -o ignore -e ignore -- xbps-create -A noarch -n foo-1.1_1 -s "foo pkg" ../pkg_A
	atf_check -o ignore -e ignore -- xbps-rindex -d -a $PWD/*.xbps
	atf_check -e match:"not registered" -- xbps-rindex -p $PWD/foo-1.0_1.noarch.xbps
	cd ..
	atf_check -o "inline:[-] foo-1.1_1 foo pkg\n" -- \
		xbps-query -r root -C empty.conf --repository=some_repo -s foo
}

atf_test_case purge_staged

purge_staged_head() {
	atf_set "descr" "xbps-rindex(1) -p: purge staged package test"
}

purge_staged_body() {
	mkdir -p some_repo pkg_A pkg_B
	cd some_repo
	atf_check -o ignore -e ignore -- xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" --shlib-provides "foo.so.1" ../pkg_A
	atf_check -o ignore -e ignore -- xbps-create -A noarch -n bar-1.0_1 -s "bar pkg" --shlib-requires "foo.so.1" ../pkg_B
	atf_check -o ignore -e ignore -- xbps-rindex -d -a $PWD/*.xbps
	atf_check -o ignore -e ignore -- xbps-create -A noarch -n foo-1.1_1 -s "foo pkg" --shlib-provides "foo.so.2" ../pkg_A
	atf_check -o ignore -e ignore -- xbps-rindex -d -a $PWD/foo-1.1_1.noarch.xbps
	atf_check -o inline:"    2 $PWD (Staged) (RSA unsigned)\n" -- \
		xbps-query -r ../root -i --repository=$PWD -L
	atf_check -o ignore -- xbps-rindex -p $PWD/foo-1.1_1.noarch.xbps
	atf_check -o inline:"    2 $PWD (RSA unsigned)\n" -- \
		xbps-query -r ../root -i --repository=$PWD -L
	atf_check -o "inline:foo-1.0_1\n" -- \
		xbps-query -r ../root -i --repository=$PWD -R -p pkgver foo
}

atf_test_case purge_invalid

purge_invalid_head() {
	atf_set "descr" "xbps-rindex(1) -p: invalid argument purges nothing test"
}

purge_invalid_body() {
	mkdir -p some_repo pkg_A
	cd some_repo
	atf_check -o ignore -e ignore -- xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check -o ignore -e ignore -- xbps-rindex -d -a $PWD/*.xbps
	atf_check -s exit:1 -o ignore -e match:"not a binary package path" -- \
		xbps-rindex -p $PWD/foo-1.0_1.noarch.xbps $PWD/foo
	cd ..
	atf_check -o "inline:[-] foo-1.0_1 foo pkg\n" -- \
		xbps-query -r root -C empty.conf --repository=some_repo -s foo
}

atf_init_test_cases() {
	atf_add_test_case purge
	atf_add_test_case purge_missing_file
	atf_add_test_case purge_other_version
	atf_add_test_case purge_staged
	atf_add_test_case purge_invalid
}
