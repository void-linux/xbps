#! /usr/bin/env atf-sh

# 1st test: make sure that base symlinks on rootdir are not removed.
atf_test_case keep_base_symlinks

keep_base_symlinks_head() {
	atf_set "descr" "Tests for package removal: keep base symlinks test"
}

keep_base_symlinks_body() {
	mkdir -p root/usr/bin
	ln -sfr root/usr/bin root/bin

	mkdir some_repo
	mkdir -p pkg_A/usr/bin pkg_A/bin
	touch -f pkg_A/usr/bin/foo
	cd some_repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -a *.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root --repository=some_repo -y foo
	atf_check_equal $? 0
	xbps-remove -r root -y foo
	atf_check_equal $? 0
	if [ -h root/bin ]; then
		rv=0
	else
		rv=1
	fi
	atf_check_equal $rv 0
}

atf_init_test_cases() {
	atf_add_test_case keep_base_symlinks
}
