#!/usr/bin/env atf-sh

atf_test_case repo_close

repo_close_head() {
	atf_set "descr" "Tests for pkg repos: truncate repo size to 0"
}

repo_close_body() {
	mkdir -p repo pkg_A
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -C empty.conf -r root --repository=repo -yn A
	atf_check_equal $? 0
	truncate --size 0 repo/*-repodata
	xbps-install -C empty.conf -r root --repository=repo -yn A
	# ENOENT because invalid repodata
	atf_check_equal $? 2
}

atf_init_test_cases() {
	atf_add_test_case repo_close
}
