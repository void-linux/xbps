#! /usr/bin/env atf-sh
# Test that xbps-query(8) -i works as expected

atf_test_case ignore_system

ignore_system_head() {
	atf_set "descr" "xbps-query(8) -i: ignore repos defined in the system directory (sharedir/xbps.d)"
}

ignore_system_body() {
	mkdir -p repo pkg_A/bin
	touch pkg_A/bin/file
	ln -s repo repo1
	cd repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	rm -f *.xbps
	cd ..
	systemdir=$(xbps-uhelper getsystemdir)
	mkdir -p root/${systemdir}
	echo "repository=$PWD/repo1" > root/${systemdir}/myrepo.conf
	out="$(xbps-query -C empty.conf --repository=$PWD/repo -i -L|wc -l)"
	atf_check_equal "$out" 1
}

atf_test_case ignore_conf

ignore_conf_head() {
	atf_set "descr" "xbps-query(8) -i: ignore repos defined in the configuration directory (xbps.d)"
}

ignore_conf_body() {
	mkdir -p repo pkg_A/bin
	touch pkg_A/bin/file
	ln -s repo repo1
	cd repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	rm -f *.xbps
	cd ..
	mkdir -p root/xbps.d
	echo "repository=$PWD/repo1" > root/xbps.d/myrepo.conf
	out="$(xbps-query -r root -C xbps.d --repository=$PWD/repo -i -L|wc -l)"
	atf_check_equal "$out" 1
}

atf_init_test_cases() {
	atf_add_test_case ignore_conf
	atf_add_test_case ignore_system
}
