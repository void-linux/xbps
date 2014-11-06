#!/usr/bin/env atf-sh
#
# Tests to verify that INSTALL/REMOVE scripts in pkgs work as expected.

create_script() {
	cat > "$1" <<_EOF
#!/bin/sh
ACTION="\$1"
PKGNAME="\$2"
VERSION="\$3"
UPDATE="\$4"
CONF_FILE="\$5"
ARCH="\$6"

echo "\$@" >&2
_EOF
	chmod +x "$1"
}

atf_test_case script_nargs

script_nargs_head() {
	atf_set "descr" "Tests for package scripts: number of arguments"
}

script_nargs_body() {
	mkdir some_repo root
	mkdir -p pkg_A/usr/bin
	echo "A-1.0_1" > pkg_A/usr/bin/foo
	create_script pkg_A/INSTALL

	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -y A
	atf_check_equal $? 0

	rval=0
	xbps-reconfigure -C empty.conf -r root -f A 2>out
	out="$(cat out)"
	expected="post A 1.0_1 no no $(uname -m)"
	if [ "$out" != "$expected" ]; then
		echo "out: '$out'"
		echo "expected: '$expected'"
		rval=1
	fi
	atf_check_equal $rval 0
}

atf_test_case script_arch

script_arch_head() {
	atf_set "descr" "Tests for package scripts: XBPS_ARCH overrides \$ARCH"
}

script_arch_body() {
	mkdir some_repo root
	mkdir -p pkg_A/usr/bin
	echo "A-1.0_1" > pkg_A/usr/bin/foo
	create_script pkg_A/INSTALL

	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -y A
	atf_check_equal $? 0

	# Check that XBPS_ARCH overrides $ARCH.
	rval=0
	XBPS_ARCH=foo xbps-reconfigure -C empty.conf -r root -f A 2>out
	out="$(cat out)"
	expected="post A 1.0_1 no no foo"
	if [ "$out" != "$expected" ]; then
		echo "out: '$out'"
		echo "expected: '$expected'"
		rval=1
	fi
	atf_check_equal $rval 0
}

atf_init_test_cases() {
	atf_add_test_case script_nargs
	atf_add_test_case script_arch
}
