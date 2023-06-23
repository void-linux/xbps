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
	touch pkg_A/usr/bin/foo pkg_A/usr/bin/blah
	touch pkg_B/usr/sbin/baz

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
	xbps-install -r root -C null.conf --repository=$PWD/some_repo -dyvf A-1.0_1
	atf_check_equal $? 0

	rv=0
	if [ ! -f root/usr/sbin/baz ]; then
		rv=1
	fi
	for f in usr/bin/foo usr/bin/blah; do
		if [ -f root/$f ]; then
			rv=2
		fi
	done
	atf_check_equal $rv 0
}

atf_test_case reinstall_keep_directories

reinstall_keep_directories_head() {
	atf_set "descr" "Keep directories that are not obsolete when reinstalling a package"
}

reinstall_keep_directories_body() {
	#
	# Simulate a pkg downgrade and make sure directories in both versions
	# will not be recreated.
	#
	mkdir some_repo
	mkdir -p pkg_A/usr/bin pkg_B/usr/bin
	touch pkg_A/usr/bin/foo pkg_A/usr/bin/blah
	touch pkg_B/usr/bin/baz

	cd some_repo
	xbps-create -A noarch -n A-1.1_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0

	cd ..
	xbps-install -r root -C null.conf --repository=$PWD/some_repo -yv A-1.1_1
	atf_check_equal $? 0

	inode1="$(stat -c '%i' root/usr/bin)"

	rm -f some_repo/*
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "foo pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0

	cd ..
	xbps-install -r root -C null.conf --repository=$PWD/some_repo -dyvf A-1.0_1
	atf_check_equal $? 0

	inode2="$(stat -c '%i' root/usr/bin)"
	atf_check_equal "$inode1" "$inode2"
}

# 2- make sure that root symlinks aren't detected as obsoletes on upgrades.
atf_test_case root_symlinks_update

root_symlinks_update_head() {
	atf_set "descr" "Test that root symlinks aren't obsoletes on update"
}

root_symlinks_update_body() {
	mkdir repo
	mkdir -p pkg_A/usr/bin pkg_A/usr/sbin pkg_A/usr/lib pkg_A/var pkg_A/run
	touch pkg_A/usr/bin/foo
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
	touch pkg_A/usr/bin/blah

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

	foofile="$(xbps-query -r root -f A-1.1_1|grep foo)"
	atf_check_equal "$foofile" ""

	foofile="$(xbps-query -r root -f B-1.1_1|grep foo)"
	atf_check_equal "$foofile" /usr/bin/foo

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
	touch -mt 197001010000.00 pkg_libressl/usr/lib/libcrypto.so.30
	touch -mt 197001010000.00 pkg_libcrypto/usr/lib/libcrypto.so.30

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
	xbps-create -A noarch -n libcrypto-1.1_1 -s "libcrypto pkg" --replaces "libressl<1.1_1" ../pkg_libcrypto
	atf_check_equal $? 0
	xbps-create -A noarch -n libressl-1.1_1 -s "libressl pkg" --dependencies "libcrypto>=1.0" ../pkg_libressl
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0

	xbps-install -r root --repository=$PWD -yuvd
	atf_check_equal $? 0

	foofile="$(xbps-query -r root -f libressl|grep crypto)"
	atf_check_equal "$foofile" ""

	foofile="$(xbps-query -r root -f libcrypto|grep crypto)"
	atf_check_equal "$foofile" /usr/lib/libcrypto.so.30

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
	touch -mt 197001010000.00 pkg_libressl/usr/lib/libcrypto.so.30
	touch -mt 197001010000.00 pkg_libressl/usr/lib/libssl.so.30
	touch -mt 197001010000.00 pkg_libcrypto/usr/lib/libcrypto.so.30

	cd repo
	xbps-create -A noarch -n libressl-1.0_1 -s "libressl pkg" ../pkg_libressl
	atf_check_equal $? 0
	xbps-create -A noarch -n libcrypto-1.0_1 -s "libcrypto pkg" ../pkg_libcrypto
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0

	xbps-install -r root --repository=$PWD -yvd libressl
	atf_check_equal $? 0

	rm -f ../pkg_libressl/usr/lib/libcrypto.*
	xbps-create -A noarch -n libcrypto-1.0_2 -s "libcrypto pkg" ../pkg_libcrypto
	atf_check_equal $? 0
	xbps-create -A noarch -n libressl-1.1_1 -s "libressl pkg" --dependencies "libcrypto>=1.0" ../pkg_libressl
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0

	xbps-install -r root --repository=$PWD -yuvd
	atf_check_equal $? 0

	foofile="$(xbps-query -r root -f libressl|grep crypto)"
	atf_check_equal "$foofile" ""

	foofile="$(xbps-query -r root -f libcrypto|grep crypto)"
	atf_check_equal "$foofile" /usr/lib/libcrypto.so.30

	xbps-pkgdb -r root -av
	atf_check_equal $? 0
}

atf_test_case update_to_meta_depends_replaces

update_to_meta_depends_replaces_head() {
	# https://github.com/void-linux/xbps/issues/12
	atf_set "descr" "Update package to meta moving files to dependency and replaces"
}

update_to_meta_depends_replaces_body() {
	mkdir repo
	mkdir -p pkg_openssl/usr/lib pkg_libressl
	echo "0123456789" > pkg_openssl/usr/lib/libcrypto.so.30
	echo "0123456789" > pkg_openssl/usr/lib/libssl.so.30
	touch -mt 197001010000.00 pkg_openssl/usr/lib/libcrypto.so.30
	touch -mt 197001010000.00 pkg_openssl/usr/lib/libssl.so.30

	cd repo
	xbps-create -A noarch -n openssl-1.0_1 -s "openssl pkg" ../pkg_openssl
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yvd openssl
	atf_check_equal $? 0

	cd repo
	mv ../pkg_openssl/usr ../pkg_libressl/
	touch -mt 197001010000.00 ../pkg_libressl/usr/lib/libcrypto.so.30
	touch -mt 197001010000.00 ../pkg_libressl/usr/lib/libssl.so.30
	xbps-create -A noarch -n openssl-1.0_2 -s "openssl pkg (meta)" --dependencies="libressl>=1.0_1" ../pkg_openssl
	atf_check_equal $? 0
	xbps-create -A noarch -n libressl-1.0_1 -s "libressl pkg" --replaces "libressl>=0" ../pkg_libressl
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yuvd
	atf_check_equal $? 0

	xbps-query -r root -S openssl
	atf_check_equal $? 0

	xbps-query -r root -S libressl
	atf_check_equal $? 0

	foofile=$(xbps-query -r root -f libressl|grep crypto)
	atf_check_equal $foofile /usr/lib/libcrypto.so.30

	xbps-pkgdb -r root -av
	atf_check_equal $? 0
}

atf_test_case directory_to_symlink

directory_to_symlink_head() {
	atf_set "descr" "Update replaces directory with symlink"
}

directory_to_symlink_body() {
	mkdir -p some_repo pkg_A/foo
	touch pkg_A/foo/bar
	# create package and install it
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -y A
	atf_check_equal $? 0

	# make an update to the package
	cd some_repo
	rm -rf ../pkg_A/foo
	ln -sf ../bar ../pkg_A/foo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -dvyu
	atf_check_equal $? 0
}

atf_test_case directory_to_symlink_preserve

directory_to_symlink_preserve_head() {
	atf_set "descr" "Update replaces directory with symlink (preserved file directory)"
}

directory_to_symlink_preserve_body() {
	mkdir -p some_repo pkg_A/foo xbps.d
	touch pkg_A/foo/bar
	echo "preserve=/foo/bar" >xbps.d/preserve.conf
	# create package and install it
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -y A
	atf_check_equal $? 0

	echo "preserve me" > ./root/foo/bar

	# make an update to the package
	cd some_repo
	rm -rf ../pkg_A/foo
	ln -sf ../bar ../pkg_A/foo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root -C $PWD/xbps.d --repository=$PWD/some_repo -dvyu
	# ENOTEMPTY
	atf_check_equal $? 39

	out="$(cat ./root/foo/bar)"
	atf_check_equal "$out" "preserve me"
}

atf_test_case symlink_to_file_preserve

symlink_to_file_preserve_head() {
	atf_set "descr" "Update replaces preserved symlink with file"
}

symlink_to_file_preserve_body() {
	mkdir -p some_repo pkg_A/foo xbps.d
	touch pkg_A/foo/bar
	echo "preserve=/foo/bar" >xbps.d/preserve.conf
	# create package and install it
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -y A
	atf_check_equal $? 0

	rm ./root/foo/bar
	ln -sf foo ./root/foo/bar

	# make an update to the package
	cd some_repo
	echo "fail" >../pkg_A/foo/bar
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root -C $PWD/xbps.d --repository=$PWD/some_repo -dvyu
	atf_check_equal $? 0

	rv=0
	[ -h root/foo/bar ] || rv=1
	atf_check_equal "$rv" 0
}

atf_test_case update_extract_dir

update_extract_dir_head() {
	atf_set "descr" "Update replaces file with directory"
}

update_extract_dir_body() {
	mkdir -p some_repo pkg_A
	touch pkg_A/file00
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -y A
	atf_check_equal $? 0
	rm pkg_A/file00
	mkdir -p pkg_A/file00
	touch pkg_A/file00/file01
	cd some_repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -d -Suy A
	atf_check_equal $? 0
}

atf_test_case replace_package_same_files

replace_package_same_files_head() {
	atf_set "descr" "Package gets replaced by another one with same files"
}

replace_package_same_files_body() {
	mkdir repo
	mkdir -p pkg_openssl/usr/lib/openssl pkg_libressl/usr/lib
	echo "0123456789" > pkg_openssl/usr/lib/libcrypto.so.30
	echo "0123456789" > pkg_openssl/usr/lib/libssl.so.30
	echo "0123456789" > pkg_libressl/usr/lib/libcrypto.so.30
	echo "0123456789" > pkg_libressl/usr/lib/libssl.so.30
	echo "0123456789" > pkg_libressl/usr/lib/openssl/foo
	touch -mt 197001010000.00 pkg_openssl/usr/lib/libcrypto.so.30
	touch -mt 197001010000.00 pkg_openssl/usr/lib/libssl.so.30
	touch -mt 197001010000.00 pkg_libressl/usr/lib/libcrypto.so.30
	touch -mt 197001010000.00 pkg_libressl/usr/lib/libssl.so.30

	cd repo
	xbps-create -A noarch -n openssl-1.0_1 -s "openssl pkg" ../pkg_openssl
	atf_check_equal $? 0
	xbps-create -A noarch -n libressl-1.0_1 -s "libressl pkg" --replaces "openssl>=0" ../pkg_libressl
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yvd openssl
	atf_check_equal $? 0

	xbps-install -r root --repository=$PWD/repo -yvd libressl
	atf_check_equal $? 0

	xbps-query -r root -S openssl
	atf_check_equal $? 2

	xbps-query -r root -S libressl
	atf_check_equal $? 0

	xbps-pkgdb -r root -av
	atf_check_equal $? 0
}

atf_test_case nonempty_dir_abort

nonempty_dir_abort_head() {
	atf_set "descr" "Abort transaction if a directory replaced by a file"
}

nonempty_dir_abort_body() {
	mkdir repo root
	mkdir -p pkg_A pkg_B/foo
	echo "0123456789" > pkg_B/foo/bar

	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yvd A B
	atf_check_equal $? 0

	cd repo
	rm -f *.xbps
	xbps-create -A noarch -n A-1.0_2 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	rm -rf ../pkg_B/foo
	echo "0123456789" > ../pkg_B/foo
	xbps-create -A noarch -n B-1.0_2 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	touch root/foo/abort

	xbps-install -r root --repository=$PWD/repo -yvdu
	# ENOTEMPTY
	atf_check_equal $? 39
	out=$(xbps-query -r root -p state A)
	atf_check_equal "$out" "installed"
	out=$(xbps-query -r root -p state B)
	atf_check_equal "$out" "installed"
	xbps-pkgdb -r root -av
	atf_check_equal $? 0

	rm root/foo/abort

	xbps-install -r root --repository=$PWD/repo -yvdu
	atf_check_equal $? 0
	out=$(xbps-query -r root -p state A)
	atf_check_equal "$out" "installed"
	out=$(xbps-query -r root -p state B)
	atf_check_equal "$out" "installed"
	xbps-pkgdb -r root -av
	atf_check_equal $? 0
}

atf_test_case conflicting_files_in_transaction

conflicting_files_in_transaction_head() {
	atf_set "descr" "Test that obsolete files are removed on reinstall"
}

conflicting_files_in_transaction_body() {
	mkdir some_repo root
	mkdir -p pkg_A pkg_B
	touch pkg_A/foo pkg_B/foo

	cd some_repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.1_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root -C null.conf --repository=$PWD/some_repo -ydv A B
	# EEXIST
	atf_check_equal $? 17

	# ignored file conflicts
	xbps-install -r root -C null.conf --repository=$PWD/some_repo -Iydv A B
	atf_check_equal $? 0
}

atf_test_case obsolete_dir

obsolete_dir_head() {
	atf_set "descr" "Obsolete directories get removed"
}

obsolete_dir_body() {
	mkdir repo root
	mkdir -p pkg_A/foo/bar/
	ln -s usr/bin pkg_A/bin
	touch pkg_A/foo/bar/lol

	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yvd A
	atf_check_equal $? 0

	cd repo
	rm -f *.xbps
	rm -rf ../pkg_A/foo
	xbps-create -A noarch -n A-1.0_2 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yvdu
	atf_check_equal $? 0

	rv=0
	[ -d root/foo/bar ] && rv=2
	[ -d root/foo ] && rv=3
	atf_check_equal "$rv" 0
}

atf_test_case base_symlinks

base_symlinks_head() {
	atf_set "descr" "Base symlinks are not removed"
}

base_symlinks_body() {
	mkdir repo root
	mkdir -p pkg_A/usr/bin
	touch >pkg_A/usr/bin/foo
	ln -s usr/bin pkg_A/bin

	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yvd A
	atf_check_equal $? 0

	cd repo
	rm -f *.xbps
	rm -f ../pkg_A/bin
	rm -rf ../pkg_A/usr
	xbps-create -A noarch -n A-1.0_2 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yvdu
	atf_check_equal $? 0

	rv=0
	[ -d root/usr/bin/foo ] && rv=2
	[ -d root/usr/bin ] && rv=3
	[ -d root/usr ] && rv=4
	[ -h root/bin ] || rv=5
	atf_check_equal "$rv" 0
}

atf_test_case keep_modified_files

keep_modified_files_head() {
	atf_set "descr" "Test to keep modified files from being deleted"
}

keep_modified_files_body() {
	mkdir some_repo root
	mkdir -p pkg_A
	touch pkg_A/foo

	cd some_repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root -C null.conf --repository=$PWD/some_repo -ydv A
	atf_check_equal $? 0

	echo "modified" >root/foo

	cd some_repo
	rm ../pkg_A/foo
	xbps-create -A noarch -n A-1.1_2 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root -C null.conf --repository=$PWD/some_repo -ydvu
	atf_check_equal $? 0

	rv=0
	[ -e root/foo ] || rv=1
	atf_check_equal $rv 0
}

atf_test_case alternative_to_regular_file

alternative_to_regular_file_head() {
	atf_set "descr" "Test update changing alternative to regular file"
}

alternative_to_regular_file_body() {
	mkdir some_repo root
	mkdir -p pkg_A/bar
	touch pkg_A/foo pkg_A/bar/keep

	cd some_repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" \
		--alternatives "foo:/alt1:foo foo:/alt2:bar" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root -C null.conf --repository=$PWD/some_repo -ydv A
	atf_check_equal $? 0

	cd some_repo
	rm ../pkg_A/foo
	touch ../pkg_A/alt1
	touch ../pkg_A/alt2
	xbps-create -A noarch -n A-1.1_2 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root -C null.conf --repository=$PWD/some_repo -ydvu
	atf_check_equal $? 0

	rv=0
	[ -f root/alt1 ] || rv=1
	atf_check_equal $rv 0

	rv=0
	[ -f root/alt2 ] || rv=1
	atf_check_equal $rv 0
}

atf_test_case replace_package_same_files_unordered

replace_package_same_files_unordered_head() {
	atf_set "descr" "Package gets replaced by another one with same files unordered"
}

replace_package_same_files_unordered_body() {
	mkdir repo
	mkdir -p pkg_polkit/usr/bin pkg_polkit-elogind/usr/bin pkg_A
	echo "0123456789" > pkg_polkit-elogind/usr/bin/polkit
	echo "0123456789" > pkg_polkit/usr/bin/polkit
	touch -mt 197001010000.00 pkg_polkit/usr/bin/polkit
	touch -mt 197001010000.00 pkg_polkit-elogind/usr/bin/polkit

	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" --dependencies "polkit-elogind>=0" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "A>=0" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n polkit-elogind-1.0_1 -s "polkit-elogind pkg" ../pkg_polkit-elogind
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yvd A B
	atf_check_equal $? 0

	xbps-pkgdb -r root -av
	atf_check_equal $? 0

	xbps-query -r root -S polkit-elogind
	atf_check_equal $? 0

	xbps-query -r root -S polkit
	atf_check_equal $? 2

	cd repo
	xbps-create -A noarch -n A-1.0_2 -s "A pkg" --dependencies "polkit>=1.0_2" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_2 -s "B pkg" --dependencies "A>=1.0_2 polkit>=1.0_2" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n polkit-1.0_2 -s "polkit pkg" --replaces "polkit-elogind>=0" --provides "polkit-elogind-1.0_2" ../pkg_polkit
	atf_check_equal $? 0
	rm -rf ../pkg_polkit-elogind/usr
	xbps-create -A noarch -n polkit-elogind-1.0_2 -s "polkit pkg (transitional dummy package)" --dependencies "polkit>=1.0_2" ../pkg_polkit-elogind
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	# out=$(xbps-install -r root --repository=$PWD/repo -yvdun)
	# atf_check_equal "$out" "A-1.0_2 update noarch $PWD/repo\nlibressl-1.0_2 update noarch $PWD/repo"

	xbps-install -r root --repository=$PWD/repo -yvdu

	xbps-query -r root -S polkit-elogind
	atf_check_equal $? 2

	xbps-query -r root -S polkit
	atf_check_equal $? 0

	xbps-pkgdb -r root -av
	atf_check_equal $? 0
}

atf_test_case obsoletes_with_alternatives

obsoletes_with_alternatives_head() {
	atf_set "descr" "Package adds alternative"
}

obsoletes_with_alternatives_body() {
	mkdir repo
	mkdir -p pkg_A/usr/bin
	touch pkg_A/usr/bin/tar

	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yvd A
	atf_check_equal $? 0

	xbps-pkgdb -r root -av
	atf_check_equal $? 0

	cd repo
	mv ../pkg_A/usr/bin/tar ../pkg_A/usr/bin/gtar
	xbps-create -A noarch -n A-1.0_2 -s "A pkg" --alternatives "tar:tar:/usr/bin/gtar" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yvdu

	xbps-pkgdb -r root -av
	atf_check_equal $? 0

	test -h root/usr/bin/tar
	atf_check_equal $? 0
}

atf_test_case multiple_obsoletes_with_alternatives

multiple_obsoletes_with_alternatives_head() {
	atf_set "descr" "Multiple packages add alternative"
}

multiple_obsoletes_with_alternatives_body() {
	mkdir repo
	mkdir -p pkg_A/usr/bin pkg_B/usr/bin
	touch pkg_A/usr/bin/tar pkg_B/usr/bin/bsdtar

	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yvd A B
	atf_check_equal $? 0

	xbps-pkgdb -r root -av
	atf_check_equal $? 0

	cd repo
	mv ../pkg_A/usr/bin/tar ../pkg_A/usr/bin/gtar
	xbps-create -A noarch -n A-1.0_2 -s "A pkg" --alternatives "tar:tar:/usr/bin/gtar" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_2 -s "B pkg" --alternatives "tar:tar:/usr/bin/bsdtar" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yvdu

	xbps-pkgdb -r root -av
	atf_check_equal $? 0

	ls -lsa root/usr/bin
	test -h root/usr/bin/tar
	atf_check_equal $? 0
}

atf_test_case multiple_obsoletes_with_alternatives_unordered

multiple_obsoletes_with_alternatives_unordered_head() {
	atf_set "descr" "Multiple packages add alternative unordered"
	atf_expect_fail "not fixed yet"
}

multiple_obsoletes_with_alternatives_unordered_body() {
	mkdir repo
	mkdir -p pkg_A/usr/bin pkg_B/usr/bin
	touch pkg_A/usr/bin/tar pkg_B/usr/bin/bsdtar

	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yvd A B
	atf_check_equal $? 0

	xbps-pkgdb -r root -av
	atf_check_equal $? 0

	cd repo
	mv ../pkg_A/usr/bin/tar ../pkg_A/usr/bin/gtar
	xbps-create -A noarch -n A-1.0_2 -s "A pkg" --alternatives "tar:tar:/usr/bin/gtar" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_2 -s "B pkg" --alternatives "tar:tar:/usr/bin/bsdtar" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yvdu B A

	xbps-pkgdb -r root -av
	atf_check_equal $? 0

	ls -lsa root/usr/bin
	test -h root/usr/bin/tar
	atf_check_equal $? 0
}

atf_test_case obsolete_directory_multiple_packages1

obsolete_directory_multiple_packages1_head() {
	atf_set "descr" "Remove multiple packages with the same obsolete directory, variant 1"
}

obsolete_directory_multiple_packages1_body() {
	mkdir repo
	mkdir -p pkg_A/usr/lib/gcc/9.2/include pkg_B/usr/lib/gcc/9.2/include
	touch pkg_A/usr/lib/gcc/9.2/include/foo.h pkg_B/usr/lib/gcc/9.2/include/bar.h

	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yvd A B
	atf_check_equal $? 0

	xbps-pkgdb -r root -av
	atf_check_equal $? 0

	rm -rf pkg_A pkg_B
	mkdir -p pkg_A/usr/lib/gcc/9.3/include pkg_B/usr/lib/gcc/9.3/include
	touch pkg_A/usr/lib/gcc/9.3/include/foo.h pkg_B/usr/lib/gcc/9.3/include/bar.h

	cd repo
	xbps-create -A noarch -n A-2.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-2.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yvdu B A

	xbps-pkgdb -r root -av
	atf_check_equal $? 0

	ls -lsa root/usr/lib/gcc
	test -d root/usr/lib/gcc/9.2
	atf_check_equal $? 1
}

atf_test_case obsolete_directory_multiple_packages2

obsolete_directory_multiple_packages2_head() {
	atf_set "descr" "Remove multiple packages with the same obsolete directory, variant 2"
}

obsolete_directory_multiple_packages2_body() {
	mkdir repo
	mkdir -p pkg_A/usr/lib/gcc/9.2/include pkg_B/usr/lib/gcc/9.2/include
	touch pkg_A/usr/lib/gcc/9.2/include/foo.h pkg_B/usr/lib/gcc/9.2/include/bar.h

	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yvd A B
	atf_check_equal $? 0

	xbps-pkgdb -r root -av
	atf_check_equal $? 0

	rm -rf pkg_A pkg_B
	mkdir -p pkg_A/usr/lib/gcc/9.3/include pkg_B/usr/lib/gcc/9.3/include
	touch pkg_A/usr/lib/gcc/9.3/include/foo.h pkg_B/usr/lib/gcc/9.3/include/bar.h

	cd repo
	xbps-create -A noarch -n A-2.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-2.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yvdu A B

	xbps-pkgdb -r root -av
	atf_check_equal $? 0

	ls -lsa root/usr/lib/gcc
	test -d root/usr/lib/gcc/9.2
	atf_check_equal $? 1
}

atf_test_case obsolete_directory_multiple_packages3

obsolete_directory_multiple_packages3_head() {
	atf_set "descr" "Remove multiple packages with the same obsolete directory, variant 3"
}

obsolete_directory_multiple_packages3_body() {
	mkdir repo
	mkdir -p pkg_A/usr/lib/gcc/9.2/include pkg_B/usr/lib/gcc/9.2/include pkg_C/usr/lib/gcc/9.2/include
	touch pkg_A/usr/lib/gcc/9.2/include/foo.h pkg_B/usr/lib/gcc/9.2/include/bar.h pkg_C/usr/lib/gcc/9.2/include/fizz.h

	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-create -A noarch -n C-1.0_1 -s "C pkg" ../pkg_C
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yvd A B C
	atf_check_equal $? 0

	xbps-pkgdb -r root -av
	atf_check_equal $? 0

	rm -rf pkg_A pkg_B pkg_C
	mkdir -p pkg_A/usr/lib/gcc/9.3/include pkg_B/usr/lib/gcc/9.3/include pkg_C/usr/lib/gcc/9.3/include
	touch pkg_A/usr/lib/gcc/9.3/include/foo.h pkg_B/usr/lib/gcc/9.3/include/bar.h pkg_C/usr/lib/gcc/9.3/include/fizz.h

	cd repo
	xbps-create -A noarch -n A-2.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-2.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-create -A noarch -n C-2.0_1 -s "C pkg" ../pkg_C
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yvdu B A C

	xbps-pkgdb -r root -av
	atf_check_equal $? 0

	ls -lsa root/usr/lib/gcc
	test -d root/usr/lib/gcc/9.2
	atf_check_equal $? 1
}

atf_test_case obsolete_directory_multiple_packages4

obsolete_directory_multiple_packages4_head() {
	atf_set "descr" "Remove multiple packages with the same obsolete directory, variant 4"
}

obsolete_directory_multiple_packages4_body() {
	mkdir repo
	mkdir -p pkg_A/usr/lib/gcc/9.2/include pkg_B/usr/lib/gcc/9.2/include pkg_C/usr/lib/gcc/9.2/include
	touch pkg_A/usr/lib/gcc/9.2/include/foo.h pkg_B/usr/lib/gcc/9.2/include/bar.h pkg_C/usr/lib/gcc/9.2/include/fizz.h

	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-create -A noarch -n C-1.0_1 -s "C pkg" ../pkg_C
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yvd A B C
	atf_check_equal $? 0

	xbps-pkgdb -r root -av
	atf_check_equal $? 0

	rm -rf pkg_A pkg_B pkg_C
	mkdir -p pkg_A/usr/lib/gcc/9.3/include pkg_B/usr/lib/gcc/9.3/include pkg_C/usr/lib/gcc/9.3/include
	touch pkg_A/usr/lib/gcc/9.3/include/foo.h pkg_B/usr/lib/gcc/9.3/include/bar.h pkg_C/usr/lib/gcc/9.3/include/fizz.h

	cd repo
	xbps-create -A noarch -n A-2.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-2.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-create -A noarch -n C-2.0_1 -s "C pkg" ../pkg_C
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/repo -yvdu A B C

	xbps-pkgdb -r root -av
	atf_check_equal $? 0

	ls -lsa root/usr/lib/gcc
	test -d root/usr/lib/gcc/9.2
	atf_check_equal $? 1
}

atf_init_test_cases() {
	atf_add_test_case reinstall_obsoletes
	atf_add_test_case reinstall_keep_directories
	atf_add_test_case root_symlinks_update
	atf_add_test_case files_move_from_dependency
	atf_add_test_case files_move_to_dependency
	atf_add_test_case files_move_to_dependency2
	atf_add_test_case update_to_meta_depends_replaces
	atf_add_test_case directory_to_symlink
	atf_add_test_case directory_to_symlink_preserve
	atf_add_test_case symlink_to_file_preserve
	atf_add_test_case update_extract_dir
	atf_add_test_case replace_package_same_files
	atf_add_test_case nonempty_dir_abort
	atf_add_test_case conflicting_files_in_transaction
	atf_add_test_case obsolete_dir
	atf_add_test_case base_symlinks
	atf_add_test_case keep_modified_files
	atf_add_test_case alternative_to_regular_file
	atf_add_test_case replace_package_same_files_unordered
	atf_add_test_case obsoletes_with_alternatives
	atf_add_test_case multiple_obsoletes_with_alternatives
	atf_add_test_case multiple_obsoletes_with_alternatives_unordered
	atf_add_test_case obsolete_directory_multiple_packages1
	atf_add_test_case obsolete_directory_multiple_packages2
	atf_add_test_case obsolete_directory_multiple_packages3
	atf_add_test_case obsolete_directory_multiple_packages4
}
