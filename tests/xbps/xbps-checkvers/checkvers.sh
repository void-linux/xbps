#! /usr/bin/env atf-sh

atf_test_case srcpkg_older

srcpkg_older_head() {
	atf_set "descr" "xbps-checkvers(8): test when srcpkg contains an older version"
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
	out=`xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages`
	atf_check_equal $? 0
	atf_check_equal "$out" ""
}

atf_test_case srcpkg_older_with_refs

srcpkg_older_with_refs_head() {
	atf_set "descr" "xbps-checkvers(8): test when srcpkg contains an older version with refs"
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
	out=`xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages`
	atf_check_equal $? 0
	atf_check_equal "$out" ""
}

atf_test_case srcpkg_newer

srcpkg_newer_head() {
	atf_set "descr" "xbps-checkvers(8): test when srcpkg contains a newer version"
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
	out=`xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages`
	atf_check_equal $? 0
	atf_check_equal "$out" "pkgname: A repover: 1.0_1 srcpkgver: 1.1_1"
}

atf_test_case srcpkg_newer_with_refs

srcpkg_newer_with_refs_head() {
	atf_set "descr" "xbps-checkvers(8): test when srcpkg contains a newer version with refs"
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
	out=`xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages`
	atf_check_equal $? 0
	atf_check_equal "$out" "pkgname: A repover: 1.0_1 srcpkgver: 1.1_1"
}

atf_test_case srcpkg_newer_with_refs_and_source

srcpkg_newer_with_refs_and_source_head() {
	atf_set "descr" "xbps-checkvers(8): test when srcpkg contains a newer version with refs and file sourcing"
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
	out=`xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages`
	atf_check_equal $? 0
	atf_check_equal "$out" "pkgname: A repover: 1.0_1 srcpkgver: 1.1_1"
}

atf_test_case srcpkg_missing_pkgname

srcpkg_missing_pkgname_head() {
	atf_set "descr" "xbps-checkvers(8): test when srcpkg does not set pkgname"
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
	atf_check_equal "$(cat out)" "ERROR: 'A/template': missing pkgname variable!"
}

atf_test_case srcpkg_missing_version

srcpkg_missing_version_head() {
	atf_set "descr" "xbps-checkvers(8): test when srcpkg does not set version"
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
	atf_check_equal "$(cat out)" "ERROR: 'A/template': missing version variable!"
}

atf_test_case srcpkg_missing_revision

srcpkg_missing_revision_head() {
	atf_set "descr" "xbps-checkvers(8): test when srcpkg does not set revision"
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
	atf_check_equal "$(cat out)" "ERROR: 'A/template': missing revision variable!"
}

atf_test_case srcpkg_missing_pkgver

srcpkg_missing_pkgver_head() {
	atf_set "descr" "xbps-checkvers(8): test when srcpkg does not set pkgname/version"
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
	atf_check_equal "$(cat out)" "ERROR: 'A/template': missing pkgname variable!"
}

atf_test_case srcpkg_missing_pkgverrev

srcpkg_missing_pkgverrev_head() {
	atf_set "descr" "xbps-checkvers(8): test when srcpkg does not set pkgname/version/revision"
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
	atf_check_equal "$(cat out)" "ERROR: 'A/template': missing pkgname variable!"
}

atf_test_case srcpkg_with_a_ref

srcpkg_with_a_ref_head() {
	atf_set "descr" "xbps-checkvers(8): test when srcpkg does set a ref in pkgname/version/revision"
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
	atf_check_equal "$(cat out)" "pkgname: foo repover: 1.0_1 srcpkgver: 1.1_1"
}

atf_test_case srcpkg_with_a_ref_and_comment

srcpkg_with_a_ref_and_comment_head() {
	atf_set "descr" "xbps-checkvers(8): test when srcpkg does set a ref in pkgname/version/revision with a comment"
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
	atf_check_equal "$(cat out)" "pkgname: foo repover: 1.0_1 srcpkgver: 1.1_1"
}

atf_test_case reverts

reverts_head() {
	atf_set "descr" "xbps-checkvers(8): test with reverts"
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
	out=`xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages`
	atf_check_equal $? 0
	atf_check_equal "$out" "pkgname: A repover: 1.1_1 srcpkgver: 1.0_1"
}

atf_test_case reverts_alpha

reverts_alpha_head() {
	atf_set "descr" "xbps-checkvers(8): test with reverts containing an alphanumeric character"
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
	out=`xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages`
	atf_check_equal $? 0
	atf_check_equal "$out" "pkgname: fs-utils repover: 1.10_1 srcpkgver: 1.10_1"
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
}
