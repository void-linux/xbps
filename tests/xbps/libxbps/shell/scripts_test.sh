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

	unset XBPS_ARCH XBPS_TARGET_ARCH

	arch=$(xbps-uhelper -r root arch)
	cd some_repo
	atf_check -o ignore -- xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	# XXX: xbps-rindex has no root flag to ignore /usr/share/xbps.d/xbps-arch.conf
	XBPS_ARCH="$arch" atf_check -o ignore -- xbps-rindex -a $PWD/*.xbps
	cd ..
	atf_check -o ignore \
		-e inline:"pre A 1.0_1 no no ${arch}\npost A 1.0_1 no no ${arch}\n" -- \
		xbps-install -C empty.conf -r root --repository=$PWD/some_repo -y A

	atf_check -o ignore -e inline:"post A 1.0_1 no no ${arch}\n" -- \
		xbps-reconfigure -C empty.conf -r root -f A
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

create_script_stdout() {
	cat > "$2" <<_EOF
#!/bin/sh
ACTION="\$1"
PKGNAME="\$2"
VERSION="\$3"
UPDATE="\$4"
CONF_FILE="\$5"
ARCH="\$6"

echo "$1 \$@"
_EOF
	chmod +x "$2"
}

atf_test_case script_action

script_action_head() {
	atf_set "descr" "Tests for package scripts: different actions"
}

script_action_body() {
	mkdir some_repo root
	mkdir -p pkg_A/usr/bin
	echo "A-1.0_1" > pkg_A/usr/bin/foo
	create_script_stdout "install 1.0_1" pkg_A/INSTALL
	create_script_stdout "remove 1.0_1" pkg_A/REMOVE

	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -y A >out
	atf_check_equal $? 0

	grep "^install 1.0_1 pre A 1.0_1 no no" out
	atf_check_equal $? 0
	grep "^install 1.0_1 post A 1.0_1 no no" out
	atf_check_equal $? 0

	create_script_stdout "install 1.1_1" pkg_A/INSTALL
	create_script_stdout "remove 1.1_1" pkg_A/REMOVE
	cd some_repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -yu >out
	atf_check_equal $? 0

	grep "^remove 1.0_1 pre A 1.0_1 yes no" out
	atf_check_equal $? 0
	grep "^install 1.1_1 pre A 1.1_1 yes no" out
	atf_check_equal $? 0
	grep "^remove 1.0_1 post A 1.0_1 yes no" out
	atf_check_equal $? 1
	grep "^remove 1.0_1 purge A 1.0_1 yes no" out
	atf_check_equal $? 1
	grep "^install 1.1_1 post A 1.1_1 yes no" out
	atf_check_equal $? 0

	create_script_stdout "install 1.0_2" pkg_A/INSTALL
	create_script_stdout "remove 1.0_2" pkg_A/REMOVE
	cd some_repo
	xbps-create -A noarch -n A-1.0_2 -s "A pkg" --reverts "1.1_1" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -yu >out
	atf_check_equal $? 0
	grep "^remove 1.1_1 pre A 1.1_1 yes no" out
	atf_check_equal $? 0
	grep "^install 1.0_2 pre A 1.0_2 yes no" out
	atf_check_equal $? 0
	grep "^install 1.0_2 post A 1.0_2 yes no" out
	atf_check_equal $? 0

	xbps-remove -C empty.conf -r root -y A >out
	atf_check_equal $? 0

	grep "^remove 1.0_2 pre A 1.0_2 no no" out
	atf_check_equal $? 0
	grep "^remove 1.0_2 post A 1.0_2 no no" out
	atf_check_equal $? 0
	grep "^remove 1.0_2 purge A 1.0_2 no no" out
	atf_check_equal $? 0

	# reinstall run new INSTALL script.
	create_script_stdout "install old 2.0_1" pkg_A/INSTALL
	create_script_stdout "remove old 2.0_1" pkg_A/REMOVE
	cd some_repo
	xbps-create -A noarch -n A-2.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -y A >out
	atf_check_equal $? 0

	create_script_stdout "install new 2.0_1" pkg_A/INSTALL
	create_script_stdout "remove new 2.0_1" pkg_A/REMOVE
	cd some_repo
	xbps-create -A noarch -n A-2.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -fa $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -yf A >out
	atf_check_equal $? 0

	cat out>&2
	atf_check_equal $? 0
	grep "^remove old 2.0_1 pre A 2.0_1 no no" out
	atf_check_equal $? 0
	grep "^remove old 2.0_1 post A 2.0_1 no no" out
	atf_check_equal $? 0
	grep "^remove old 2.0_1 purge A 2.0_1 no no" out
	atf_check_equal $? 0
	grep "^install new 2.0_1 pre A 2.0_1 no no" out
	atf_check_equal $? 0
	grep "^install new 2.0_1 post A 2.0_1 no no" out
	atf_check_equal $? 0
}

atf_init_test_cases() {
	atf_add_test_case script_nargs
	atf_add_test_case script_arch
	atf_add_test_case script_action
}
