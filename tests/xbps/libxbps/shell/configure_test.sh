#!/usr/bin/env atf-sh

atf_test_case filemode

filemode_head() {
	atf_set "descr" "Tests for pkg configuration: sane umask for file mode creation"
}

filemode_body() {
	mkdir -p foo
	umask 077
	mkdir -p repo pkg_A
	cat >>pkg_A/INSTALL<<EOF
#!/bin/sh
export PATH="/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin"
case "\$1" in
post)
	touch file
	;;
esac
EOF
	chmod 755 pkg_A/INSTALL
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -C empty.conf -r root --repository=$PWD/repo -yd A
	atf_check_equal $? 0
	perms=$(stat --format=%a root/file)
	atf_check_equal $perms 644
}

atf_init_test_cases() {
	atf_add_test_case filemode
}
