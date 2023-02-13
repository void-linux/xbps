#! /usr/bin/env atf-sh
# Test that xbps-rindex(1) -a (add mode) works as expected.

# 1st test: test that update mode work as expected.
atf_test_case update

update_head() {
	atf_set "descr" "xbps-rindex(1) -a: update test"
}

update_body() {
	mkdir -p some_repo pkg_A
	touch pkg_A/file00
	cd some_repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	xbps-create -A noarch -n foo-1.1_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	result="$(xbps-query -r root -C empty.conf --repository=some_repo -s '')"
	expected="[-] foo-1.1_1 foo pkg"
	rv=0
	if [ "$result" != "$expected" ]; then
		echo "result: $result"
		echo "expected: $expected"
		rv=1
	fi
	atf_check_equal $rv 0
}

atf_test_case revert

revert_head() {
	atf_set "descr" "xbps-rindex(1) -a: revert version test"
}

revert_body() {
	mkdir -p some_repo pkg_A
	touch pkg_A/file00
	cd some_repo
	xbps-create -A noarch -n foo-1.1_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	xbps-create -A noarch -n foo-1.0_1 -r "1.1_1" -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	result="$(xbps-query -r root -C empty.conf --repository=some_repo -s '')"
	expected="[-] foo-1.0_1 foo pkg"
	rv=0
	if [ "$result" != "$expected" ]; then
		echo "result: $result"
		echo "expected: $expected"
		rv=1
	fi
	atf_check_equal $rv 0
}

atf_test_case stage

stage_head() {
	atf_set "descr" "xbps-rindex(1) -a: commit package to stage test"
}

stage_body() {
	mkdir -p some_repo pkg_A pkg_B
	touch pkg_A/file00 pkg_B/file01
	cd some_repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" --shlib-provides "libfoo.so.1" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	[ -f *-stagedata ]
	atf_check_equal $? 1

	xbps-create -A noarch -n foo-1.1_1 -s "foo pkg" --shlib-provides "libfoo.so.2" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	[ -f *-stagedata ]
	atf_check_equal $? 1

	xbps-create -A noarch -n bar-1.0_1 -s "foo pkg" --shlib-requires "libfoo.so.2" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	[ -f *-stagedata ]
	atf_check_equal $? 1

	xbps-create -A noarch -n foo-1.2_1 -s "foo pkg" --shlib-provides "libfoo.so.3" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	[ -f *-stagedata ]
	atf_check_equal $? 0

	xbps-create -A noarch -n bar-1.1_1 -s "foo pkg" --shlib-requires "libfoo.so.3" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	[ -f *-stagedata ]
	atf_check_equal $? 1
}

atf_test_case stage_resolve_bug

stage_resolve_bug_head() {
	atf_set "descr" "xbps-rindex(1) -a: commit package to stage test"
}

stage_resolve_bug_body() {
	# Scanario: provides a shlib, then it is moved (for example for splitting the
	# pkg) to libprovider. afterwards requirer is added resulting in
	mkdir provider libprovider requirer stage-trigger some_repo
	touch provider/1 libprovider/2 requirer/3 stage-trigger/4
	cd some_repo

	# first add the provider and the requirer to the repo
	xbps-create -A noarch -n provider-1.0_1 -s "foo pkg" --shlib-provides "libfoo.so.1 libbar.so.1" ../provider
	atf_check_equal $? 0
	xbps-create -A noarch -n require-1.0_1 -s "foo pkg" --shlib-requires "libfoo.so.1" ../requirer
	atf_check_equal $? 0
	xbps-create -A noarch -n stage-trigger-1.0_1 -s "foo pkg" --shlib-requires "libbar.so.1" ../stage-trigger
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0

	# then add libprovider that also provides the library
	xbps-create -A noarch -n libprovider-1.0_2 -s "foo pkg" --shlib-provides "libfoo.so.1" ../libprovider
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	[ -f *-stagedata ]
	atf_check_equal $? 1

	# trigger staging
	xbps-create -A noarch -n provider-1.0_2 -s "foo pkg" --shlib-provides "libfoo.so.1" ../provider
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	[ -f *-stagedata ]
	atf_check_equal $? 0

	# then add a new provider not containing the provides field. This resulted in
	# a stage state despites the library is resolved through libprovides
	xbps-create -A noarch -n provider-1.0_3 -s "foo pkg" ../provider
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	[ -f *-stagedata ]
	atf_check_equal $? 0

	# resolve staging
	# the actual bug appeared here: libfoo.so.1 is still provided by libprovider, but
	# xbps-rindex fails to register that.
	xbps-create -A noarch -n stage-trigger-1.0_2 -s "foo pkg" ../stage-trigger
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	[ -f *-stagedata ]
	atf_check_equal $? 1
}

atf_test_case abi_mismatch

abi_mismatch_head() {
	atf_set "descr" "xbps-rindex(1) -a: don't index package with different abi"
}

abi_mismatch_body() {
	mkdir -p some_repo pkg_A pkg_B
	touch pkg_A/file00 pkg_B/file01
	cd some_repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n bar-1.1_1 -s "foo pkg" -a 1 ../pkg_B
	atf_check_equal $? 0

	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0

	cd ..
	n="$(xbps-query -r root --repository=some_repo -s "" | wc -l)"
	atf_check_equal $n 1
}

atf_test_case abi_mismatch_2

abi_mismatch_2_head() {
	atf_set "descr" "xbps-rindex(1) -a: don't index package with different abi"
}

abi_mismatch_2_body() {
	mkdir -p some_repo pkg_A pkg_B
	touch pkg_A/file00 pkg_B/file01
	cd some_repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" -a 2 ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n bar-1.1_1 -s "foo pkg" -a 1 ../pkg_B
	atf_check_equal $? 0

	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0

	cd ..
	n="$(xbps-query -r root --repository=some_repo -s "" | wc -l)"
	atf_check_equal $n 1
}

atf_test_case abi_match

abi_match_head() {
	atf_set "descr" "xbps-rindex(1) -a: index packages with matching abi"
}

abi_match_body() {
	mkdir -p some_repo pkg_A pkg_B
	touch pkg_A/file00 pkg_B/file01
	cd some_repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" -a 1 ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n bar-1.1_1 -s "foo pkg" -a 1 ../pkg_B
	atf_check_equal $? 0

	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0

	cd ..
	n="$(xbps-query -r root --repository=some_repo -s "" | wc -l)"
	atf_check_equal $n 2
}

atf_init_test_cases() {
	atf_add_test_case update
	atf_add_test_case revert
	atf_add_test_case stage
	atf_add_test_case stage_resolve_bug
	atf_add_test_case abi_mismatch
	atf_add_test_case abi_mismatch_2
	atf_add_test_case abi_match
}
