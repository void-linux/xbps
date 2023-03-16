#! /usr/bin/env atf-sh
# Test that xbps-query(1) list modes work as expected

atf_test_case list_repos

list_repos_head() {
	atf_set "descr" "xbps-query(1) -L"
}

list_repos_body() {
	mkdir -p some_repo pkg_A/bin
	touch pkg_A/bin/file
	cd some_repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n baz-1.0_1 -s "baz pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	rm -f *.xbps
	cd ..
	output="$(xbps-query -C empty.conf -i --repository=some_repo --repository=vanished_repo -L | tr -d '\n')"
	atf_check_equal "$output" "    2 ${PWD}/some_repo (RSA unsigned)	-1 vanished_repo (RSA maybe-signed)"

	output="$(xbps-query -C empty.conf -i --repository=https://localhost/wtf -L | tr -d '\n')"
	atf_check_equal "$output" "    -1 https://localhost/wtf (RSA maybe-signed)"
}

list_files_head() {
	atf_set "descr" "xbps-query(1) [--long] -f"
}

list_files_body() {
	mkdir -p some_repo pkg/bin
	cd pkg/bin
	touch suidfile
	chmod 4755 suidfile
	touch suidfile_no_x
	chmod 4644 suidfile_no_x
	touch sgidfile
	chmod 2755 sgidfile
	touch sgidfile_no_x
	chmod 2644 sgidfile_no_x
	touch some_exe
	chmod 0750 some_exe
	touch some_file
	chmod 0644 some_file
	touch owned_by_nobody
	touch owned_by_foo
	echo "AAAAAAAAAAAAAAAAAAAAAAAAAAAA" > file_with_size
	ln -s foo symlink
	cd ../../some_repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" \
		--chown "/bin/owned_by_nobody:nobody:root /bin/owned_by_foo:foo:foo" ../pkg
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	output="$(xbps-query -C empty.conf -i --repository=some_repo -f foo | tr -d '\n')"
	atf_check_equal "$output" "/bin/file_with_size/bin/owned_by_foo/bin/owned_by_nobody/bin/sgidfile/bin/sgidfile_no_x/bin/some_exe/bin/some_file/bin/suidfile/bin/suidfile_no_x/bin/symlink -> /bin/foo"
	output="$(xbps-query -C empty.conf -i --repository=some_repo --long -f foo | tr -d '\n')"
	atf_check_equal "$output" "-rw-r--r-- root root 29 B /bin/file_with_size-rw-r--r-- foo foo 0 B /bin/owned_by_foo-rw-r--r-- nobody root 0 B /bin/owned_by_nobody-rwxr-sr-x root root 0 B /bin/sgidfile-rw-r--r-- root root 0 B /bin/sgidfile_no_x-rwxr-x--- root root 0 B /bin/some_exe-rw-r--r-- root root 0 B /bin/some_file-rwsr-xr-x root root 0 B /bin/suidfile-rwSr--r-- root root 0 B /bin/suidfile_no_xl--------- root root 0 B /bin/symlink -> /bin/foo"
}

atf_init_test_cases() {
	atf_add_test_case list_repos
	atf_add_test_case list_files
}
