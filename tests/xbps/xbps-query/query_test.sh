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

search_head() {
	atf_set "descr" "xbps-query(1) --search"
}

search_body() {
	mkdir -p root some_repo pkg_A pkg_B pkg_C

	cd some_repo
	atf_check -o ignore -- xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check -o ignore -- xbps-create -A noarch -n bar-1.0_1 -s "bar pkg" ../pkg_B
	atf_check -o ignore -- xbps-create -A noarch -n fizz-1.0_1 -s "fizz pkg" ../pkg_C
	atf_check -o ignore -- xbps-rindex -a $PWD/*.xbps
	cd ..

	# regex error
	atf_check -e match:"ERROR: failed to compile regexp: \(:" -s exit:1 -- \
		xbps-query -r root --repository=some_repo --regex -s '('

	# repo mode
	atf_check -o inline:"[-] foo-1.0_1 foo pkg\n" -- \
		xbps-query -r root --repository=some_repo -s foo
	atf_check -o inline:"[-] fizz-1.0_1 fizz pkg\n[-] foo-1.0_1  foo pkg\n" -- \
		xbps-query -r root --repository=some_repo -s f

	atf_check -o ignore -- \
		xbps-install -r root --repository=some_repo -y foo

	# repo mode + installed
	atf_check -o inline:"[*] foo-1.0_1 foo pkg\n" -- \
		xbps-query -r root --repository=some_repo -s foo
	atf_check -o inline:"[-] fizz-1.0_1 fizz pkg\n[*] foo-1.0_1  foo pkg\n" -- \
		xbps-query -r root --repository=some_repo -s f

	# installed mode
	atf_check -o inline:"[*] foo-1.0_1 foo pkg\n" -- \
		xbps-query -r root -s foo
}

atf_init_test_cases() {
	atf_add_test_case cat_file
	atf_add_test_case repo_cat_file
	atf_add_test_case search
}
