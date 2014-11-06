#! /usr/bin/env atf-sh

atf_test_case install_existent

checkvers_older_head() {
	atf_set "descr" "xbps-checkvers(8): test checkversion when repo is older"
}

checkvers_older_body() {
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
	xbps-rindex -d -a *.xbps
	atf_check_equal $? 0
	cd ..
	out=`xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages`
	atf_check_equal $? 0
	atf_check_equal "$out" ""
}

checkvers_reverts_head() {
	atf_set "descr" "xbps-checkvers(8): test checkversion with reverts"
}

checkvers_reverts_body() {
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
	xbps-rindex -d -a *.xbps
	atf_check_equal $? 0
	cd ..
	out=`xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages`
	atf_check_equal $? 0
	atf_check_equal "$out" "pkgname: A repover: 1.1_1 srcpkgver: 1.0_1"
}

checkvers_newer_head() {
	atf_set "descr" "xbps-checkvers(8): test checkversion with newer version"
}

checkvers_newer_body() {
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
	xbps-rindex -d -a *.xbps
	atf_check_equal $? 0
	cd ..
	out=`xbps-checkvers -R $PWD/some_repo -D $PWD/void-packages`
	atf_check_equal $? 0
	atf_check_equal "$out" "pkgname: A repover: 1.0_1 srcpkgver: 1.1_1"
}

atf_init_test_cases() {
	atf_add_test_case checkvers_older
	atf_add_test_case checkvers_reverts
	atf_add_test_case checkvers_newer
}
