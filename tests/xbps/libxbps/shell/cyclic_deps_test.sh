#!/usr/bin/env atf-sh
#

atf_test_case cyclic_dep_vpkg

cyclic_dep_vpkg_head() {
	atf_set "descr" "Tests for cyclic deps: pkg depends on a cyclic vpkg"
}

cyclic_dep_vpkg_body() {
	mkdir some_repo
	mkdir -p pkg_A/usr/bin pkg_B/usr/bin pkg_C/usr/bin pkg_D/usr/bin
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" --provides "libGL-7.11_1" --dependencies "libGL>=7.11" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "libGL>=7.11" ../pkg_B
	atf_check_equal $? 0
	xbps-create -A noarch -n C-1.0_1 -s "C pkg" --dependencies "B>=0" ../pkg_C
	atf_check_equal $? 0

	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/some_repo -dy C
	atf_check_equal $? 0

	xbps-query  -r root --fulldeptree -x C
	atf_check_equal $? 0
}

atf_test_case cyclic_dep_vpkg2

cyclic_dep_vpkg2_head() {
	atf_set "descr" "Tests for cyclic deps: unresolved circular dependencies"
}

cyclic_dep_vpkg2_body() {
	mkdir -p some_repo pkg_A/usr/bin pkg_B/usr/bin pkg_C/usr/bin pkg_D/usr/bin

	cd some_repo
	atf_check -o ignore -- xbps-create -A noarch -n A-1.0_1 -s "A pkg" --provides "libGL-7.11_1" --dependencies "xserver-abi-video<20" ../pkg_A
	atf_check -o ignore -- xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "libGL>=7.11" --provides "xserver-abi-video-19_1" ../pkg_B
	atf_check -o ignore -- xbps-create -A noarch -n C-1.0_1 -s "C pkg" --dependencies "libGL>=7.11" ../pkg_C
	atf_check -o ignore -e ignore -- xbps-rindex -d -a $PWD/*.xbps
	cd ..

	atf_check \
		-o match:"A-1\.0_1 install" \
		-o match:"B-1\.0_1 install" \
		-o match:"C-1\.0_1 install" \
		-- xbps-install -r root --repository=$PWD/some_repo -ny C
}

atf_test_case cyclic_dep_vpkg3

cyclic_dep_vpkg3_head() {
	atf_set "descr" "Tests for cyclic deps: unresolvable circular dependencies"
}

cyclic_dep_vpkg3_body() {
	mkdir -p some_repo pkg_A/usr/bin pkg_B/usr/bin pkg_C/usr/bin pkg_D/usr/bin

	cd some_repo
	atf_check -o ignore -- xbps-create -A noarch -n A-1.0_1 -s "A pkg" --provides "libGL-7.11_1" --dependencies "xserver-abi-video<20" ../pkg_A
	atf_check -o ignore -- xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "libGL>=7.11" --provides "xserver-abi-video-21_1" ../pkg_B
	atf_check -o ignore -- xbps-create -A noarch -n C-1.0_1 -s "C pkg" --dependencies "libGL>=7.11" ../pkg_C
	atf_check -o ignore -e ignore -- xbps-rindex -d -a $PWD/*.xbps
	cd ..

	atf_check \
		-s exit:19 \
		-e match:"MISSING: xserver-abi-video<20" \
		-- xbps-install -r root --repository=$PWD/some_repo -ny C
}

atf_test_case cyclic_dep_full

cyclic_dep_full_head() {
	atf_set "descr" "Tests for cyclic deps: verify fulldeptree"
}

cyclic_dep_full_body() {
	mkdir some_repo
	mkdir -p pkg_A/usr/bin pkg_B/usr/bin
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" --dependencies "B>=0" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "A>=0" ../pkg_B
	atf_check_equal $? 0

	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/some_repo -dy B
	atf_check_equal $? 0
	xbps-query -r root --fulldeptree -dx B
	atf_check_equal $? 0
	xbps-remove -r root -Ryvd B
	atf_check_equal $? 0
}


atf_test_case cyclic_dep_of_dep

cyclic_dep_of_dep_head() {
	atf_set "descr" "Tests for cyclic deps: installing cylic deps directly and indirectly"
}

cyclic_dep_of_dep_body() {
	mkdir repo pkg

	cd repo
	atf_check -o ignore -- xbps-create -A noarch -n base-system-1.0_1 -s "base-system" --dependencies "systemd>=0" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n systemd-1.0_1 -s "systemd" --dependencies "systemd-libudev>=0 systemd-analyze>=0" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n systemd-libudev-1.0_1 -s "systemd-libudev" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n systemd-analyze-1.0_1 -s "systemd-analyze" --dependencies "systemd>=1.0_1" ../pkg
	atf_check -o ignore -e ignore -- xbps-rindex -a *.xbps
	cd ..

	atf_check \
		-o match:"systemd-libudev-1\.0_1 install" \
		-o match:"systemd-analyze-1\.0_1 install" \
		-o match:"systemd-1\.0_1 install" \
		-- xbps-install -r root -R repo -n systemd

	atf_check \
		-o match:"base-system-1\.0_1 install" \
		-o match:"systemd-libudev-1\.0_1 install" \
		-o match:"systemd-analyze-1\.0_1 install" \
		-o match:"systemd-1\.0_1 install" \
		-- xbps-install -r root -R repo -n base-system
}

atf_test_case cyclic_dep_update

cyclic_dep_update_head() {
	atf_set "descr" "Tests for cyclic deps: test cylic deps during updates"
}

cyclic_dep_update_body() {
	mkdir repo pkg

	cd repo
	atf_check -o ignore -- xbps-create -A noarch -n base-system-1.0_1 -s "base-system" --dependencies "systemd>=0" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n systemd-1.0_1 -s "systemd" ../pkg
	atf_check -o ignore -e ignore -- xbps-rindex -a *.xbps
	cd ..

	atf_check \
		-o match:"systemd-1\.0_1: installed successfully" \
		-o match:"base-system-1\.0_1: installed successfully" \
		-- xbps-install -r root -R repo -y base-system

	cd repo
	atf_check -o ignore -- xbps-create -A noarch -n base-system-2.0_1 -s "base-system" --dependencies "systemd>=0" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n systemd-2.0_1 -s "systemd" --dependencies "systemd-libudev>=0 systemd-analyze>=0" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n systemd-libudev-2.0_1 -s "systemd-libudev" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n systemd-analyze-2.0_1 -s "systemd-analyze" --dependencies "systemd>=2.0_1" ../pkg
	atf_check -o ignore -e ignore -- xbps-rindex -a *.xbps
	cd ..

	atf_check \
		-o match:"base-system-2\.0_1 update" \
		-- xbps-install -r root -R repo -n -u base-system

	atf_check \
		-o match:"systemd-libudev-2\.0_1 install" \
		-o match:"systemd-analyze-2\.0_1 install" \
		-o match:"systemd-2\.0_1 update" \
		-- xbps-install -r root -R repo -n -u systemd
		# XXX: xbps automatically updates revdeps...
		# -o not-match:"base-system-2\.0_1 update"

	atf_check \
		-o match:"base-system-2\.0_1 update" \
		-o match:"systemd-libudev-2\.0_1 install" \
		-o match:"systemd-analyze-2\.0_1 install" \
		-o match:"systemd-2\.0_1 update" \
		-- xbps-install -r root -R repo -n -u

	cd repo
	atf_check -o ignore -- xbps-create -A noarch -n base-system-3.0_1 -s "base-system" --dependencies "systemd>=2" ../pkg
	atf_check -o ignore -e ignore -- xbps-rindex -a *.xbps
	cd ..

	atf_check \
		-o match:"base-system-3\.0_1 update" \
		-o match:"systemd-libudev-2\.0_1 install" \
		-o match:"systemd-analyze-2\.0_1 install" \
		-o match:"systemd-2\.0_1 update" \
		-- xbps-install -r root -R repo -n -u base-system

	atf_check \
		-o match:"systemd-libudev-2\.0_1 install" \
		-o match:"systemd-analyze-2\.0_1 install" \
		-o match:"systemd-2\.0_1 update" \
		-- xbps-install -r root -R repo -n -u systemd
		# XXX: xbps automatically updates revdeps...
		# -o not-match:"base-system-3\.0_1 update" 

	atf_check \
		-o match:"base-system-3\.0_1 update" \
		-o match:"systemd-libudev-2\.0_1 install" \
		-o match:"systemd-analyze-2\.0_1 install" \
		-o match:"systemd-2\.0_1 update" \
		-- xbps-install -r root -R repo -n -u
}

atf_test_case cyclic_dep_nested

cyclic_dep_nested_head() {
	atf_set "descr" "Tests for cyclic deps: nested cyclic deps"
}

cyclic_dep_nested_body() {
	mkdir -p repo pkg

	cd repo
	atf_check -o ignore -- xbps-create -A noarch -n A-1.0_1 -s "A pkg" --dependencies "B>=1" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "C>=1" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n C-1.0_1 -s "C pkg" --dependencies "A>=1" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n D-1.0_1 -s "D pkg" --dependencies "E>=1" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n E-1.0_1 -s "E pkg" --dependencies "A>=1 D>=1" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n F-1.0_1 -s "F pkg" --dependencies "E>=1" ../pkg
	atf_check -o ignore -e ignore -- xbps-rindex -d -a $PWD/*.xbps
	cd ..

	atf_check \
		-o match:"A-1\.0_1 install" \
		-o match:"B-1\.0_1 install" \
		-o match:"C-1\.0_1 install" \
		-- xbps-install -r root -R repo -n -u A
	atf_check \
		-o match:"C-1\.0_1 install" \
		-o match:"B-1\.0_1 install" \
		-o match:"A-1\.0_1 install" \
		-o match:"E-1\.0_1 install" \
		-o match:"D-1\.0_1 install" \
		-- xbps-install -r root -R repo -n -u D
	atf_check \
		-o match:"C-1\.0_1 install" \
		-o match:"B-1\.0_1 install" \
		-o match:"A-1\.0_1 install" \
		-o match:"E-1\.0_1 install" \
		-o match:"D-1\.0_1 install" \
		-o match:"F-1\.0_1 install" \
		-- xbps-install -r root -R repo -n -u F
}

atf_test_case cyclic_dep_incompatible

cyclic_dep_incompatible_head() {
	atf_set "descr" "Tests for cyclic deps: incompatible cyclic dep"
}

cyclic_dep_incompatible_body() {
	mkdir -p repo pkg

	cd repo
	atf_check -o ignore -- xbps-create -A noarch -n A-1.0_1 -s "A pkg" --dependencies "B>=1" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "C>=1" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n C-1.0_1 -s "C pkg" --dependencies "A>=2" ../pkg
	atf_check -o ignore -e ignore -- xbps-rindex -d -a $PWD/*.xbps
	cd ..

	atf_check \
		-s exit:19 \
		-e match:"MISSING: A>=2" \
		-- xbps-install -r root -R repo -n -u A
}

atf_init_test_cases() {
	atf_add_test_case cyclic_dep_vpkg
	atf_add_test_case cyclic_dep_vpkg2
	atf_add_test_case cyclic_dep_vpkg3
	atf_add_test_case cyclic_dep_full
	atf_add_test_case cyclic_dep_of_dep
	atf_add_test_case cyclic_dep_update
	atf_add_test_case cyclic_dep_nested
	atf_add_test_case cyclic_dep_incompatible
}
