#! /usr/bin/env atf-sh
# Test that xbps-rindex(1) -R (remove mode) works as expected.

atf_test_case remove_same

remove_same_head() {
	atf_set "descr" "xbps-rindex(1) -R: test removing indexed version"
}

remove_same_body() {
	mkdir -p some_repo pkg_A pkg_B pkg_C
	cd some_repo
	xbps-create -A noarch -n foo-a-1.0_1 -s "foo pkg a" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n foo-b-1.0_1 -s "foo pkg b" ../pkg_B
	atf_check_equal $? 0
	xbps-create -A noarch -n foo-c-1.0_1 -s "foo pkg c" ../pkg_C
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	result="$(xbps-query -r root -C empty.conf --repository=some_repo -s '')"
	expected="[-] foo-a-1.0_1 foo pkg a
[-] foo-b-1.0_1 foo pkg b
[-] foo-c-1.0_1 foo pkg c"
	rv=0
	if [ "$result" != "$expected" ]; then
		echo "result: $result"
		echo "expected: $expected"
		rv=1
	fi
	atf_check_equal $rv 0
	xbps-rindex -R $PWD/some_repo/foo-a-1.0_1.noarch.xbps $PWD/some_repo/foo-c-1.0_1.noarch.xbps
	atf_check_equal $? 0
	result="$(xbps-query -r root -C empty.conf --repository=some_repo -s '')"
	expected="[-] foo-b-1.0_1 foo pkg b"
	rv=0
	if [ "$result" != "$expected" ]; then
		echo "result: $result"
		echo "expected: $expected"
		rv=1
	fi
	atf_check_equal $rv 0
}

atf_test_case remove_newer

remove_newer_head() {
	atf_set "descr" "xbps-rindex(1) -R: test removing newer version"
}

remove_newer_body() {
	mkdir -p some_repo pkg_A-2 pkg_A-1.0
	cd some_repo
	xbps-create -A noarch -n foo-a-2_1 -s "foo pkg a" ../pkg_A-2
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	result="$(xbps-query -r root -C empty.conf --repository=some_repo -s '')"
	expected="[-] foo-a-2_1 foo pkg a"
	rv=0
	if [ "$result" != "$expected" ]; then
		echo "result: $result"
		echo "expected: $expected"
		rv=1
	fi
	atf_check_equal $rv 0
	cd some_repo
	xbps-create -A noarch -n foo-a-1.0_1 -s "foo pkg a" ../pkg_A-1.0
	atf_check_equal $? 0
	cd ..
	xbps-rindex -R $PWD/some_repo/foo-a-1.0_1.noarch.xbps
	atf_check_equal $? 0
	result="$(xbps-query -r root -C empty.conf --repository=some_repo -s '')"
	expected=""
	rv=0
	if [ "$result" != "$expected" ]; then
		echo "result: $result"
		echo "expected: $expected"
		rv=1
	fi
	atf_check_equal $rv 0
}


atf_test_case remove_not_indexed

remove_not_indexed_head() {
	atf_set "descr" "xbps-rindex(1) -R: force remove test"
}

remove_not_indexed_body() {
	mkdir -p some_repo pkg_A
	cd some_repo
	xbps-create -A noarch -n foo-a-1.0_1 -s "foo pkg a" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	result="$(xbps-query -r root -C empty.conf --repository=some_repo -s '')"
	expected="[-] foo-a-1.0_1 foo pkg a"
	rv=0
	if [ "$result" != "$expected" ]; then
		echo "result: $result"
		echo "expected: $expected"
		rv=1
	fi
	atf_check_equal $rv 0
	xbps-rindex -R $PWD/some_repo/removed-earlier-1_1.noarch.xbps
	atf_check_equal $? 0
	result="$(xbps-query -r root -C empty.conf --repository=some_repo -s '')"
	rv=0
	if [ "$result" != "$expected" ]; then
		echo "result: $result"
		echo "expected: $expected"
		rv=1
	fi
	atf_check_equal $rv 0
}

atf_init_test_cases() {
	atf_add_test_case remove_same
	atf_add_test_case remove_newer
	atf_add_test_case remove_not_indexed
}
