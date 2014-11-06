#! /usr/bin/env atf-sh
# Test that xbps-rindex(8) -a (add mode) works as expected.

# 1st test: test that update mode work as expected.
atf_test_case update

update_head() {
	atf_set "descr" "xbps-rindex(8) -a: update test"
}

update_body() {
	mkdir -p some_repo pkg_A
	touch pkg_A/file00
	cd some_repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a *.xbps
	atf_check_equal $? 0
	xbps-create -A noarch -n foo-1.1_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a *.xbps
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

revert_head() {
	atf_set "descr" "xbps-rindex(8) -a: revert version test"
}

revert_body() {
	mkdir -p some_repo pkg_A
	touch pkg_A/file00
	cd some_repo
	xbps-create -A noarch -n foo-1.1_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a *.xbps
	atf_check_equal $? 0
	xbps-create -A noarch -n foo-1.0_1 -r "1.1_1" -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a *.xbps
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

atf_init_test_cases() {
	atf_add_test_case update
	atf_add_test_case revert
}
