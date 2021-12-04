#! /usr/bin/env atf-sh
# Test that xbps-query(1) list modes work as expected

atf_test_case list_repos

list_repos_head() {
	atf_set "descr" "xbps-query(1) -L"
}

list_repos_body() {
	mkdir -p some_repo pkg_A/bin
	touch pkg_A/bin/file
	cd some_repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n baz-1.0_1 -s "baz pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	rm -f *.xbps
	cd ..
	output="$(xbps-query -C empty.conf -i --repository=some_repo --repository=vanished_repo -L | tr -d '\n')"
	atf_check_equal "$output" "    2 ${PWD}/some_repo (RSA unsigned)	-1 vanished_repo (RSA maybe-signed)"
}

atf_init_test_cases() {
	atf_add_test_case list_repos
}
