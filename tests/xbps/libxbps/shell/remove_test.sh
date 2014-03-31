#! /usr/bin/env atf-sh

# 1st test: make sure that base symlinks on rootdir are not removed.
atf_test_case keep_base_symlinks

keep_base_symlinks_head() {
	atf_set "descr" "Tests for package removal: keep base symlinks test"
}

keep_base_symlinks_body() {
	mkdir -p root/usr/bin root/usr/lib root/run root/var
	ln -sfr root/usr/bin root/bin
	ln -sfr root/usr/lib root/lib
	ln -sfr root/usr/lib root/usr/lib32
	ln -sfr root/usr/lib root/usr/lib64
	ln -sfr root/run root/var/run

	mkdir some_repo
	mkdir -p pkg_A/usr/bin pkg_A/bin pkg_A/usr/lib pkg_A/var
	touch -f pkg_A/usr/bin/foo
	ln -sfr pkg_A/usr/lib pkg_A/usr/lib32
	ln -sfr pkg_A/usr/lib pkg_A/usr/lib64
	ln -sfr /run pkg_A/var/run

	cd some_repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -a *.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root -C null.conf --repository=$PWD/some_repo -y foo
	atf_check_equal $? 0
	xbps-remove -r root -y foo
	atf_check_equal $? 0
	if [ -h root/bin -a -h root/lib -a -h root/usr/lib32 -a -h root/usr/lib64 -a -h root/var/run ]; then
		rv=0
	else
		rv=1
	fi
	atf_check_equal $rv 0
}

# 2nd test: make sure that all symlinks are removed.
atf_test_case remove_symlinks

remove_symlinks_head() {
	atf_set "descr" "Tests for package removal: symlink cleanup test"
}

remove_symlinks_body() {
	mkdir some_repo
	mkdir -p pkg_A/usr/lib pkg_B/usr/lib
	touch -f pkg_A/usr/lib/libfoo.so.1.2.0
	ln -sfr pkg_A/usr/lib/libfoo.so.1.2.0 pkg_A/usr/lib/libfoo.so.1
	ln -sfr pkg_B/usr/lib/libfoo.so.1 pkg_B/usr/lib/libfoo.so

	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 --dependencies "A>=0" -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -a *.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root -C null.conf --repository=$PWD/some_repo -y B
	atf_check_equal $? 0
	xbps-pkgdb -r root -m manual A
	atf_check_equal $? 0
	xbps-remove -r root -Ryv B
	atf_check_equal $? 0
	rv=0
	if [ -h root/usr/lib/libfoo.so ]; then
	        rv=1
	fi
	atf_check_equal $rv 0
}

# 3rd test: make sure that symlinks to the rootfs are also removed.
atf_test_case remove_symlinks_from_root

remove_symlinks_from_root_head() {
	atf_set "descr" "Tests for package removal: symlink from root test"
}

remove_symlinks_from_root_body() {
	mkdir some_repo
	mkdir -p pkg_A/bin
	ln -sf /bin/bash pkg_A/bin/bash
	ln -sf /bin/bash pkg_A/bin/sh

	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -a *.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root -C null.conf --repository=$PWD/some_repo -y A
	atf_check_equal $? 0
	xbps-remove -r root -Ryv A
	atf_check_equal $? 0
	rv=0
	if [ -h root/bin/bash -o -h root/bin/sh ]; then
	        rv=1
	fi
	atf_check_equal $rv 0
}

atf_init_test_cases() {
	atf_add_test_case keep_base_symlinks
	atf_add_test_case remove_symlinks
	atf_add_test_case remove_symlinks_from_root
}
