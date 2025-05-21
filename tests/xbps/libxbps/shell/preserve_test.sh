#!/usr/bin/env atf-sh

atf_test_case preserve_update

preserve_update_head() {
	atf_set "descr" "Tests for pkg install/upgrade with preserve"
}

preserve_update_body() {
	mkdir some_repo
	mkdir -p pkg_A/1.0
	echo "blahblah" > pkg_A/1.0/blah
	echo "foofoo" > pkg_A/1.0/foo

	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" --preserve ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/some_repo -yd A
	atf_check_equal $? 0

	cd some_repo
	mv ../pkg_A/1.0 ../pkg_A/1.1
	rm -f *.xbps
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" --preserve ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/some_repo -yd A
	atf_check_equal $? 0

	rv=0
	[ -e root/1.0/blah -a -e root/1.0/blah ] || rv=1
	[ -e root/1.1/blah -a -e root/1.1/blah ] || rv=2
	atf_check_equal $rv 0

	xbps-pkgdb -r root -av
	atf_check_equal $? 0
}

atf_test_case preserve_update_conflict

preserve_update_conflict_head() {
	# current behaviour is to not fail
	atf_set "descr" "Tests for pkg install/upgrade with preserve conflict"
}

preserve_update_conflict_body() {
	mkdir some_repo
	mkdir -p pkg_A/1.0
	echo "blahblah" > pkg_A/1.0/blah
	echo "foofoo" > pkg_A/1.0/foo

	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" --preserve ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/some_repo -yd A
	atf_check_equal $? 0

	cd some_repo
	rm -f *.xbps
	xbps-create -A noarch -n A-1.0_2 -s "A pkg" --preserve ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/some_repo -yd A
	atf_check_equal $? 0

	rv=0
	[ -e root/1.0/blah -a -e root/1.0/blah ] || rv=1
	atf_check_equal $rv 0

	xbps-pkgdb -r root -av
	atf_check_equal $? 0
}

atf_test_case preserve_remove

preserve_remove_head() {
	atf_set "descr" "Tests for pkg removal with preserve"
}

preserve_remove_body() {
	mkdir some_repo
	mkdir -p pkg_A/1.0
	echo "blahblah" > pkg_A/1.0/blah
	echo "foofoo" > pkg_A/1.0/foo

	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" --preserve ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/some_repo -yd A
	atf_check_equal $? 0

	cd some_repo
	mv ../pkg_A/1.0 ../pkg_A/1.1
	rm -f *.xbps
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" --preserve ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/some_repo -yd A
	atf_check_equal $? 0

	rv=0
	[ -e root/1.0/blah -a -e root/1.0/blah ] || rv=1
	[ -e root/1.1/blah -a -e root/1.1/blah ] || rv=2
	atf_check_equal $rv 0

	xbps-remove -r root -yd A
	atf_check_equal $? 0

	rv=0
	[ -e root/1.0/blah -a -e root/1.0/blah ] || rv=1
	[ -e root/1.1/blah -a -e root/1.1/blah ] || rv=2
	atf_check_equal $rv 0

	xbps-pkgdb -r root -av
	atf_check_equal $? 0
}


atf_init_test_cases() {
	atf_add_test_case preserve_update
	atf_add_test_case preserve_update_conflict
	atf_add_test_case preserve_remove
}
