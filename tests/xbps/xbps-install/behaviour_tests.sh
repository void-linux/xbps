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

atf_test_case update_unpacked

update_unpacked_head() {
	atf_set "descr" "xbps-install(1): update unpacked pkg"
}

update_unpacked_body() {
	mkdir -p some_repo pkg_A
	touch pkg_A/file00
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -yU A
	atf_check_equal $? 0
	cd some_repo
	xbps-create -A noarch -n A-1.0_2 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	set -- $(xbps-install -r root -C empty.conf --repository=$PWD/some_repo -un A)
	if [ "$2" != "update" ]; then
		atf_fail "'$2' != 'update'"
	fi
}

atf_test_case unpacked_dep

unpacked_dep_head() {
	atf_set "descr" "xbps-install(1): unpacked dependency"
}

unpacked_dep_body() {
	mkdir -p some_repo pkg_A pkg_B
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" -D "A>=0" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -yU A
	atf_check_equal $? 0
	res=$(xbps-install -r root -C empty.conf --repository=$PWD/some_repo -un B | grep -Fv -e "A-1.0_1 configure" -e "B-1.0_1 install")
	atf_check_equal "$res" ""
}

atf_test_case reinstall_unpacked_unpack_only

reinstall_unpacked_unpack_only_head() {
	atf_set "descr" "xbps-install(1): reinstall unpacked packages with unpack-only"
}

reinstall_unpacked_unpack_only_body() {
	mkdir -p some_repo pkg_A
	touch pkg_A/file00
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -yU A
	atf_check_equal $? 0
	set -- $(xbps-install -r root -C empty.conf --repository=$PWD/some_repo -fUn A)
	if [ "$2" != "reinstall" ]; then
		atf_fail "'$2' != 'reinstall'"
	fi
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

atf_test_case detect_conflict_update

detect_conflict_update_head() {
	atf_set "descr" "xbps-install(1): test updates with conflicts"
}

detect_conflict_update_body() {
	atf_expect_fail "conflicts aren't dealt with in updates"

	mkdir -p some_repo pkg_A pkg_B
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" -C "A>=0" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -yU A

	cd some_repo
	xbps-create -A noarch -n A-1.0_2 -s "A pkg" -D "B>=0" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -d -r root -C empty.conf --repository=$PWD/some_repo -yUu A
	atf_check_equal $? 11

	res=$(xbps-query -r root -C empty.conf -p pkgname B)
	atf_check_equal "$res" ""
}

atf_test_case detect_conflict_install

detect_conflict_install_head() {
	atf_set "descr" "xbps-install(1): test installation with conflicts"
}

detect_conflict_install_body() {
	mkdir -p some_repo pkg_A pkg_B
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" -C "A>=0" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -yU A B
	atf_check_equal $? 11
}

atf_init_test_cases() {
	atf_add_test_case install_existent
	atf_add_test_case update_existent
	atf_add_test_case update_unpacked
	atf_add_test_case unpacked_dep
	atf_add_test_case reinstall_unpacked_unpack_only
	atf_add_test_case reproducible
	atf_add_test_case detect_conflict_update
	atf_add_test_case detect_conflict_install
}
