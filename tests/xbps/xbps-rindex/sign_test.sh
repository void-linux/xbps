#! /usr/bin/env atf-sh
# Test that xbps-rindex(1) signing repo metadata works as expected.

get_resources() {
	mkdir -p root/var/db/xbps/keys
	mkdir -p /var/db/xbps/keys
	cp $(atf_get_srcdir)/data/id_xbps .
	cp $(atf_get_srcdir)/data/bd:75:21:4e:40:06:97:5e:72:31:40:6e:9e:08:a8:ae.plist root/var/db/xbps/keys
	cp $(atf_get_srcdir)/data/bd:75:21:4e:40:06:97:5e:72:31:40:6e:9e:08:a8:ae.plist /var/db/xbps/keys
}

atf_test_case sign

sign_head() {
	atf_set "descr" "xbps-rindex(1) signing test"
}

sign_body() {
	get_resources
	# make pkg
	mkdir -p some_repo pkg_A
	touch pkg_A/file00
	cd some_repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	# make repodata
	xbps-rindex -a $PWD/*.xbps
	atf_check_equal $? 0
	repodata=$(ls *-repodata)
	atf_check_equal $(tar tf $repodata | wc -l) 2
	# sign repodata
	xbps-rindex -s $PWD --signedby test --privkey ../id_xbps
	atf_check_equal $? 0
	atf_check_equal $(tar tf $repodata | wc -l) 3
	# update pkg
	xbps-create -A noarch -n foo-1.1_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	# update repodata
	xbps-rindex -a $PWD/*.xbps --privkey ../id_xbps
	atf_check_equal $? 0
	atf_check_equal $(tar tf $repodata | wc -l) 3
}

atf_test_case verify

verify_head() {
	atf_set "descr" "xbps-rindex(1) verifying test"
}

verify_body() {
	get_resources
	# make pkg
	mkdir -p some_repo pkg_A
	touch pkg_A/file00
	cd some_repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	# make repodata
	xbps-rindex -a $PWD/*.xbps
	atf_check_equal $? 0
	repodata=$(ls *-repodata)
	# sign repodata
	xbps-rindex -s $PWD --signedby test --privkey ../id_xbps
	atf_check_equal $? 0
	# verify signature
	xbps-install -nid --repository=$PWD foo 2>&1 | grep -q "some_repo/$repodata' signature passed."
	atf_check_equal $? 0
	# modify what is signed
	tar tf $repodata
	mkdir repodata
	cd repodata
	tar xf ../$repodata
	sed -i -e 's:string>test<:string>stranger<:' index-meta.plist
	tar cf ../$repodata index.plist index-meta.plist index-meta.plist.sig
	atf_check_equal $? 0
	cd ..
	# verify wrong signature
	xbps-install -nid --repository=$PWD foo 2>&1 | grep -q "some_repo/$repodata' signature failed. Taking safe part."
	atf_check_equal $? 0
}

atf_init_test_cases() {
	atf_add_test_case sign
	atf_add_test_case verify
}
