#! /usr/bin/env atf-sh
# Test that xbps-repodb(1) --index mode works as expected.

atf_test_case update

update_head() {
	atf_set "descr" "xbps-repodb(1) --index: update test"
}

update_body() {
	stagedata=$(xbps-uhelper arch)-stagedata
	mkdir -p some_repo pkg root
	touch pkg/file00
	cd some_repo
	xbps-create -A noarch -n lib-1.0_1 -s "lib pkg" --shlib-provides "libfoo.so.1" ../pkg
	atf_check_equal a$? a0
	xbps-create -A noarch -n commA-1.0_1 -s "pkg A" --shlib-requires "libfoo.so.1" ../pkg
	atf_check_equal s$? s0
	xbps-create -A noarch -n commB-1.0_1 -s "pkg B" --shlib-requires "libfoo.so.1" ../pkg
	atf_check_equal d$? d0
	xbps-rindex --stage -a $PWD/*.xbps
	atf_check_equal f$? f0
	xbps-repodb --index $PWD
	atf_check_equal $? 0
	mv $stagedata stagedata.moved # xbps-query reads stagedata
	atf_check_equal "$(xbps-query --repository=. -p pkgver lib)" lib-1.0_1
	atf_check_equal "$(xbps-query --repository=. -p pkgver commA)" commA-1.0_1
	atf_check_equal "$(xbps-query --repository=. -p pkgver commB)" commB-1.0_1
	mv stagedata.moved $stagedata

	xbps-create -A noarch -n lib-1.10_1 -s "lib pkg" --shlib-provides "libfoo.so.2" ../pkg
	atf_check_equal z$? z0
	xbps-rindex --stage -a $PWD/*.xbps
	atf_check_equal x$? x0
	xbps-repodb --index $PWD
	atf_check_equal $? 0
	mv $stagedata stagedata.moved # xbps-query reads stagedata
	atf_check_equal "$(xbps-query --repository=. -p pkgver lib)" lib-1.0_1
	mv stagedata.moved $stagedata

	xbps-create -A noarch -n commB-2.0_1 -s "pkg B" --shlib-requires "libfoo.so.2" ../pkg
	atf_check_equal c$? c0
	xbps-rindex --stage -a $PWD/*.xbps
	atf_check_equal v$? v0
	xbps-repodb --index $PWD
	atf_check_equal $? 0
	mv $stagedata stagedata.moved # xbps-query reads stagedata
	atf_check_equal "$(xbps-query --repository=. -p pkgver lib)" lib-1.0_1
	atf_check_equal "$(xbps-query --repository=. -p pkgver commB)" commB-1.0_1
	mv stagedata.moved $stagedata

	xbps-create -A noarch -n commA-1.1_1 -s "pkg A" --shlib-requires "libfoo.so.2" ../pkg
	atf_check_equal b$? b0
	xbps-rindex --stage -a $PWD/*.xbps
	atf_check_equal n$? n0
	xbps-repodb --index $PWD
	atf_check_equal $? 0
	rm $stagedata # xbps-query reads stagedata
	atf_check_equal "$(xbps-query --repository=. -p pkgver lib)" lib-1.10_1
	atf_check_equal "$(xbps-query --repository=. -p pkgver commA)" commA-1.1_1
	atf_check_equal "$(xbps-query --repository=. -p pkgver commB)" commB-2.0_1
}

nonexistent_requires_head() {
	atf_set "descr" "xbps-repodb(1) --index: [unrealistic] try to index package requiring nonexistent shlib"
}

nonexistent_requires_body() {
	stagedata=$(xbps-uhelper arch)-stagedata
	mkdir -p some_repo pkg root
	touch pkg/file00
	cd some_repo
	xbps-create -A noarch -n lib-1.0_1 -s "lib pkg" --shlib-provides "libfoo.so.1" ../pkg
	atf_check_equal a$? a0
	xbps-rindex --stage -a $PWD/*.xbps
	atf_check_equal s$? s0
	xbps-repodb --index $PWD
	atf_check_equal $? 0
	mv $stagedata stagedata.moved # xbps-query reads stagedata
	atf_check_equal "$(xbps-query --repository=. -p pkgver lib)" lib-1.0_1
	atf_check_equal "$(xbps-query --repository=. -p pkgver commA)" ""
	mv stagedata.moved $stagedata

	xbps-create -A noarch -n commA-1.0_1 -s "pkg A" --shlib-requires "libbaz.so.1" ../pkg
	atf_check_equal d$? d0
	xbps-rindex --stage -a $PWD/*.xbps
	atf_check_equal f$? f0
	xbps-repodb --index $PWD
	atf_check_equal $? 0
	mv $stagedata stagedata.moved # xbps-query reads stagedata
	atf_check_equal "$(xbps-query --repository=. -p pkgver lib)" lib-1.0_1
	atf_check_equal "$(xbps-query --repository=. -p pkgver commA)" ""
	mv stagedata.moved $stagedata
}

library_advanced_head() {
	atf_set "descr" "xbps-repodb(1) --index: [unlikely] update library again before revdeps depending middle version are indexed"
}

library_advanced_body() {
	stagedata=$(xbps-uhelper arch)-stagedata
	mkdir -p some_repo pkg root
	touch pkg/file00
	cd some_repo
	xbps-create -A noarch -n lib-1.0_1 -s "lib pkg" --shlib-provides "libfoo.so.1" ../pkg
	atf_check_equal a$? a0
	xbps-create -A noarch -n commA-1.0_1 -s "pkg A" --shlib-requires "libfoo.so.1" ../pkg
	atf_check_equal s$? s0
	xbps-create -A noarch -n commB-1.0_1 -s "pkg B" --shlib-requires "libfoo.so.1" ../pkg
	atf_check_equal d$? d0
	xbps-rindex --stage -a $PWD/*.xbps
	atf_check_equal f$? f0
	xbps-repodb --index $PWD
	atf_check_equal $? 0
	mv $stagedata stagedata.moved # xbps-query reads stagedata
	atf_check_equal "$(xbps-query --repository=. -p pkgver lib)" lib-1.0_1
	atf_check_equal "$(xbps-query --repository=. -p pkgver commA)" "commA-1.0_1"
	atf_check_equal "$(xbps-query --repository=. -p pkgver commB)" "commB-1.0_1"
	mv stagedata.moved $stagedata

	xbps-create -A noarch -n lib-2.0_1 -s "lib pkg" --shlib-provides "libfoo.so.2" ../pkg
	atf_check_equal q$? q0
	xbps-create -A noarch -n commA-1.0_2 -s "pkg A" --shlib-requires "libfoo.so.2" ../pkg
	atf_check_equal w$? w0
	xbps-rindex --stage -a $PWD/*.xbps
	atf_check_equal e$? e0
	xbps-repodb --index $PWD
	atf_check_equal $? 0
	mv $stagedata stagedata.moved # xbps-query reads stagedata
	atf_check_equal "$(xbps-query --repository=. -p pkgver lib)" lib-1.0_1
	atf_check_equal "$(xbps-query --repository=. -p pkgver commA)" "commA-1.0_1"
	atf_check_equal "$(xbps-query --repository=. -p pkgver commB)" "commB-1.0_1"
	mv stagedata.moved $stagedata

	xbps-create -A noarch -n lib-2.0_1 -s "lib pkg" --shlib-provides "libfoo.so.2" ../pkg
	atf_check_equal y$? y0
	xbps-create -A noarch -n commA-1.0_2 -s "pkg A" --shlib-requires "libfoo.so.2" ../pkg
	atf_check_equal u$? u0
	xbps-rindex --stage -a $PWD/*.xbps
	atf_check_equal i$? i0
	xbps-repodb --index $PWD
	atf_check_equal $? 0
	mv $stagedata stagedata.moved # xbps-query reads stagedata
	atf_check_equal "$(xbps-query --repository=. -p pkgver lib)" lib-1.0_1
	atf_check_equal "$(xbps-query --repository=. -p pkgver commA)" "commA-1.0_1"
	atf_check_equal "$(xbps-query --repository=. -p pkgver commB)" "commB-1.0_1"
	mv stagedata.moved $stagedata

	xbps-create -A noarch -n lib-3.0_1 -s "lib pkg" --shlib-provides "libfoo.so.3" ../pkg
	atf_check_equal z$? z0
	xbps-create -A noarch -n commB-1.0_2 -s "pkg B" --shlib-requires "libfoo.so.3" ../pkg
	atf_check_equal x$? x0
	xbps-rindex --stage -a $PWD/*.xbps
	atf_check_equal c$? c0
	xbps-repodb --index $PWD
	atf_check_equal $? 0
	mv $stagedata stagedata.moved # xbps-query reads stagedata
	#fails here
	atf_check_equal "$(xbps-query --repository=. -p pkgver lib)" lib-1.0_1
	atf_check_equal "$(xbps-query --repository=. -p pkgver commA)" "commA-1.0_1"
	atf_check_equal "$(xbps-query --repository=. -p pkgver commB)" "commB-1.0_1"
}

library_split_head() {
	atf_set "descr" "xbps-repodb(1) --index: split library into parts without rebuild of revdeps"
}

library_split_body() {
	atf_expect_fail "Not implemented"

	stagedata=$(xbps-uhelper arch)-stagedata
	mkdir -p some_repo pkg root
	touch pkg/file00
	cd some_repo
	xbps-create -A noarch -n lib-1.0_1 -s "lib pkg" --shlib-provides "libfoonet.so.4 libfoonum.so.1" ../pkg
	atf_check_equal a$? a0
	xbps-create -A noarch -n commA-1.0_1 -s "pkg A" --shlib-requires "libfoonum.so.1" -D 'lib>=1.0_1' ../pkg
	atf_check_equal s$? s0
	xbps-rindex --stage -a $PWD/*.xbps
	atf_check_equal d$? d0
	xbps-repodb --index $PWD
	atf_check_equal $? 0
	xbps-install -n commA --repository .
	atf_check_equal f$? f0

	xbps-create -A noarch -n lib-1.0_2 -s "lib pkg" ../pkg
	atf_check_equal q$? q0
	xbps-create -A noarch -n libnet-1.0_2 -s "lib pkg, net part" --shlib-provides "libfoonet.so.4" ../pkg
	atf_check_equal w$? w0
	xbps-create -A noarch -n libnum-1.0_2 -s "lib pkg, num part" --shlib-provides "libfoonum.so.1" ../pkg
	atf_check_equal e$? e0
	xbps-rindex --stage -a $PWD/*.xbps
	atf_check_equal r$? r0
	xbps-repodb --index $PWD
	atf_check_equal $? 0
	xbps-install -n commA --repository .
	#fails here
	atf_check_equal t$? t0
}

move_between_repos_head() {
	atf_set "descr" "xbps-repodb(1) --index: move package between repos on update"
}

move_between_repos_body() {
	arch=$(xbps-uhelper arch)
	repodata=${arch}-repodata
	stagedata=${arch}-stagedata
	mkdir -p some_repo other_repo pkg root
	touch pkg/file00
	cd some_repo
	xbps-create -A ${arch} -n lib-1.0_1 -s "lib pkg" --shlib-provides "libfoonet.so.4 libfoonum.so.1" ../pkg
	atf_check_equal a$? a0
	xbps-create -A ${arch} -n commA-1.0_1 -s "pkg A" --shlib-requires "libfoonum.so.1" -D 'lib>=1.0_1' ../pkg
	atf_check_equal s$? s0
	xbps-rindex --stage -a $PWD/*.xbps
	atf_check_equal d$? d0
	atf_check_equal $? 0
	cd ..
	cd other_repo
	xbps-create -A ${arch} -n pkgB-0.1_1 -s "pkgB" ../pkg
	atf_check_equal $? 0
	xbps-create -A ${arch} -n lib-1.1_1 -s "lib pkg" --shlib-provides "libfoonet.so.4 libfoonum.so.1" ../pkg
	atf_check_equal $? 0
	xbps-rindex --stage -a $PWD/pkgB-0.1_1.${arch}.xbps
	atf_check_equal $? 0
	cd ..
	xbps-repodb --index some_repo other_repo
	atf_check_equal $? 0
	atf_check_equal "$(xbps-query --repository=some_repo -p pkgver commA)b" commA-1.0_1b
	atf_check_equal "$(xbps-query --repository=some_repo -p pkgver lib)b" lib-1.0_1b
	atf_check_equal "$(xbps-query --repository=other_repo -p pkgver lib)b" b
	cd other_repo
	xbps-rindex --stage -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-repodb --index some_repo other_repo
	atf_check_equal $? 0
	rm some_repo/${stagedata} other_repo/${stagedata}
	atf_check_equal "$(xbps-query --repository=some_repo -p pkgver commA)" commA-1.0_1
	atf_check_equal "$(xbps-query --repository=other_repo -p pkgver lib)" lib-1.1_1
	atf_check_equal "$(xbps-query --repository=some_repo -p pkgver lib)" ""
}

atf_init_test_cases() {
	atf_add_test_case update
	atf_add_test_case nonexistent_requires
	atf_add_test_case library_advanced
	atf_add_test_case library_split
	atf_add_test_case move_between_repos
}
