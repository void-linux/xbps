#! /usr/bin/env atf-sh

# xbps issue #31.
# How to reproduce it:
# 	Generate pkg A-0.1_1:
#		/dir/dir/foo
#	Install pkg A.
#	Generate pkg A-0.2_1:
#		/dir/foo
#	Update pkg A to 0.2_1

atf_test_case issue31

issue31_head() {
	atf_set "descr" "xbps issue #31 (https://github.com/xtraeme/xbps/issues/31)"
}

issue31_body() {
	mkdir -p pkg_A/usr/share/licenses/chromium/license.html
	echo random > pkg_A/usr/share/licenses/chromium/license.html/eula.html
	xbps-create -A noarch -n A-0.1_1 -s "pkg A" pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0

	xbps-install -r root -C null.conf --repository=$PWD -y A
	atf_check_equal $? 0

	touch root/usr/share/licenses/chromium/license.html/fail

	mkdir -p pkg_B/usr/share/licenses/chromium
	echo "morerandom" > pkg_B/usr/share/licenses/chromium/license.html
	xbps-create -A noarch -n A-0.2_1 -s "pkg A" pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a A-0.2_1.noarch.xbps
	atf_check_equal $? 0

	xbps-install -r root -C null.conf --repository=$PWD -yuvd A
	# ENOTEMPTY
	atf_check_equal $? 39

	rm root/usr/share/licenses/chromium/license.html/fail
	xbps-install -r root -C null.conf --repository=$PWD -yuvd A
	atf_check_equal $? 0
}

atf_init_test_cases() {
	atf_add_test_case issue31
}
