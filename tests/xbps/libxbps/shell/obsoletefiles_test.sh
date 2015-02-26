#! /usr/bin/env atf-sh

# 1- find obsolete files on reinstall (and downgrades).
atf_test_case reinstall_obsoletes

reinstall_obsoletes_update_head() {
	atf_set "descr" "Test that obsolete files are removed on reinstall"
}

reinstall_obsoletes_body() {
	#
	# Simulate a pkg downgrade and make sure that obsolete files
	# are found and removed properly.
	#
	mkdir some_repo
	mkdir -p pkg_A/usr/bin pkg_B/usr/sbin
	touch -f pkg_A/usr/bin/foo pkg_A/usr/bin/blah
	touch -f pkg_B/usr/sbin/baz

	cd some_repo
	xbps-create -A noarch -n A-1.1_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0

	cd ..
	xbps-install -r root -C null.conf --repository=$PWD/some_repo -yv A-1.1_1
	atf_check_equal $? 0

	rm -f some_repo/*
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "foo pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0

	cd ..
	xbps-install -r root -C null.conf --repository=$PWD/some_repo -yvf A-1.0_1
	atf_check_equal $? 0

	rv=0
	if [ ! -f root/usr/sbin/baz ]; then
		rv=1
	fi
	for f in usr/bin/foo usr/bin/blah; do
		if [ -f root/$f ]; then
			rv=1
		fi
	done
	atf_check_equal $rv 0
}

# 2- make sure that root symlinks aren't detected as obsoletes on upgrades.
atf_test_case root_symlinks_update

root_symlinks_update_head() {
	atf_set "descr" "Test that root symlinks aren't obsoletes on update"
}

root_symlinks_update_body() {
	mkdir repo
	mkdir -p pkg_A/usr/bin pkg_A/usr/sbin pkg_A/usr/lib pkg_A/var pkg_A/run
	touch -f pkg_A/usr/bin/foo
	ln -sf usr/bin pkg_A/bin
	ln -sf usr/sbin pkg_A/sbin
	ln -sf lib pkg_A/usr/lib32
	ln -sf lib pkg_A/usr/lib64
	ln -sf ../../run pkg_A/var/run

	cd repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	rm -rf ../pkg_A
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0

	xbps-install -r root -C null.conf --repository=$PWD -yd foo
	atf_check_equal $? 0

	cd ..
	mkdir -p pkg_A/usr/bin
	touch -f pkg_A/usr/bin/blah

	cd repo
	xbps-create -A noarch -n foo-1.1_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	rm -rf ../pkg_A
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0

	xbps-install -r root -C null.conf --repository=$PWD -yuvd foo
	atf_check_equal $? 0

	rv=1
	if [ -h root/bin -a -h root/sbin -a -h root/usr/lib32 -a -h root/usr/lib64 -a -h root/var/run ]; then
		rv=0
	fi
	ls -l root
	ls -l root/usr
	ls -l root/var
	atf_check_equal $rv 0
}

atf_test_case files_move_from_dependency

files_move_from_dependency_head() {
	atf_set "descr" "Test that moving files between a dependency to the main pkg works"
}

files_move_from_dependency_body() {
	mkdir repo
	mkdir -p pkg_A/usr/bin pkg_A/usr/sbin pkg_B/usr/sbin
	echo "0123456789" > pkg_A/usr/bin/foo
	echo "9876543210" > pkg_A/usr/sbin/blah
	echo "7777777777" > pkg_B/usr/sbin/bob

	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "A>=0" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0

	xbps-install -r root -C null.conf --repository=$PWD -yvd B
	atf_check_equal $? 0

	mv ../pkg_A/usr/bin ../pkg_B/usr
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.1_1 -s "B pkg" --dependencies "A>=1.1" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0

	xbps-install -r root -C null.conf --repository=$PWD -yuvd
	atf_check_equal $? 0

	foofile=$(xbps-query -r root -f A-1.1_1|grep foo)
	atf_check_equal $foofile ""

	foofile=$(xbps-query -r root -f B-1.1_1|grep foo)
	atf_check_equal $foofile /usr/bin/foo

	xbps-pkgdb -r root -av
	atf_check_equal $? 0
}

atf_test_case files_move_to_dependency

files_move_to_dependency_head() {
	atf_set "descr" "Test that moving files to a dependency works"
}

files_move_to_dependency_body() {
	mkdir repo
	mkdir -p pkg_libressl/usr/lib pkg_libcrypto/usr/lib
	echo "0123456789" > pkg_libressl/usr/lib/libcrypto.so.30
	echo "0123456789" > pkg_libcrypto/usr/lib/libcrypto.so.30

	cd repo
	xbps-create -A noarch -n libressl-1.0_1 -s "libressl pkg" ../pkg_libressl
	atf_check_equal $? 0
	xbps-create -A noarch -n libcrypto-1.0_1 -s "libcrypto pkg" --replaces "libressl<1.1_1" ../pkg_libcrypto
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0

	xbps-install -r root --repository=$PWD -yvd libressl
	atf_check_equal $? 0

	rm -rf ../pkg_libressl/*
	xbps-create -A noarch -n libressl-1.1_1 -s "libressl pkg" --dependencies "libcrypto>=1.0" ../pkg_libressl
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0

	xbps-install -r root --repository=$PWD -yuvd
	atf_check_equal $? 0

	foofile=$(xbps-query -r root -f libressl|grep crypto)
	atf_check_equal $foofile ""

	foofile=$(xbps-query -r root -f libcrypto|grep crypto)
	atf_check_equal $foofile /usr/lib/libcrypto.so.30

	xbps-pkgdb -r root -av
	atf_check_equal $? 0
}

atf_test_case files_move_to_dependency2

files_move_to_dependency2_head() {
	atf_set "descr" "Test that moving files to a dependency works (without replaces)"
}

files_move_to_dependency2_body() {
	mkdir repo
	mkdir -p pkg_libressl/usr/lib pkg_libcrypto/usr/lib
	echo "0123456789" > pkg_libressl/usr/lib/libcrypto.so.30
	echo "0123456789" > pkg_libressl/usr/lib/libssl.so.30
	echo "0123456789" > pkg_libcrypto/usr/lib/libcrypto.so.30

	cd repo
	xbps-create -A noarch -n libressl-1.0_1 -s "libressl pkg" ../pkg_libressl
	atf_check_equal $? 0
	xbps-create -A noarch -n libcrypto-1.0_1 -s "libcrypto pkg" ../pkg_libcrypto
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0

	xbps-install -r root --repository=$PWD -yvd libressl
	atf_check_equal $? 0

	sleep 1
	rm -f ../pkg_libressl/usr/lib/libcrypto.*
	touch -f ../pkg_libcrypto/usr/lib/libcrypto.so.30
	xbps-create -A noarch -n libcrypto-1.0_2 -s "libcrypto pkg" ../pkg_libcrypto
	atf_check_equal $? 0
	xbps-create -A noarch -n libressl-1.1_1 -s "libressl pkg" --dependencies "libcrypto>=1.0" ../pkg_libressl
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0

	xbps-install -r root --repository=$PWD -yuvd
	atf_check_equal $? 0

	foofile=$(xbps-query -r root -f libressl|grep crypto)
	atf_check_equal $foofile ""

	foofile=$(xbps-query -r root -f libcrypto|grep crypto)
	atf_check_equal $foofile /usr/lib/libcrypto.so.30

	xbps-pkgdb -r root -av
	atf_check_equal $? 0
}

atf_init_test_cases() {
	atf_add_test_case reinstall_obsoletes
	atf_add_test_case root_symlinks_update
	atf_add_test_case files_move_from_dependency
	atf_add_test_case files_move_to_dependency
	atf_add_test_case files_move_to_dependency2
}
