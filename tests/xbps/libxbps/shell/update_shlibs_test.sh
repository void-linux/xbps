#!/usr/bin/env atf-sh
#
# 1st case:
#	- A is updated and contains a soname bump
#	- A revdeps are broken
#	- ENOEXEC expected (unresolved shlibs)

atf_test_case shlib_bump

shlib_bump_head() {
	atf_set "descr" "Tests for pkg updates: update pkg with soname bump"
}

shlib_bump_body() {
	mkdir -p repo pkg_A pkg_B
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" --shlib-provides "libfoo.so.1" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "A>=0" --shlib-requires "libfoo.so.1" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/repo -yvd B
	atf_check_equal $? 0

	cd repo
	xbps-create -A noarch -n A-2.0_1 -s "A pkg" --shlib-provides "libfoo.so.2" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	# returns ENOEXEC if there are unresolved shlibs
	xbps-install -C empty.conf -r root --repository=$PWD/repo -yuvd A
	atf_check_equal $? 8
}

# 2nd case:
#	- A is updated and contains a soname bump
#	- A revdeps are broken but there are updates in transaction

atf_test_case shlib_bump_revdep_in_trans

shlib_bump_revdep_in_trans_head() {
	atf_set "descr" "Tests for pkg updates: update pkg with soname bump, updated revdep in trans"
}

shlib_bump_revdep_in_trans_body() {
	mkdir -p repo pkg_A pkg_B
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" --shlib-provides "libfoo.so.1" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "A>=1.0" --shlib-requires "libfoo.so.1" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/repo -yvd B
	atf_check_equal $? 0

	cd repo
	xbps-create -A noarch -n A-2.0_1 -s "A pkg" --shlib-provides "libfoo.so.2" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-2.0_1 -s "B pkg" --dependencies "A>=0" --shlib-requires "libfoo.so.2" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/repo -yuvd
	atf_check_equal $? 0
	atf_check_equal $(xbps-query -C empty.conf -r root -ppkgver A) A-2.0_1
	atf_check_equal $(xbps-query -C empty.conf -r root -ppkgver B) B-2.0_1
}

# 3rd case:
#	- A is updated and contains a soname bump
#	- A revdeps are broken and there are broken updates in transaction
#	- ENOEXEC expected (unresolved shlibs)

atf_test_case shlib_bump_incomplete_revdep_in_trans

shlib_bump_incomplete_revdep_in_trans_head() {
	atf_set "descr" "Tests for pkg updates: update pkg with soname bump, updated revdep in trans with unresolved shlibs"
}

shlib_bump_incomplete_revdep_in_trans_body() {
	mkdir -p repo pkg_A pkg_B
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" --shlib-provides "libfoo.so.1" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "A>=1.0" --shlib-requires "libfoo.so.1" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/repo -yvd B
	atf_check_equal $? 0

	cd repo
	xbps-create -A noarch -n A-2.0_1 -s "A pkg" --shlib-provides "libfoo.so.2" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.1_1 -s "B pkg" --dependencies "A>=0" --shlib-requires "libfoo.so.1" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/repo -yuvd
	atf_check_equal $? 8
}

atf_test_case shlib_bump_revdep_diff

shlib_bump_revdep_diff_head() {
	atf_set "descr" "Tests for pkg updates: revdep does not require previous shlibs"
}

shlib_bump_revdep_diff_body() {
	mkdir -p repo pkg
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" --shlib-provides "liba.so.1" ../pkg
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --shlib-provides "libb.so.1" ../pkg
	atf_check_equal $? 0
	xbps-create -A noarch -n C-1.0_1 -s "C pkg" --dependencies "A>=0 B>=0" --shlib-requires "liba.so.1 libb.so.1" ../pkg
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/repo -yvd C
	atf_check_equal $? 0

	cd repo
	xbps-create -A noarch -n A-2.0_1 -s "A pkg" --shlib-provides "liba.so.2" ../pkg
	atf_check_equal $? 0
	xbps-create -A noarch -n B-2.0_1 -s "B pkg" --shlib-provides "libb.so.2" ../pkg
	atf_check_equal $? 0
	xbps-create -A noarch -n C-2.0_1 -s "C pkg" --dependencies "A>=2.0" --shlib-requires "liba.so.2" ../pkg
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/repo -yuvd
	atf_check_equal $? 0
	atf_check_equal $(xbps-query -C empty.conf -r root -ppkgver A) A-2.0_1
	atf_check_equal $(xbps-query -C empty.conf -r root -ppkgver B) B-2.0_1
	atf_check_equal $(xbps-query -C empty.conf -r root -ppkgver C) C-2.0_1
}

atf_test_case shlib_bump_versioned

shlib_bump_versioned_head() {
	atf_set "descr" "Tests for pkg updates: pkg update with a versioned soname bump"
}

shlib_bump_versioned_body() {
	mkdir -p repo pkg
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" --shlib-provides "liba-0.so" ../pkg
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "A>=0" --shlib-requires "liba-0.so" ../pkg
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/repo -yvd B
	atf_check_equal $? 0

	cd repo
	xbps-create -A noarch -n A-2.0_1 -s "A pkg" --shlib-provides "liba-1.so" ../pkg
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/repo -yuvd
	# ENOEXEC == unresolved shlibs
	atf_check_equal $? 8
}

atf_test_case shlib_unknown_provider

shlib_unknown_provider_head() {
	atf_set "descr" "Tests for pkg updates: pkg update with unknown shlib provider"
}

shlib_unknown_provider_body() {
	mkdir -p repo pkg
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" --shlib-provides "liba-0.so" ../pkg
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "A>=0" --shlib-requires "liba-0.so" ../pkg
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/repo -yvd B
	atf_check_equal $? 0

	cd repo
	xbps-create -A noarch -n A-2.0_1 -s "A pkg" ../pkg
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/repo -yuvd
	# ENOEXEC == unresolved shlibs
	atf_check_equal $? 8
}

atf_test_case shlib_provides_replaces

shlib_provides_replaces_head() {
	atf_set "descr" "Tests for pkg updates: replace pkg with shlib provided"
}

shlib_provides_replaces_body() {
	mkdir -p repo pkg_A pkg_B
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" --shlib-provides "libfoo.so.1" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "A>=1.0" --shlib-requires "libfoo.so.1" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/repo -yvd B
	atf_check_equal $? 0

	cd repo
	xbps-create -A noarch -n B-2.0_1 -s "B pkg" --dependencies "C>=0" --shlib-requires "libfoo.so.1" ../pkg_B
	atf_check_equal $? 0
	xbps-create -A noarch -n C-2.0_1 -s "C pkg" --replaces "A>=1.0" --shlib-provides "libfoo.so.1" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/repo -yuvd
	atf_check_equal $? 0
	atf_check_equal $(xbps-query -C empty.conf -r root -ppkgver B) B-2.0_1
	atf_check_equal $(xbps-query -C empty.conf -r root -ppkgver C) C-2.0_1
	xbps-query -C empty.conf -r root A
	atf_check_equal $? 2
}

atf_test_case shlib_32bit

shlib_32bit_head() {
	atf_set "descr" "32bit shlib-provides does not satisfy non-32bit shlib-requires"
}

shlib_32bit_body() {
	mkdir -p repo pkg_A pkg_B
	cd repo
	atf_check -o ignore -- xbps-create -A noarch -n A-1.0_1 -s "A pkg" --shlib-provides "libfoo.so.1" ../pkg_A
	atf_check -o ignore -- xbps-create -A noarch -n A-32bit-1.0_1 -s "A pkg" --shlib-provides "libfoo.so.1" ../pkg_A
	atf_check -o ignore -- xbps-create -A noarch -n B-1.0_1 -s "B-32bit pkg" --shlib-requires "libfoo.so.1" ../pkg_B
	atf_check -o ignore -- xbps-create -A noarch -n B-32bit-1.0_1 -s "B-32bit pkg" --shlib-requires "libfoo.so.1" ../pkg_B
	atf_check -o ignore -e ignore -- xbps-rindex -a $PWD/*.xbps
	cd ..

	atf_check \
		-s exit:8 \
		-e match:"B-32bit-1\.0_1: broken, unresolvable shlib \`libfoo\.so\.1'" \
		-- xbps-install -R repo -r root -ny A B-32bit
	atf_check \
		-s exit:8 \
		-e match:"B-1\.0_1: broken, unresolvable shlib \`libfoo\.so\.1'" \
		-- xbps-install -R repo -r root -ny A-32bit B
}

atf_test_case shlib_bump_32bit

shlib_bump_32bit_head() {
	atf_set "descr" "Tests for pkg updates: update pkg with 32bit soname bump"
}

shlib_bump_32bit_body() {
	mkdir -p repo pkg_A pkg_B
	cd repo
	atf_check -o ignore -- xbps-create -A noarch -n A-1.0_1 -s "A pkg" --shlib-provides "libfoo.so.1" ../pkg_A
	atf_check -o ignore -- xbps-create -A noarch -n A-32bit-1.0_1 -s "A-32bit pkg" --shlib-provides "libfoo.so.1" ../pkg_A
	atf_check -o ignore -- xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "A>=0" --shlib-requires "libfoo.so.1" ../pkg_B
	atf_check -o ignore -- xbps-create -A noarch -n B-32bit-1.0_1 -s "B-32bit pkg" --dependencies "A-32bit>=0" --shlib-requires "libfoo.so.1" ../pkg_B
	atf_check -o ignore -e ignore -- xbps-rindex -a $PWD/*.xbps
	cd ..

	atf_check -o ignore -e ignore -- xbps-install -C empty.conf -r root --repository=$PWD/repo -y B B-32bit

	cd repo
	atf_check -o ignore -- xbps-create -A noarch -n A-2.0_1 -s "A pkg" --shlib-provides "libfoo.so.2" ../pkg_A
	atf_check -o ignore -e ignore -- xbps-rindex -a $PWD/*.xbps
	cd ..

	# returns ENOEXEC if there are unresolved shlibs
	atf_check \
		-s exit:8 \
		-e match:"B-1\.0_1: broken, unresolvable shlib \`libfoo\.so\.1'" \
		-- xbps-install -C empty.conf -r root --repository=$PWD/repo -yu A A-32bit

	cd repo
	atf_check -o ignore -- xbps-create -A noarch -n A-32bit-2.0_1 -s "A-32bit pkg" --shlib-provides "libfoo.so.2" ../pkg_A
	atf_check -o ignore -- xbps-create -A noarch -n B-1.0_2 -s "B pkg" --dependencies "A>=0" --shlib-requires "libfoo.so.2" ../pkg_B
	atf_check -o ignore -e ignore -- xbps-rindex -a $PWD/*.xbps
	cd ..

	# returns ENOEXEC if there are unresolved shlibs
	atf_check \
		-s exit:8 \
		-e match:"B-32bit-1\.0_1: broken, unresolvable shlib \`libfoo\.so\.1'" \
		-- xbps-install -C empty.conf -r root --repository=$PWD/repo -yu A A-32bit
}

atf_init_test_cases() {
	atf_add_test_case shlib_bump
	atf_add_test_case shlib_bump_incomplete_revdep_in_trans
	atf_add_test_case shlib_bump_revdep_in_trans
	atf_add_test_case shlib_bump_revdep_diff
	atf_add_test_case shlib_bump_versioned
	atf_add_test_case shlib_unknown_provider
	atf_add_test_case shlib_provides_replaces
	atf_add_test_case shlib_32bit
	atf_add_test_case shlib_bump_32bit
}
