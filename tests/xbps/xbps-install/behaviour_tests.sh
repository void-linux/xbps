#! /usr/bin/env atf-sh

atf_test_case install_existent

install_existent_head() {
	atf_set "descr" "xbps-install(1): install multiple existent pkgs (issue #53)"
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
	atf_set "descr" "xbps-install(1): update existent pkg"
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

atf_test_case reproducible

reproducible_head() {
	atf_set "descr" "xbps-install(1): test --reproducible"
}

reproducible_body() {
	mkdir -p repo-1 repo-2 pkg_A
	touch pkg_A/file
	cd repo-1
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ../repo-2
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root-1 --repo=$PWD/repo-1 --repro -y A
	atf_check_equal $? 0
	xbps-install -r root-2 --repo=$PWD/repo-2 --repro -y A
	atf_check_equal $? 0

	# Compare pkgdb in both rootdirs
	cmp -s root-1/var/db/xbps/pkgdb-0.38.plist root-2/var/db/xbps/pkgdb-0.38.plist
	atf_check_equal $? 0

	# Now check without --reproducible
	rm -rf root-1 root-2

	xbps-install -r root-1 --repo=$PWD/repo-1 --repro -y A
	atf_check_equal $? 0
	xbps-install -r root-2 --repo=$PWD/repo-2 -y A
	atf_check_equal $? 0

	# Compare pkgdb in both rootdirs
	cmp -s root-1/var/db/xbps/pkgdb-0.38.plist root-2/var/db/xbps/pkgdb-0.38.plist
	atf_check_equal $? 1
}

atf_init_test_cases() {
	atf_add_test_case install_existent
	atf_add_test_case update_existent
	atf_add_test_case reproducible
}
