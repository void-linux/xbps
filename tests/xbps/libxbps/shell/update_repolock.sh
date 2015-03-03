#!/usr/bin/env atf-sh

atf_test_case update_repolock

update_repolock_head() {
	atf_set "descr" "Tests for pkg update: pkg is in repository locked mode"
}

update_repolock_body() {
	mkdir -p repo repo2 pkg_A
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ../repo2
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	# install A-1.0_1 from repository "repo"
	xbps-install -r root --repository=repo2 --repository=repo -yvd A-1.0_1
	atf_check_equal $? 0
	# A-1.0_1 is now locked
	xbps-pkgdb -r root -m repolock A
	atf_check_equal $? 0
	out=$(xbps-query -r root -p repository A)
	atf_check_equal "$out" "$(readlink -f repo)"
	# no update due to repository locking
	xbps-install -r root --repository=repo2 --repository=repo -yuvd
	atf_check_equal $? 0
	out=$(xbps-query -r root -p pkgver A)
	atf_check_equal $out A-1.0_1
	# disable repolock
	xbps-pkgdb -r root -m repounlock A
	atf_check_equal $? 0
	out=$(xbps-query -r root -p repolock A)
	atf_check_equal "$out" ""
	# update A to 1.1_1 from repo2
	xbps-install -r root --repository=repo2 --repository=repo -yuvd
	atf_check_equal $? 0
	out=$(xbps-query -r root -p pkgver A)
	atf_check_equal $out A-1.1_1
	out=$(xbps-query -r root -p repository A)
	atf_check_equal "$out" "$(readlink -f repo2)"
}

atf_init_test_cases() {
	atf_add_test_case update_repolock
}
