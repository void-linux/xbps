#! /usr/bin/env atf-sh

# xbps issue #6.
# How to reproduce it:
#	- Create pkg a-0.1_1 (empty).
#	- Create two empty dirs on target rootdir.
#		/usr/lib/firefox/dictionaries
#		/usr/lib/firefox/hyphenation
#	- Create pkg a-0.2_1 containing 2 symlinks overwriting existing dirs.
#		/usr/lib/firefox/dictionaries -> /usr/share/hunspell
#		/usr/lib/firefox/hyphenation -> /usr/share/hyphen
#	- Upgrade pkg a to 0.2.

atf_test_case issue6

issue6_head() {
	atf_set "descr" "xbps issue #6 (https://github.com/xtraeme/xbps/issues/6)"
}

issue6_body() {
	mkdir repo
	cd repo
	xbps-create -A noarch -n a-0.1_1 -s "pkg a" .
	atf_check_equal $? 0
	xbps-rindex -a *.xbps
	atf_check_equal $? 0
	xbps-install -r rootdir --repository=$PWD -y a
	atf_check_equal $? 0
	mkdir -p rootdir/usr/lib/firefox/dictionaries
	mkdir -p rootdir/usr/lib/firefox/hyphenation
	mkdir -p pkg_a/usr/lib/firefox
	ln -s /usr/share/hunspell pkg_a/usr/lib/firefox/dictionaries
	ln -s /usr/share/hyphen pkg_a/usr/lib/firefox/hyphenation
	xbps-create -A noarch -n a-0.2_1 -s "pkg a" pkg_a
	atf_check_equal $? 0
	rm -rf pkg_a
	xbps-rindex -a *.xbps
	atf_check_equal $? 0
	xbps-install -r rootdir --repository=$PWD -yu
	atf_check_equal $? 0
	tgt1=$(readlink rootdir/usr/lib/firefox/dictionaries)
	tgt2=$(readlink rootdir/usr/lib/firefox/hyphenation)
	rval=1
	if [ "$tgt1" = "/usr/share/hunspell" -a "$tgt2" = "/usr/share/hyphen" ]; then
		rval=0
	fi
	atf_check_equal $rval 0
}

atf_init_test_cases() {
	atf_add_test_case issue6
}
