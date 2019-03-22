#!/usr/bin/env atf-sh
#
# This test case reproduces the following issue:
#
#	- A-1.0_1 is installed.
#	- B-1.0_1 is going to be installed and it should replace pkg A, but
#		  incorrectly wants to replace A multiple times, i.e
#		  replaces="A>=0 A>=0"
#	- B-1.0_1 is installed.
#	- A-1.0_1 is registered in pkgdb, while it should not.

atf_test_case replace_dups

replace_dups_head() {
	atf_set "descr" "Tests for package replace: replacing multiple times a pkg"
}

replace_dups_body() {
	mkdir some_repo root
	mkdir -p pkg_A/usr/bin pkg_B/usr/bin
	echo "A-1.0_1" > pkg_A/usr/bin/foo
	echo "B-1.0_1" > pkg_B/usr/bin/foo
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --replaces "A>=0 A>=0" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -C xbps.d -r root --repository=$PWD/some_repo -yd A
	atf_check_equal $? 0
	xbps-install -C xbps.d -r root --repository=$PWD/some_repo -yd B
	atf_check_equal $? 0
	result=$(xbps-query -C xbps.d -r root -l|wc -l)
	atf_check_equal $result 1
	atf_check_equal $(xbps-query -C xbps.d -r root -p state B) installed
}

atf_test_case replace_ntimes

replace_ntimes_head() {
	atf_set "descr" "Tests for package replace: replacing installed pkg by multiple pkgs"
}

replace_ntimes_body() {
	mkdir some_repo root
	mkdir -p pkg_A/usr/bin pkg_B/usr/bin pkg_C/usr/bin pkg_D/usr/bin
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-create -A noarch -n C-1.0_1 -s "C pkg" ../pkg_C
	atf_check_equal $? 0
	xbps-create -A noarch -n D-1.0_1 -s "D pkg" ../pkg_D
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -C xbps.d -r root --repository=$PWD/some_repo -yd A B C D
	atf_check_equal $? 0
	cd some_repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.1_1 -s "B pkg" --replaces "A<1.1" ../pkg_B
	atf_check_equal $? 0
	xbps-create -A noarch -n C-1.1_1 -s "C pkg" --replaces "A<1.1" ../pkg_C
	atf_check_equal $? 0
	xbps-create -A noarch -n D-1.1_1 -s "D pkg" --replaces "A<1.1" ../pkg_D
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	result=$(xbps-install -C xbps.d -r root --repository=$PWD/some_repo -yun|wc -l)
	atf_check_equal $result 4
}

atf_test_case self_replace

self_replace_head() {
	atf_set "descr" "Tests for package replace: self replacing virtual packages"
}

self_replace_body() {
	mkdir some_repo root
	mkdir -p pkg_A/usr/bin pkg_B/usr/bin
	echo "A-1.0_1" > pkg_A/usr/bin/foo
	echo "B-1.0_1" > pkg_B/usr/bin/foo
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --replaces "A>=0" --provides="A-1.0_1" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -C xbps.d -r root --repository=$PWD/some_repo -yd A
	atf_check_equal $? 0
	xbps-install -C xbps.d -r root --repository=$PWD/some_repo -yd B
	atf_check_equal $? 0
	xbps-install -C xbps.d -r root --repository=$PWD/some_repo -yd A
	atf_check_equal $? 0
	out=$(xbps-query -C xbps.d -r root -l|awk '{print $2}')
	exp="A-1.0_1"
	atf_check_equal $out $exp
	atf_check_equal $(xbps-query -C xbps.d -r root -p state A) installed
}

atf_test_case replace_vpkg

replace_vpkg_head() {
	atf_set "descr" "Tests for package replace: replacing pkg with a virtual pkg"
}

replace_vpkg_body() {
	mkdir some_repo root
	mkdir -p libGL-32bit/usr/bin catalyst-32bit/usr/bin qt-32bit/usr/bin
	cd some_repo
	xbps-create -A noarch -n libGL-32bit-1.0_1 -s "libGL-32bit pkg" ../libGL-32bit
	atf_check_equal $? 0
	xbps-create -A noarch -n catalyst-32bit-1.0_1 -s "catalyst 32bit pkg" \
		--provides "libGL-32bit-1.0_1" --replaces "libGL-32bit>=0" \
		--dependencies "qt-32bit>=0" ../catalyst-32bit
	atf_check_equal $? 0
	xbps-create -A noarch -n qt-32bit-1.0_1 -s "qt 32bit pkg" --dependencies "libGL-32bit>=0" ../qt-32bit
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -C xbps.d -r root --repository=$PWD/some_repo -yd libGL-32bit
	atf_check_equal $? 0
	xbps-install -C xbps.d -r root --repository=$PWD/some_repo -yd catalyst-32bit
	atf_check_equal $? 0
}

atf_test_case replace_pkg_files

replace_pkg_files_head() {
	atf_set "descr" "Tests for package replace: replacing pkg files"
}

replace_pkg_files_body() {
	mkdir -p repo root libGL/usr/lib nvidia/usr/lib
	echo 123456789 > libGL/usr/lib/libGL.so.1.9
	ln -s libGL.so.1.9 libGL/usr/lib/libGL.so.1
	ln -s libGL.so.1.9 libGL/usr/lib/libGL.so
	echo 987654321 > nvidia/usr/lib/libGL.so.340.48
	ln -s libGL.so.340.48 nvidia/usr/lib/libGL.so.1
	ln -s libGL.so.340.48 nvidia/usr/lib/libGL.so

	cd repo
	xbps-create -A noarch -n libGL-1.0_1 -s "libGL pkg" ../libGL
	atf_check_equal $? 0
	xbps-create -A noarch -n nvidia-1.0_1 -s "nvidia pkg" --provides "libGL-1.0_1" --replaces "libGL>=0" ../nvidia
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -C xbps.d -r root --repository=$PWD/repo -yvd libGL
	atf_check_equal $? 0
	xbps-install -C xbps.d -r root --repository=$PWD/repo -yvd nvidia
	atf_check_equal $? 0
	ls -l root/usr/lib
	result=$(readlink root/usr/lib/libGL.so)
	atf_check_equal $result libGL.so.340.48
	result=$(readlink root/usr/lib/libGL.so.1)
	atf_check_equal $result libGL.so.340.48
	result=$(cat root/usr/lib/libGL.so.340.48)
	atf_check_equal $result 987654321
}

atf_test_case replace_pkg_files_unmodified

replace_pkg_files_unmodified_head() {
	atf_set "descr" "Tests for package replace: replacing pkg files with unmodified files"
}

replace_pkg_files_unmodified_body() {
	mkdir -p repo root libGL/usr/lib nvidia/usr/lib
	echo 123456789 > libGL/usr/lib/libGL.so.1.9
	ln -s libGL.so.1.9 libGL/usr/lib/libGL.so.1
	ln -s libGL.so.1.9 libGL/usr/lib/libGL.so
	echo 123456789 > nvidia/usr/lib/libGL.so.1.9
	ln -s libGL.so.1.9 nvidia/usr/lib/libGL.so.1
	ln -s libGL.so.1.9 nvidia/usr/lib/libGL.so

	cd repo
	xbps-create -A noarch -n libGL-1.0_1 -s "libGL pkg" ../libGL
	atf_check_equal $? 0
	xbps-create -A noarch -n nvidia-1.0_1 -s "nvidia pkg" --provides "libGL-1.0_1" --replaces "libGL>=0" ../nvidia
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -C xbps.d -r root --repository=$PWD/repo -yvd libGL
	atf_check_equal $? 0
	xbps-install -C xbps.d -r root --repository=$PWD/repo -yvd nvidia
	atf_check_equal $? 0
	ls -l root/usr/lib
	result=$(readlink root/usr/lib/libGL.so)
	atf_check_equal $result libGL.so.1.9
	result=$(readlink root/usr/lib/libGL.so.1)
	atf_check_equal $result libGL.so.1.9
	result=$(cat root/usr/lib/libGL.so.1.9)
	atf_check_equal $result 123456789
}

atf_test_case replace_pkg_with_update

replace_pkg_with_update_head() {
	atf_set "descr" "Tests for package replace: replace a pkg that needs to be updated with a vpkg"
}

replace_pkg_with_update_body() {
	mkdir some_repo root
	mkdir -p pkg_A/usr/bin pkg_B/usr/bin
	echo "A-1.0_1" > pkg_A/usr/bin/foo
	echo "B-1.0_1" > pkg_B/usr/bin/foo
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --replaces "A>=0" --provides="A-1.1_1" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -C xbps.d -r root --repository=$PWD/some_repo -yd A
	atf_check_equal $? 0
	cd some_repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -C xbps.d -r root --repository=$PWD/some_repo -yfd A B
	atf_check_equal $? 0
	result=$(xbps-query -C xbps.d -r root -l|wc -l)
	atf_check_equal $result 1
	atf_check_equal $(xbps-query -C xbps.d -r root -p state B) installed
}

atf_test_case replace_vpkg_with_update

replace_vpkg_with_update_head() {
	atf_set "descr" "Tests for package replace: replace a vpkg that needs to be updated with a vpkg (#116)"
}

replace_vpkg_with_update_body() {
	mkdir some_repo root
	mkdir -p pkg_A/usr/bin pkg_B/usr/bin
	echo "A-1.0_1" > pkg_A/usr/bin/foo
	echo "B-1.0_1" > pkg_B/usr/bin/foo
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" --replaces "awk>=0" --provides="awk-0_1" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --replaces "awk>=0" --provides="awk-0_1" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -C xbps.d -r root --repository=$PWD/some_repo -yd A
	atf_check_equal $? 0
	cd some_repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" --replaces "awk>=0" --provides "awk-0_1" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -C xbps.d -r root --repository=$PWD/some_repo -yfd A B
	atf_check_equal $? 0
	result=$(xbps-query -C xbps.d -r root -l|wc -l)
	atf_check_equal $result 1
	atf_check_equal $(xbps-query -C xbps.d -r root -p state B) installed
}

atf_init_test_cases() {
	atf_add_test_case replace_dups
	atf_add_test_case replace_ntimes
	atf_add_test_case replace_vpkg
	atf_add_test_case replace_pkg_files
	atf_add_test_case replace_pkg_files_unmodified
	atf_add_test_case replace_pkg_with_update
	atf_add_test_case replace_vpkg_with_update
	atf_add_test_case self_replace
}
