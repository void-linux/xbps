#! /usr/bin/env atf-sh

cat_file_head() {
	atf_set "descr" "xbps-query(1) --cat: cat pkgdb file"
}

cat_file_body() {
	mkdir -p repo pkg_A/bin
	echo "hello world!" > pkg_A/bin/file
	cd repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a *.xbps
	atf_check_equal $? 0
	cd ..
	mkdir root
	xbps-install -r root --repository=repo -dvy foo
	atf_check_equal $? 0
	res=$(xbps-query -r root -dv -C empty.conf --cat /bin/file foo)
	atf_check_equal $? 0
	atf_check_equal "$res" "hello world!"
}

repo_cat_file_head() {
	atf_set "descr" "xbps-query(1) -R --cat: cat repo file"
}

repo_cat_file_body() {
	mkdir -p repo pkg_A/bin
	echo "hello world!" > pkg_A/bin/file
	cd repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a *.xbps
	atf_check_equal $? 0
	cd ..
	res=$(xbps-query -r root -dv --repository=repo -C empty.conf --cat /bin/file foo)
	atf_check_equal $? 0
	atf_check_equal "$res" "hello world!"
}

atf_init_test_cases() {
	atf_add_test_case cat_file
	atf_add_test_case repo_cat_file
}
