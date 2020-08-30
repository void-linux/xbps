#! /usr/bin/env atf-sh
# Test that xbps-digest(1) works as expected.

atf_test_case empty_string

empty_string_head() {
	atf_set "descr" "xbps-digest(1): check empty string hash"
}

empty_string_body() {
	result="$(printf "" | xbps-digest)"
	expected=e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
	rv=0
	if [ "$result" != "$expected" ]; then
		echo "result: $result"
		echo "expected: $expected"
		rv=1
	fi
	atf_check_equal $rv 0
}

atf_test_case small_file

small_file_head() {
	atf_set "descr" "xbps-digest(1): check small file hash"
}

small_file_body() {
	printf "abc\nbca" > file
	result="$(xbps-digest file)"
	expected="36749ea1445c9fcb405767cbf67ebb4679dd4f7560a3b5fa977bc288fe15f999"
	rv=0
	if [ "$result" != "$expected" ]; then
		echo "result: $result"
		echo "expected: $expected"
		rv=1
	fi
	atf_check_equal $rv 0
}

atf_init_test_cases() {
	atf_add_test_case empty_string
	atf_add_test_case small_file
}
