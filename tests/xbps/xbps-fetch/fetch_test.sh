#! /usr/bin/env atf-sh
# Test that xbps-fetch works as expected.

atf_test_case success

success_head() {
	atf_set "descr" "xbps-fetch: test successful remote fetch"
}

success_body() {
	mkdir some_repo
	touch some_repo/pkg_A
	atf_check -o ignore -- xbps-fetch file://$PWD/some_repo/pkg_A
}

atf_test_case pkgnotfound

pkgnotfound_head() {
	atf_set "descr" "xbps-fetch: test remote package not found"
}

pkgnotfound_body() {
	atf_check -s exit:1 \
		-e match:"ERROR: xbps-fetch: failed to fetch: file://${PWD}/nonexistant" \
		-- xbps-fetch file://$PWD/nonexistant
}

atf_test_case identical

identical_head() {
	atf_set "descr" "xbps-fetch: test fetching identical file from remote"
}

identical_body() {
	mkdir some_repo
	echo 'content' > some_repo/pkg_A
	echo 'content' > pkg_A
	atf_check \
		-e inline:"WARNING: xbps-fetch: file://$PWD/some_repo/pkg_A: file is identical with remote.\n" \
		-- xbps-fetch file://$PWD/some_repo/pkg_A
}

atf_test_case multiple_success

multiple_success_head() {
	atf_set "descr" "xbps-fetch: test fetching multiple remote files"
}

multiple_success_body() {
	mkdir some_repo
	touch some_repo/pkg_A some_repo/pkg_B
	atf_check -o ignore -- xbps-fetch file://$PWD/some_repo/pkg_A file://$PWD/some_repo/pkg_B
	atf_check -- test -f pkg_A
	atf_check -- test -f pkg_B
}

atf_test_case multiple_notfound

multiple_notfound_head() {
	atf_set "descr" "xbps-fetch: test fetching multiple remote files, with one not found"
}

multiple_notfound_body() {
	mkdir some_repo
	touch some_repo/pkg_A
	atf_check -s exit:1 -e match:"failed to fetch" -o ignore \
		-- xbps-fetch file://$PWD/some_repo/nonexistant file://$PWD/some_repo/pkg_A
	atf_check -- test -f pkg_A
	atf_check -s exit:1 -- test -f nonexistant
}

atf_init_test_cases() {
	atf_add_test_case success
	atf_add_test_case pkgnotfound
	atf_add_test_case identical
	atf_add_test_case multiple_success
	atf_add_test_case multiple_notfound
}
