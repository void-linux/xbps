#! /usr/bin/env atf-sh

atf_test_case srcpkg_older

srcpkg_older_head() {
	atf_set "descr" "xbps-checkvers(1): test when srcpkg contains an older version"
}
srcpkg_older_body() {
	mkdir -p some_repo pkg_A void-packages/srcpkgs/A
	touch pkg_A/file00
	cat > void-packages/srcpkgs/A/template <<EOF
pkgname=A
version=1.0
revision=1
do_install() {
	:
}
EOF
	cd some_repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	out=$(xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages)
	atf_check_equal $? 0
	atf_check_equal "$out" ""
}

atf_test_case srcpkg_older_with_refs

srcpkg_older_with_refs_head() {
	atf_set "descr" "xbps-checkvers(1): test when srcpkg contains an older version with refs"
}
srcpkg_older_with_refs_body() {
	mkdir -p some_repo pkg_A void-packages/srcpkgs/A
	touch pkg_A/file00
	cat > void-packages/srcpkgs/A/template <<EOF
_mypkgname=A
_myversion=1
pkgname=\${_mypkgname}
version=\${_myversion}.0
revision=1
do_install() {
	:
}
EOF
	cd some_repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	out=$(xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages)
	atf_check_equal $? 0
	atf_check_equal "$out" ""
}

atf_test_case srcpkg_newer

srcpkg_newer_head() {
	atf_set "descr" "xbps-checkvers(1): test when srcpkg contains a newer version"
}
srcpkg_newer_body() {
	mkdir -p some_repo pkg_A void-packages/srcpkgs/A
	touch pkg_A/file00
	cat > void-packages/srcpkgs/A/template <<EOF
pkgname=A
version=1.1
revision=1
do_install() {
	:
}
EOF
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	out=$(xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages)
	atf_check_equal $? 0
	atf_check_equal "$out" "A 1.0_1 1.1_1 A $PWD/some_repo"
}

atf_test_case srcpkg_newer_with_refs

srcpkg_newer_with_refs_head() {
	atf_set "descr" "xbps-checkvers(1): test when srcpkg contains a newer version with refs"
}
srcpkg_newer_with_refs_body() {
	mkdir -p some_repo pkg_A void-packages/srcpkgs/A
	touch pkg_A/file00
	cat > void-packages/srcpkgs/A/template <<EOF
_mypkgname="A"
_myversion="1"
pkgname=\${_mypkgname}
version=\${_myversion}.1
revision=1
do_install() {
	:
}
EOF
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	out=$(xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages)
	atf_check_equal $? 0
	atf_check_equal "$out" "A 1.0_1 1.1_1 A $PWD/some_repo"
}

atf_test_case srcpkg_newer_with_refs_and_source

srcpkg_newer_with_refs_and_source_head() {
	atf_set "descr" "xbps-checkvers(1): test when srcpkg contains a newer version with refs and file sourcing"
}
srcpkg_newer_with_refs_and_source_body() {
	mkdir -p some_repo pkg_A void-packages/srcpkgs/A
	touch pkg_A/file00
	cat > void-packages/srcpkgs/A/template <<EOF
. ./source/a/file
_mypkgname="A"
_myversion="1"
pkgname=\${_mypkgname}
version=\${_myversion}.1
revision=1
do_install() {
	:
}
EOF
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	out=$(xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages)
	atf_check_equal $? 0
	atf_check_equal "$out" "A 1.0_1 1.1_1 A $PWD/some_repo"
}

atf_test_case srcpkg_missing_pkgname

srcpkg_missing_pkgname_head() {
	atf_set "descr" "xbps-checkvers(1): test when srcpkg does not set pkgname"
}
srcpkg_missing_pkgname_body() {
	mkdir -p some_repo pkg_A void-packages/srcpkgs/A
	cat > void-packages/srcpkgs/A/template <<EOF
#pkgname=A
version=1.1
revision=1
do_install() {
	:
}
EOF
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages 2>out
	atf_check_equal $? 1
	atf_check_equal "$(cat out)" "ERROR: 'A/template': missing required variable (pkgname, version or revision)!"
}

atf_test_case srcpkg_missing_version

srcpkg_missing_version_head() {
	atf_set "descr" "xbps-checkvers(1): test when srcpkg does not set version"
}
srcpkg_missing_version_body() {
	mkdir -p some_repo pkg_A void-packages/srcpkgs/A
	cat > void-packages/srcpkgs/A/template <<EOF
pkgname=A
#version=1.1
revision=1
do_install() {
	:
}
EOF
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages 2>out
	atf_check_equal $? 1
	atf_check_equal "$(cat out)" "ERROR: 'A/template': missing required variable (pkgname, version or revision)!"
}

atf_test_case srcpkg_missing_revision

srcpkg_missing_revision_head() {
	atf_set "descr" "xbps-checkvers(1): test when srcpkg does not set revision"
}
srcpkg_missing_revision_body() {
	mkdir -p some_repo pkg_A void-packages/srcpkgs/A
	cat > void-packages/srcpkgs/A/template <<EOF
pkgname=A
version=1.1
#revision=1
do_install() {
	:
}
EOF
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages 2>out
	atf_check_equal $? 1
	atf_check_equal "$(cat out)" "ERROR: 'A/template': missing required variable (pkgname, version or revision)!"
}

atf_test_case srcpkg_missing_pkgver

srcpkg_missing_pkgver_head() {
	atf_set "descr" "xbps-checkvers(1): test when srcpkg does not set pkgname/version"
}
srcpkg_missing_pkgver_body() {
	mkdir -p some_repo pkg_A void-packages/srcpkgs/A
	cat > void-packages/srcpkgs/A/template <<EOF
#pkgname=A
#version=1.1
revision=1
do_install() {
	:
}
EOF
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages 2>out
	atf_check_equal $? 1
	atf_check_equal "$(cat out)" "ERROR: 'A/template': missing required variable (pkgname, version or revision)!"
}

atf_test_case srcpkg_missing_pkgverrev

srcpkg_missing_pkgverrev_head() {
	atf_set "descr" "xbps-checkvers(1): test when srcpkg does not set pkgname/version/revision"
}
srcpkg_missing_pkgverrev_body() {
	mkdir -p some_repo pkg_A void-packages/srcpkgs/A
	cat > void-packages/srcpkgs/A/template <<EOF
#pkgname=A
#version=1.1
revision=1
do_install() {
	:
}
EOF
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages 2>out
	atf_check_equal $? 1
	atf_check_equal "$(cat out)" "ERROR: 'A/template': missing required variable (pkgname, version or revision)!"
}

atf_test_case srcpkg_with_a_ref

srcpkg_with_a_ref_head() {
	atf_set "descr" "xbps-checkvers(1): test when srcpkg does set a ref in pkgname/version/revision"
}
srcpkg_with_a_ref_body() {
	mkdir -p some_repo pkg_A void-packages/srcpkgs/foo
	cat > void-packages/srcpkgs/foo/template <<EOF
_var=foo
pkgname=\${_var}
version=1.1
revision=1
do_install() {
	:
}
EOF
	cd some_repo
	xbps-create -A noarch -n foo-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages >out
	atf_check_equal $? 0
	atf_check_equal "$(cat out)" "foo 1.0_1 1.1_1 foo $PWD/some_repo"
}

atf_test_case srcpkg_with_a_ref_and_comment

srcpkg_with_a_ref_and_comment_head() {
	atf_set "descr" "xbps-checkvers(1): test when srcpkg does set a ref in pkgname/version/revision with a comment"
}
srcpkg_with_a_ref_and_comment_body() {
	mkdir -p some_repo pkg_A void-packages/srcpkgs/foo
	cat > void-packages/srcpkgs/foo/template <<EOF
pkgname=foo #kajskajskajskajskajskjaksjaksjaks
version=1.1
revision=1
do_install() {
	:
}
EOF
	cd some_repo
	xbps-create -A noarch -n foo-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages >out
	atf_check_equal $? 0
	atf_check_equal "$(cat out)" "foo 1.0_1 1.1_1 foo $PWD/some_repo"
}

atf_test_case reverts

reverts_head() {
	atf_set "descr" "xbps-checkvers(1): test with reverts"
}

reverts_body() {
	mkdir -p some_repo pkg_A void-packages/srcpkgs/A
	touch pkg_A/file00
	cat > void-packages/srcpkgs/A/template <<EOF
pkgname=A
reverts="1.1_1"
version=1.0
revision=1
do_install() {
	:
}
EOF
	cd some_repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	out=$(xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages)
	atf_check_equal $? 0
	atf_check_equal "$out" "A 1.1_1 1.0_1 A $PWD/some_repo"
}

atf_test_case reverts_alpha

reverts_alpha_head() {
	atf_set "descr" "xbps-checkvers(1): test with reverts containing an alphanumeric character"
}

reverts_alpha_body() {
	mkdir -p some_repo pkg_A void-packages/srcpkgs/fs-utils
	touch pkg_A/file00
	cat > void-packages/srcpkgs/fs-utils/template <<EOF
pkgname=fs-utils
reverts=v1.10_1
version=1.10
revision=1
do_install() {
	:
}
EOF
	cd some_repo
	xbps-create -A noarch -n fs-utils-1.10_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages
	out=$(xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages)
	atf_check_equal $? 0
	atf_check_equal "$out" ""
}

atf_test_case reverts_many

reverts_many_head() {
	atf_set "descr" "xbps-checkvers(1): test with multiple reverts"
}

reverts_many_body() {
	mkdir -p some_repo pkg_A void-packages/srcpkgs/A
	touch pkg_A/file00
	cat > void-packages/srcpkgs/A/template <<EOF
pkgname=A
reverts="1.1_1 1.2_1 1.3_1 1.3_2 1.3_3 1.3_4"
version=1.0
revision=1
do_install() {
	:
}
EOF
	cd some_repo
	xbps-create -A noarch -n A-1.2_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	out=$(xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages)
	atf_check_equal $? 0
	atf_check_equal "$out" "A 1.2_1 1.0_1 A $PWD/some_repo"

	cd some_repo
	rm *.xbps
	xbps-rindex -c .
	atf_check_equal $? 0
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	out=$(xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages)
	atf_check_equal $? 0
	atf_check_equal "$out" "A 1.1_1 1.0_1 A $PWD/some_repo"

	cd some_repo
	rm *.xbps
	xbps-rindex -c .
	atf_check_equal $? 0
	xbps-create -A noarch -n A-1.3_4 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	out=$(xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages)
	atf_check_equal $? 0
	atf_check_equal "$out" "A 1.3_4 1.0_1 A $PWD/some_repo"
}

atf_test_case manual_mode

manual_mode_head() {
	atf_set "descr" "xbps-checkvers(1): test manual mode"
}

manual_mode_body() {
	mkdir -p void-packages/srcpkgs/do-not-process void-packages/srcpkgs/process
	cat > void-packages/srcpkgs/do-not-process/template <<EOF
pkgname=do-not-process
#version=1
revision=1
EOF
	cat > void-packages/srcpkgs/process/template <<EOF
pkgname=process
version=1
revision=1
EOF
	out=$(xbps-checkvers -D $PWD/void-packages -ms srcpkgs/process/template)
	atf_check_equal $? 0
	atf_check_equal "$out" "process ? 1_1 process ?"
}

atf_test_case subpkg

subpkg_head() {
	atf_set "descr" "xbps-checkvers(1): test subpkgs"
}
subpkg_body() {
	mkdir -p repo pkg_A void-packages/srcpkgs/A
	touch pkg_A/file00
	cat > void-packages/srcpkgs/A/template <<EOF
pkgname=A
version=1.0
revision=1
}
EOF
	ln -s A void-packages/srcpkgs/B
	ln -s A void-packages/srcpkgs/C
	cd repo
	xbps-create -A noarch -n B-1.1_1 -s "B pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	out=$(xbps-checkvers -i -R $PWD/repo -D $PWD/void-packages -sm B)
	atf_check_equal $? 0
	atf_check_equal "$out" "A 1.1_1 1.0_1 B $PWD/repo"

	out=$(xbps-checkvers -i -R repo -D $PWD/void-packages -sm C)
	atf_check_equal $? 0
	atf_check_equal "$out" "A ? 1.0_1 C ?"

	out=$(xbps-checkvers -i -R repo -D $PWD/void-packages -sm A)
	atf_check_equal $? 0
	atf_check_equal "$out" "A ? 1.0_1 A ?"
}

atf_test_case removed

removed_head() {
	atf_set "descr" "xbps-checkvers(1): test removed"
}

removed_body() {
	mkdir -p some_repo
	for pkg in same updated removed
	do
		mkdir -p void-packages/srcpkgs/$pkg
		cat > void-packages/srcpkgs/$pkg/template <<EOF
pkgname=$pkg
version=1
revision=1
EOF
		cd some_repo
		xbps-create -A noarch -n ${pkg}-1_1 -s "$pkg pkg" .
		atf_check_equal $? 0
		cd ..
	done

	sed -e s/version=1/version=2/ -i void-packages/srcpkgs/updated/template
	rm -r void-packages/srcpkgs/removed
	cd some_repo
	xbps-rindex -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	out=$(xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages -i)
	atf_check_equal $? 0
	atf_check_equal "$out" "updated 1_1 2_1 updated $PWD/some_repo"
	out=$(xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages -ei)
	atf_check_equal $? 0
	atf_check_equal "$out" "removed 1_1 ? removed $PWD/some_repo"
}

atf_test_case removed_subpkgs

removed_subpkgs_head() {
	atf_set "descr" "xbps-checkvers(1): handling of removed subpkgs"
}

removed_subpkgs_body() {
	mkdir -p some_repo
	for pkg in same updated removed onlybase onlydevel
	do
		mkdir -p void-packages/srcpkgs/$pkg
		ln -sr void-packages/srcpkgs/$pkg void-packages/srcpkgs/$pkg-devel
		cat > void-packages/srcpkgs/$pkg/template <<EOF
pkgname=$pkg
version=1
revision=1

$pkg-devel_package(){
	depends="${sourcepkg}-${version}_${revision}"
	pkg_install() {
		:
	}
}
EOF
		cd some_repo
		xbps-create -A noarch -n ${pkg}-1_1 -s "$pkg pkg" .
		xbps-create -A noarch -n ${pkg}-devel-1_1 -s "$pkg devel subpkg" .
		atf_check_equal $? 0
		cd ..
	done

	sed -e s/version=1/version=2/ -i void-packages/srcpkgs/updated/template
	rm -r void-packages/srcpkgs/onlydevel-devel
	mv void-packages/srcpkgs/onlydevel void-packages/srcpkgs/onlydevel-devel
	rm -r void-packages/srcpkgs/onlybase-devel
	rm -r void-packages/srcpkgs/removed
	rm -r void-packages/srcpkgs/removed-devel
	cd some_repo
	xbps-rindex -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	out=$(xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages -i)
	atf_check_equal $? 0
	atf_check_equal "$out" "updated 1_1 2_1 updated $PWD/some_repo"
	cat > expected <<EOF
onlybase-devel 1_1 ? onlybase-devel $PWD/some_repo
onlydevel 1_1 ? onlydevel $PWD/some_repo
removed 1_1 ? removed $PWD/some_repo
removed-devel 1_1 ? removed-devel $PWD/some_repo
EOF
	xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages -ei > out
	atf_check_equal $? 0
	sort < out > out.sorted
	atf_check_equal $? 0
	atf_check_equal "$(tr '\n' ' ' < out.sorted)" "$(tr '\n' ' ' < expected)"
}

atf_test_case multiline_reverts

multiline_reverts_head() {
	atf_set "descr" "xbps-checkvers(1): test with reverts newline characters"
}

multiline_reverts_body() {
	atf_expect_fail "multiline variables are not supported"
	mkdir -p some_repo pkg_A void-packages/srcpkgs/libdwarf
	touch pkg_A/file00
	cat > void-packages/srcpkgs/libdwarf/template <<EOF
pkgname=libdwarf
reverts="20201020_1 20200825_1 20200719_1 20200114_1 20191104_2 20191104_1
 20191002_1 20190529_1 20190110_1 20180809_1 20180527_1 20180129_1 20170709_2
 20170709_1 20170416_1 20161124_1 20161021_1 20161001_1 20160923_1 20160613_1
 20160507_1 20160115_1 20150507_3 20150507_2 20150507_1"
version=0.11.1
revision=1
do_install() {
	:
}
EOF
	cd some_repo
	xbps-create -A noarch -n libdwarf-0.11.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages
	out=$(xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages)
	atf_check_equal $? 0
	atf_check_equal "$out" ""
}

atf_init_test_cases() {
	atf_add_test_case srcpkg_newer
	atf_add_test_case srcpkg_newer_with_refs
	atf_add_test_case srcpkg_newer_with_refs_and_source
	atf_add_test_case srcpkg_older
	atf_add_test_case srcpkg_older_with_refs
	atf_add_test_case srcpkg_missing_pkgname
	atf_add_test_case srcpkg_missing_version
	atf_add_test_case srcpkg_missing_revision
	atf_add_test_case srcpkg_missing_pkgver
	atf_add_test_case srcpkg_missing_pkgverrev
	atf_add_test_case srcpkg_with_a_ref
	atf_add_test_case srcpkg_with_a_ref_and_comment
	atf_add_test_case reverts
	atf_add_test_case reverts_alpha
	atf_add_test_case reverts_many
	atf_add_test_case manual_mode
	atf_add_test_case subpkg
	atf_add_test_case removed
	atf_add_test_case removed_subpkgs
	atf_add_test_case multiline_reverts
}
