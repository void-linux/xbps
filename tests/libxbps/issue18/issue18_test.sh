#! /usr/bin/env atf-sh

# xbps issue #18.
# How to reproduce it:
# 	Generate pkg A-0.1_1 and B-0.1_1.
#	Install both pkgs.
#	Generate pkg A-0.2_1: conflicts with B<0.1_2.
#	Generate pkg B-0.1_2.
#	Update all packages.

atf_test_case issue18

issue18_head() {
	atf_set "descr" "xbps issue #18 (https://github.com/xtraeme/xbps/issues/18)"
}

issue18_body() {
	mkdir pkg_A pkg_B
	atf_check 'xbps-create -A noarch -n A-0.1_1 -s "pkg A" pkg_A' 0
	atf_check 'xbps-create -A noarch -n B-0.1_1 -s "pkg B" pkg_B' 0
	atf_check 'xbps-rindex -a *.xbps' 0
	atf_check 'xbps-install -r rootdir --repository=$PWD -y A B' 0

	atf_check 'xbps-create -A noarch -n A-0.2_1 -s "pkg A" --conflicts "B<0.1_2" pkg_A' 0
	atf_check 'xbps-create -A noarch -n B-0.1_2 -s "pkg B" pkg_B' 0
	atf_check 'xbps-rindex -a *.xbps' 0
	atf_check 'xbps-install -r rootdir --repository=$PWD -yu' 0
}

atf_init_test_cases() {
	atf_add_test_case issue18
}
