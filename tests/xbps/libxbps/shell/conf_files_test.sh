#! /usr/bin/env atf-sh

# 1st test: modified configuration file on disk, unmodified on upgrade.
#	result: keep file as is on disk.
atf_test_case tc1

tc1_head() {
	atf_set "descr" "Tests for configuration file handling: on-disk modified, upgrade unmodified"
}

tc1_body() {
	mkdir repo
	cd repo
	mkdir pkg_a
	echo "fooblah" > pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.1_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	rm -rf pkg_a
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	xbps-install -C null.conf -r rootdir --repository=$PWD -yvd a
	atf_check_equal $? 0

	sed -e 's,fooblah,blahfoo,' -i rootdir/cf1.conf
	mkdir pkg_a
	echo "fooblah" > pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.2_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	rm -rf pkg_a
	atf_check_equal $? 0
	xbps-install -C null.conf -r rootdir --repository=$PWD -yuvd
	atf_check_equal $? 0
	result="$(cat rootdir/cf1.conf)"
	rval=1
	if [ "${result}" = "blahfoo" ]; then
		rval=0
	fi
	atf_check_equal $rval 0
}

# 2nd test: modified configuration file on disk, modified on upgrade.
#	result: install new file as "<conf_file>.new-<version>".
atf_test_case tc2

tc2_head() {
	atf_set "descr" "Tests for configuration file handling: on-disk modified, upgrade modified"
}

tc2_body() {
	mkdir repo
	cd repo
	mkdir pkg_a
	echo "fooblah" > pkg_a/cf1.conf
	chmod 644 pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.1_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	rm -rf pkg_a
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	xbps-install -C null.conf -r rootdir --repository=$PWD -yvd a
	atf_check_equal $? 0

	sed -e 's,fooblah,blahfoo,' -i rootdir/cf1.conf
	chmod 644 rootdir/cf1.conf
	mkdir pkg_a
	echo "bazbar" > pkg_a/cf1.conf
	chmod 644 pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.2_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	rm -rf pkg_a
	atf_check_equal $? 0
	xbps-install -C null.conf -r rootdir --repository=$PWD -yuvd
	atf_check_equal $? 0
	result="$(cat rootdir/cf1.conf)"
	rval=1
	if [ "${result}" = "blahfoo" ]; then
		rval=0
	fi
	echo "result: ${result}"
	echo "expected: blahfoo"
	atf_check_equal $rval 0
	rval=1
	if [ -s rootdir/cf1.conf.new-0.2_1 ]; then
		rval=0
	fi
	atf_check_equal $rval 0
}

# 3rd test: modified configuration file on disk, unmodified on upgrade.
#	result: keep file on disk as is.
atf_test_case tc3

tc3_head() {
	atf_set "descr" "Tests for configuration file handling: on-disk modified, upgrade unmodified"
}

tc3_body() {
	mkdir repo
	cd repo
	mkdir pkg_a
	echo "fooblah" > pkg_a/cf1.conf
	chmod 644 pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.1_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	rm -rf pkg_a
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	xbps-install -C null.conf -r rootdir --repository=$PWD -yvd a
	atf_check_equal $? 0

	sed -e 's,fooblah,blahfoo,' -i rootdir/cf1.conf
	chmod 644 rootdir/cf1.conf
	mkdir pkg_a
	echo "fooblah" > pkg_a/cf1.conf
	chmod 644 pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.2_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	rm -rf pkg_a
	atf_check_equal $? 0
	xbps-install -C null.conf -r rootdir --repository=$PWD -yuvd
	atf_check_equal $? 0
	result="$(cat rootdir/cf1.conf)"
	rval=1
	if [ "${result}" = "blahfoo" ]; then
		rval=0
	fi
	atf_check_equal $rval 0
}

# 4th test: existent file on disk; new package defines configuration file.
#	result: keep on-disk file as is, install new conf file as file.new-<version>.
atf_test_case tc4

tc4_head() {
	atf_set "descr" "Tests for configuration file handling: existent on-disk, new install"
}

tc4_body() {
	mkdir repo
	cd repo
	mkdir -p pkg_a/etc
	echo "fooblah" > pkg_a/etc/cf1.conf
	chmod 644 pkg_a/etc/cf1.conf
	xbps-create -A noarch -n a-0.1_1 -s "pkg a" --config-files "/etc/cf1.conf" pkg_a
	atf_check_equal $? 0
	rm -rf pkg_a
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0

	mkdir -p rootdir/etc
	echo "blahblah" > rootdir/etc/cf1.conf

	xbps-install -C null.conf -r rootdir --repository=$PWD -yvd a
	atf_check_equal $? 0

	result="$(cat rootdir/etc/cf1.conf)"
	resultpkg="$(cat rootdir/etc/cf1.conf.new-0.1_1)"
	rval=1
	if [ "${result}" = "blahblah" -a "${resultpkg}" = "fooblah" ]; then
		rval=0
	fi
	atf_check_equal $rval 0
}

# 5th test: configuration file replaced with symlink on disk, modified on upgrade.
#	result: install new file as "<conf_file>.new-<version>".
atf_test_case tc5

tc5_head() {
	atf_set "descr" "Tests for configuration file handling: on-disk replaced with symlink, upgrade modified"
}

tc5_body() {
	mkdir repo
	cd repo
	mkdir pkg_a
	echo "fooblah" > pkg_a/cf1.conf
	chmod 644 pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.1_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	rm -rf pkg_a
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	xbps-install -C null.conf -r rootdir --repository=$PWD -yvd a
	atf_check_equal $? 0

	mv rootdir/cf1.conf rootdir/foobar.conf
	ln -sf foobar.conf rootdir/cf1.conf
	sed -e 's,fooblah,blahfoo,' -i rootdir/foobar.conf
	chmod 644 rootdir/foobar.conf

	mkdir pkg_a
	echo "bazbar" > pkg_a/cf1.conf
	chmod 644 pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.2_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0

	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	rm -rf pkg_a

	xbps-install -C null.conf -r rootdir --repository=$PWD -yuvd
	atf_check_equal $? 0

	ls -lsa rootdir
	test -h rootdir/cf1.conf
	atf_check_equal $? 0

	result="$(cat rootdir/cf1.conf)"
	rval=1
	if [ "${result}" = "blahfoo" ]; then
		rval=0
	fi

	echo "result: ${result}"
	echo "expected: blahfoo"
	atf_check_equal $rval 0
	rval=1
	if [ -s rootdir/cf1.conf.new-0.2_1 ]; then
		rval=0
	fi
	atf_check_equal $rval 0
}

# 6th test: unmodified configuration file on disk, keepconf=true, modified on upgrade.
#	result: install new file as "<conf_file>.new-<version>".
atf_test_case tc6

tc6_head() {
	atf_set "descr" "Tests for configuration file handling: on-disk unmodified, keepconf=true, upgrade modified"
}

tc6_body() {
	mkdir repo
	cd repo
	mkdir pkg_a
	echo "fooblah" > pkg_a/cf1.conf
	chmod 644 pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.1_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	rm -rf pkg_a
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	mkdir -p rootdir/xbps.d
	echo "keepconf=true" > rootdir/xbps.d/keepconf.conf

	xbps-install -C xbps.d -r rootdir --repository=$PWD/repo -yvd a
	atf_check_equal $? 0

	cd repo
	mkdir pkg_a
	echo "bazbar" > pkg_a/cf1.conf
	chmod 644 pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.2_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	rm -rf pkg_a
	atf_check_equal $? 0
	cd ..

	xbps-install -C xbps.d -r rootdir --repository=$PWD/repo -yuvd
	atf_check_equal $? 0
	result="$(cat rootdir/cf1.conf)"
	rval=1
	if [ "${result}" = "fooblah" ]; then
		rval=0
	fi
	echo "file: cf1.conf"
	echo "result: ${result}"
	echo "expected: fooblah"
	atf_check_equal $rval 0

	result="$(cat rootdir/cf1.conf.new-0.2_1)"
	rval=1
	if [ "${result}" = "bazbar" ]; then
		rval=0
	fi
	echo "file: cf1.conf.new-0.2_1"
	echo "result: ${result}"
	echo "expected: bazbar"
	atf_check_equal $rval 0
}

# 7th test: unmodified configuration file on disk, modified on upgrade.
#	result: update file
atf_test_case tc7

tc7_head() {
	atf_set "descr" "Tests for configuration file handling: on-disk unmodified, upgrade modified"
}

tc7_body() {
	mkdir repo
	cd repo
	mkdir pkg_a
	echo "original" > pkg_a/cf1.conf
	chmod 644 pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.1_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	rm -rf pkg_a
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C xbps.d -r rootdir --repository=$PWD/repo -yvd a
	atf_check_equal $? 0

	cd repo
	mkdir pkg_a
	echo "updated" > pkg_a/cf1.conf
	chmod 644 pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.2_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	rm -rf pkg_a
	atf_check_equal $? 0
	cd ..

	xbps-install -C xbps.d -r rootdir --repository=$PWD/repo -yuvd
	atf_check_equal $? 0
	atf_check_equal "$(cat rootdir/cf1.conf)" "updated"
}

# 8th test: modified configuration file on disk to same as updated, modified on upgrade.
#	result: keep on-disk file as is, don't install new conf file as file.new-<version>.
atf_test_case tc8

tc8_head() {
	atf_set "descr" "Tests for configuration file handling: on-disk modified, matches upgrade"
}

tc8_body() {
	mkdir repo
	cd repo
	mkdir pkg_a
	echo "original" > pkg_a/cf1.conf
	chmod 644 pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.1_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	rm -rf pkg_a
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C xbps.d -r rootdir --repository=$PWD/repo -yvd a
	atf_check_equal $? 0
	sed -e 's,original,improved,' -i rootdir/cf1.conf

	cd repo
	mkdir pkg_a
	echo "improved" > pkg_a/cf1.conf
	chmod 644 pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.2_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	rm -rf pkg_a
	atf_check_equal $? 0
	cd ..

	xbps-install -C xbps.d -r rootdir --repository=$PWD/repo -yuvd
	atf_check_equal $? 0
	atf_check_equal "$(cat rootdir/cf1.conf)" "improved"
	test '!' -e rootdir/cf1.conf.new-0.2_1
	atf_check_equal $? 0
}

# 9th test: removed configuration file on disk, unmodified on upgrade.
#	result: install file
atf_test_case tc9

tc9_head() {
	atf_set "descr" "Tests for configuration file handling: on-disk removed, upgrade unmodified"
}

tc9_body() {
	mkdir repo
	cd repo
	mkdir pkg_a
	echo "original" > pkg_a/cf1.conf
	chmod 644 pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.1_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	rm -rf pkg_a
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C xbps.d -r rootdir --repository=$PWD/repo -yvd a
	atf_check_equal $? 0
	rm rootdir/cf1.conf

	cd repo
	mkdir pkg_a
	echo "original" > pkg_a/cf1.conf
	chmod 644 pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.2_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	rm -rf pkg_a
	atf_check_equal $? 0
	cd ..

	xbps-install -C xbps.d -r rootdir --repository=$PWD/repo -yuvd
	atf_check_equal $? 0
	atf_check_equal "$(cat rootdir/cf1.conf)" "original"
	test '!' -e rootdir/cf1.conf.new-0.2_1
	atf_check_equal $? 0
}


# 10th test: removed configuration file on disk, modified on upgrade.
#	result: install file
atf_test_case tc10

tc10_head() {
	atf_set "descr" "Tests for configuration file handling: on-disk removed, upgrade modified"
}

tc10_body() {
	mkdir repo
	cd repo
	mkdir pkg_a
	echo "original" > pkg_a/cf1.conf
	chmod 644 pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.1_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	rm -rf pkg_a
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C xbps.d -r rootdir --repository=$PWD/repo -yvd a
	atf_check_equal $? 0
	rm rootdir/cf1.conf

	cd repo
	mkdir pkg_a
	echo "updated" > pkg_a/cf1.conf
	chmod 644 pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.2_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	rm -rf pkg_a
	atf_check_equal $? 0
	cd ..

	xbps-install -C xbps.d -r rootdir --repository=$PWD/repo -yuvd
	atf_check_equal $? 0
	atf_check_equal "$(cat rootdir/cf1.conf)" "updated"
	test '!' -e rootdir/cf1.conf.new-0.2_1
	atf_check_equal $? 0
}

# 1st link test: modified configuration link on disk, unmodified on upgrade.
#	result: keep link as is on disk.
atf_test_case tcl1

tcl1_head() {
	atf_set "descr" "Tests for configuration link handling: on-disk modified, upgrade unmodified"
}

tcl1_body() {
	atf_expect_fail "not implemented"
	mkdir repo
	cd repo
	mkdir pkg_a
	ln -s original pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.1_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	rm -rf pkg_a
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	xbps-install -C null.conf -r rootdir --repository=$PWD -yvd a
	atf_check_equal $? 0

	ln -sf custom rootdir/cf1.conf
	mkdir pkg_a
	ln -s original pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.2_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	rm -rf pkg_a
	atf_check_equal $? 0
	xbps-install -C null.conf -r rootdir --repository=$PWD -yuvd
	atf_check_equal $? 0
	atf_check_equal "$(readlink rootdir/cf1.conf)" custom
}

# 2nd test: modified configuration link on disk, modified on upgrade.
#	result: install new link as "<conf_file>.new-<version>".
atf_test_case tcl2

tcl2_head() {
	atf_set "descr" "Tests for configuration link handling: on-disk modified, upgrade modified"
}

tcl2_body() {
	atf_expect_fail "not implemented"
	mkdir repo
	cd repo
	mkdir pkg_a
	ln -s original pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.1_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	rm -rf pkg_a
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	xbps-install -C null.conf -r rootdir --repository=$PWD -yvd a
	atf_check_equal $? 0

	ln -sf custom rootdir/cf1.conf
	mkdir pkg_a
	ln -s updated pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.2_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	rm -rf pkg_a
	atf_check_equal $? 0
	xbps-install -C null.conf -r rootdir --repository=$PWD -yuvd
	atf_check_equal $? 0
	atf_check_equal "$(readlink rootdir/cf1.conf)" custom
	test -h rootdir/cf1.conf.new-0.2_1
	atf_check_equal $? 0
}

# 3rd test: modified configuration link on disk, unmodified on upgrade.
#	result: keep link on disk as is.
atf_test_case tcl3

tcl3_head() {
	atf_set "descr" "Tests for configuration link handling: on-disk modified, upgrade unmodified"
}

tcl3_body() {
	atf_expect_fail "not implemented"
	mkdir repo
	cd repo
	mkdir pkg_a
	ln -s original pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.1_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	rm -rf pkg_a
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	xbps-install -C null.conf -r rootdir --repository=$PWD -yvd a
	atf_check_equal $? 0

	ln -sf custom rootdir/cf1.conf
	mkdir pkg_a
	ln -s original pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.2_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	rm -rf pkg_a
	atf_check_equal $? 0
	xbps-install -C null.conf -r rootdir --repository=$PWD -yuvd
	atf_check_equal $? 0
	atf_check_equal "$(readlink rootdir/cf1.conf)" custom
}

# 4th test: existent link on disk; new package defines configuration link.
#	result: keep on-disk link as is, install new conf link as file.new-<version>.
atf_test_case tcl4

tcl4_head() {
	atf_set "descr" "Tests for configuration link handling: existent on-disk, new install"
}

tcl4_body() {
	atf_expect_fail "not implemented"
	mkdir repo
	cd repo
	mkdir -p pkg_a/etc
	ln -s original pkg_a/etc/cf1.conf
	xbps-create -A noarch -n a-0.1_1 -s "pkg a" --config-files "/etc/cf1.conf" pkg_a
	atf_check_equal $? 0
	rm -rf pkg_a
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	mkdir -p rootdir/etc
	ln -s custom rootdir/etc/cf1.conf
	xbps-install -C null.conf -r rootdir --repository=$PWD -yvd a
	atf_check_equal $? 0
	atf_check_equal "$(readlink rootdir/etc/cf1.conf)" custom
	atf_check_equal "$(readlink rootdir/etc/cf1.conf.new-0.1_1)" original
}

# 5th test: configuration link replaced with regular file on disk, modified on upgrade.
#	result: install new link as "<conf_file>.new-<version>".
atf_test_case tcl5

tcl5_head() {
	atf_set "descr" "Tests for configuration link handling: on-disk replaced with symlink, upgrade modified"
}

tcl5_body() {
	atf_expect_fail "not implemented"
	mkdir repo
	cd repo
	mkdir pkg_a
	ln -s original pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.1_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $?a 0a
	rm -rf pkg_a
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $?b 0b
	xbps-install -C null.conf -r rootdir --repository=$PWD -yvd a
	atf_check_equal $?c 0c

	rm rootdir/cf1.conf
	echo 'key=value' > rootdir/cf1.conf

	mkdir pkg_a
	ln -s updated pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.2_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $?d 0d
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $?e 0e
	rm -rf pkg_a

	xbps-install -C null.conf -r rootdir --repository=$PWD -yuvd
	atf_check_equal $?f 0f

	atf_check_equal "$(cat rootdir/cf1.conf)" "key=value"
	test -f rootdir/cf1.conf
	atf_check_equal $?g 0g
	test -h rootdir/cf1.conf.new-0.2_1
	atf_check_equal $?h 0h
}

# 6th test: unmodified configuration link on disk, keepconf=true, modified on upgrade.
#	result: install new link as "<conf_file>.new-<version>".
atf_test_case tcl6

tcl6_head() {
	atf_set "descr" "Tests for configuration link handling: on-disk unmodified, keepconf=true, upgrade modified"
}

tcl6_body() {
	atf_expect_fail "not implemented"
	mkdir repo
	cd repo
	mkdir pkg_a
	ln -s original pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.1_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	rm -rf pkg_a
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	mkdir -p rootdir/xbps.d
	echo "keepconf=true" > rootdir/xbps.d/keepconf.conf

	xbps-install -C xbps.d -r rootdir --repository=$PWD/repo -yvd a
	atf_check_equal $? 0

	cd repo
	mkdir pkg_a
	ln -s updated pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.2_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	rm -rf pkg_a
	atf_check_equal $? 0
	cd ..

	xbps-install -C xbps.d -r rootdir --repository=$PWD/repo -yuvd
	atf_check_equal $? 0
	atf_check_equal "$(readlink rootdir/cf1.conf)" original
	atf_check_equal "$(readlink rootdir/cf1.conf.new-0.2_1)" updated
}

# 7th test: unmodified configuration link on disk, modified on upgrade.
#	result: update link
atf_test_case tcl7

tcl7_head() {
	atf_set "descr" "Tests for configuration link handling: on-disk unmodified, upgrade modified"
}

tcl7_body() {
	mkdir repo
	cd repo
	mkdir pkg_a
	ln -s original pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.1_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	rm -rf pkg_a
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C xbps.d -r rootdir --repository=$PWD/repo -yvd a
	atf_check_equal $? 0

	cd repo
	mkdir pkg_a
	ln -s updated pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.2_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	rm -rf pkg_a
	atf_check_equal $? 0
	cd ..

	xbps-install -C xbps.d -r rootdir --repository=$PWD/repo -yuvd
	atf_check_equal $? 0
	atf_check_equal "$(readlink rootdir/cf1.conf)" updated
}

# 8th test: modified configuration link on disk to same as updated, modified on upgrade.
#	result: keep on-disk link as is, don't install new conf link as file.new-<version>.
atf_test_case tcl8

tcl8_head() {
	atf_set "descr" "Tests for configuration link handling: on-disk modified, matches upgrade"
}

tcl8_body() {
	mkdir repo
	cd repo
	mkdir pkg_a
	ln -s original pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.1_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	rm -rf pkg_a
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C xbps.d -r rootdir --repository=$PWD/repo -yvd a
	atf_check_equal $? 0
	ln -sf improved rootdir/cf1.conf

	cd repo
	mkdir pkg_a
	ln -s improved pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.2_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	rm -rf pkg_a
	atf_check_equal $? 0
	cd ..

	xbps-install -C xbps.d -r rootdir --repository=$PWD/repo -yuvd
	atf_check_equal $? 0
	atf_check_equal "$(readlink rootdir/cf1.conf)" improved
	test '!' -e rootdir/cf1.conf.new-0.2_1
	atf_check_equal $? 0
}

# 9th test: removed configuration link on disk, unmodified on upgrade.
#	result: install link, don't install new conf link as file.new-<version>.
atf_test_case tcl9

tcl9_head() {
	atf_set "descr" "Tests for configuration link handling: on-disk removed, upgrade unmodified"
}

tcl9_body() {
	mkdir repo
	cd repo
	mkdir pkg_a
	ln -s original pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.1_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	rm -rf pkg_a
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C xbps.d -r rootdir --repository=$PWD/repo -yvd a
	atf_check_equal $? 0
	rm rootdir/cf1.conf

	cd repo
	mkdir pkg_a
	ln -s original pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.2_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	rm -rf pkg_a
	atf_check_equal $? 0
	cd ..

	xbps-install -C xbps.d -r rootdir --repository=$PWD/repo -yuvd
	atf_check_equal $? 0
	atf_check_equal "$(readlink rootdir/cf1.conf)" original
	test '!' -e rootdir/cf1.conf.new-0.2_1
	atf_check_equal $? 0
}


# 10th test: removed configuration link on disk, modified on upgrade.
#	result: install link, don't install new conf link as file.new-<version>.
atf_test_case tcl10

tcl10_head() {
	atf_set "descr" "Tests for configuration link handling: on-disk removed, upgrade modified"
}

tcl10_body() {
	mkdir repo
	cd repo
	mkdir pkg_a
	ln -s original pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.1_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	rm -rf pkg_a
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C xbps.d -r rootdir --repository=$PWD/repo -yvd a
	atf_check_equal $? 0
	rm rootdir/cf1.conf

	cd repo
	mkdir pkg_a
	ln -s updated pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.2_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	rm -rf pkg_a
	atf_check_equal $? 0
	cd ..

	xbps-install -C xbps.d -r rootdir --repository=$PWD/repo -yuvd
	atf_check_equal $? 0
	atf_check_equal "$(readlink rootdir/cf1.conf)" updated
	test '!' -e rootdir/cf1.conf.new-0.2_1
	atf_check_equal $? 0
}

atf_init_test_cases() {
	atf_add_test_case tc1
	atf_add_test_case tc2
	atf_add_test_case tc3
	atf_add_test_case tc4
	atf_add_test_case tc5
	atf_add_test_case tc6
	atf_add_test_case tc7
	atf_add_test_case tc8
	atf_add_test_case tc9
	atf_add_test_case tc10

	atf_add_test_case tcl1
	atf_add_test_case tcl2
	atf_add_test_case tcl3
	atf_add_test_case tcl4
	atf_add_test_case tcl5
	atf_add_test_case tcl6
	atf_add_test_case tcl7
	atf_add_test_case tcl8
	atf_add_test_case tcl9
	atf_add_test_case tcl10
}
