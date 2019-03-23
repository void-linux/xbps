#! /usr/bin/env atf-sh

atf_test_case install_existent

install_existent_head() {
	atf_set "descr" "xbps-install(8): install multiple existent pkgs (issue #53)"
}

install_existent_body() {
	mkdir -p some_repo pkg_A pkg_B
	touch pkg_A/file00
	touch pkg_B/file00
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.1_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -y A
	atf_check_equal $? 0
	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -y A B
	atf_check_equal $? 0

	rm -r root
	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -y A
	atf_check_equal $? 0
	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -y B A
	atf_check_equal $? 0
}

atf_test_case update_existent

updated_existent_head() {
	atf_set "descr" "xbps-install(8): update existent pkg"
}

update_existent_body() {
	mkdir -p some_repo pkg_A pkg_B
	touch pkg_A/file00
	touch pkg_B/file00
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.1_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -y A
	atf_check_equal $? 0
	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -un A B
	atf_check_equal $? 0
}

atf_test_case directory_to_symlink

directory_to_symlink_head() {
	atf_set "descr" "xbps-install(8): replace directory with symlink"
}

directory_to_symlink_body() {
	atf_expect_failure "does not work at the moment"
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

	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -y -u
	atf_check_equal $? 0
}

update_extract_dir_head() {
	atf_set "descr" "xbps-install(8): update replaces file with directory"
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

atf_init_test_cases() {
	atf_add_test_case install_existent
	atf_add_test_case update_existent
	# atf_add_test_case directory_to_symlink
	atf_add_test_case update_extract_dir
}
