#! /usr/bin/env atf-sh

atf_test_case register_one

register_one_head() {
	atf_set "descr" "xbps-alternatives: register one pkg with an alternatives group"
}
register_one_body() {
	mkdir -p repo pkg_A/usr/bin
	touch pkg_A/usr/bin/fileA
	cd repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" --alternatives "file:/usr/bin/file:/usr/bin/fileA" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=repo -ydv A
	atf_check_equal $? 0
	rv=1
	if [ -e root/usr/bin/fileA ]; then
		lnk=$(readlink -f root/usr/bin/file)
		if [ "$lnk" = "$PWD/root/usr/bin/fileA" ]; then
			rv=0
		fi
		echo "A lnk: $lnk"
	fi
	atf_check_equal $rv 0
}

atf_test_case register_one_dangling

register_one_dangling_head() {
	atf_set "descr" "xbps-alternatives: register one pkg with an alternative dangling symlink"
}
register_one_dangling_body() {
	mkdir -p repo pkg_A/usr/bin
	cd repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" --alternatives "file:/usr/bin/file:/usr/bin/fileA file2:/usr/lib/fileB:/usr/include/fileB" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=repo -ydv A
	atf_check_equal $? 0
	rv=1
	if [ -h root/usr/bin/file ]; then
		lnk=$(readlink -f root/usr/bin/file)
		if [ "$lnk" = "$PWD/root/usr/bin/fileA" ]; then
			rv=0
		fi
		echo "A lnk: $lnk"
	fi
	atf_check_equal $rv 0
	rv=1
	if [ -h root/usr/lib/fileB ]; then
		lnk=$(readlink -f root/usr/lib/fileB)
		if [ "$lnk" = "$PWD/root/usr/include/fileB" ]; then
			rv=0
		fi
		echo "A lnk: $lnk"
	fi
	atf_check_equal $rv 0
}

atf_test_case register_one_relative

register_one_relative_head() {
	atf_set "descr" "xbps-alternatives: register one pkg with an alternatives group that has a relative path"
}
register_one_relative_body() {
	mkdir -p repo pkg_A/usr/bin
	touch pkg_A/usr/bin/fileA
	cd repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" --alternatives "file:file:/usr/bin/fileA" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=repo -ydv A
	atf_check_equal $? 0
	rv=1
	if [ -e root/usr/bin/fileA ]; then
		lnk=$(readlink root/usr/bin/file)
		if [ "$lnk" = "fileA" ]; then
			rv=0
		fi
		echo "A lnk: $lnk"
	fi
	atf_check_equal $rv 0
}

atf_test_case register_dups

register_dups_head() {
	atf_set "descr" "xbps-alternatives: do not register dup alternative groups"
}
register_dups_body() {
	mkdir -p repo pkg_A/usr/bin
	touch pkg_A/usr/bin/fileA
	cd repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" --alternatives "file:/usr/bin/file:/usr/bin/fileA" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=repo -ydv A
	atf_check_equal $? 0
	xbps-install -r root --repository=repo -ydfv A
	atf_check_equal $? 0
	xbps-install -r root --repository=repo -ydfv A
	atf_check_equal $? 0
	atf_check_equal "$(xbps-alternatives -r root -l|wc -l)" 3
}

atf_test_case register_multi

register_multi_head() {
	atf_set "descr" "xbps-alternatives: register multiple pkgs with an alternatives group"
}
register_multi_body() {
	mkdir -p repo pkg_A/usr/bin pkg_B/usr/bin
	touch pkg_A/usr/bin/fileA pkg_B/usr/bin/fileB
	cd repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" --alternatives "file:/usr/bin/file:/usr/bin/fileA" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.1_1 -s "B pkg" --alternatives "file:/usr/bin/file:/usr/bin/fileB" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=repo -ydv A
	atf_check_equal $? 0
	rv=1
	if [ -e root/usr/bin/fileA ]; then
		lnk=$(readlink -f root/usr/bin/file)
		if [ "$lnk" = "$PWD/root/usr/bin/fileA" ]; then
			rv=0
		fi
		echo "A lnk: $lnk"
	fi
	atf_check_equal $rv 0

	xbps-install -r root --repository=repo -ydv B
	atf_check_equal $? 0
	rv=1
	if [ -e root/usr/bin/fileA -a -e root/usr/bin/fileB ]; then
		lnk=$(readlink -f root/usr/bin/file)
		if [ "$lnk" = "$PWD/root/usr/bin/fileA" ]; then
			rv=0
		fi
		echo "B lnk: $lnk"
	fi
	atf_check_equal $rv 0
}

atf_test_case unregister_one

unregister_one_head() {
	atf_set "descr" "xbps-alternatives: unregister one pkg with an alternatives group"
}
unregister_one_body() {
	mkdir -p repo pkg_A/usr/bin
	touch pkg_A/usr/bin/fileA
	cd repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" --alternatives "file:/usr/bin/file:/usr/bin/fileA" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=repo -ydv A
	atf_check_equal $? 0
	xbps-remove -r root -yvd A
	rv=1
	if [ ! -e root/usr/bin/file -a ! -e root/usr/bin/fileA ]; then
		rv=0
	fi
	atf_check_equal $rv 0
}

atf_test_case unregister_one_relative

unregister_one_relative_head() {
	atf_set "descr" "xbps-alternatives: unregister one pkg with an alternatives group that has a relative path"
}
unregister_one_relative_body() {
	mkdir -p repo pkg_A/usr/bin
	touch pkg_A/usr/bin/fileA
	cd repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" --alternatives "file:file:/usr/bin/fileA" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=repo -ydv A
	atf_check_equal $? 0
	xbps-remove -r root -yvd A
	rv=1
	if [ ! -e root/usr/bin/file -a ! -e root/usr/bin/fileA ]; then
		rv=0
	fi
	atf_check_equal $rv 0
}

atf_test_case unregister_multi

unregister_multi_head() {
	atf_set "descr" "xbps-alternatives: unregister multiple pkgs with an alternatives group"
}
unregister_multi_body() {
	mkdir -p repo pkg_A/usr/bin pkg_B/usr/bin
	touch pkg_A/usr/bin/fileA pkg_B/usr/bin/fileB
	cd repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" --alternatives "file:/usr/bin/file:/usr/bin/fileA" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.1_1 -s "B pkg" --alternatives "file:/usr/bin/file:/usr/bin/fileB" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=repo -ydv A
	atf_check_equal $? 0

	if [ -e root/usr/bin/fileA ]; then
		lnk=$(readlink -f root/usr/bin/file)
		if [ "$lnk" = "$PWD/root/usr/bin/fileA" ]; then
			rv=0
		fi
		echo "A lnk: $lnk"
	fi
	atf_check_equal $rv 0
	xbps-remove -r root -yvd A
	rv=1
	if [ ! -e root/usr/bin/file -a ! -e root/usr/bin/fileA ]; then
		rv=0
	fi
	atf_check_equal $rv 0

	xbps-install -r root --repository=repo -ydv B
	atf_check_equal $? 0

	if [ -e root/usr/bin/fileB ]; then
		lnk=$(readlink -f root/usr/bin/file)
		if [ "$lnk" = "$PWD/root/usr/bin/fileB" ]; then
			rv=0
		fi
		echo "A lnk: $lnk"
	fi
	atf_check_equal $rv 0

	xbps-remove -r root -yvd B
	rv=1
	if [ ! -e root/usr/bin/file -a ! -e root/usr/bin/fileB ]; then
		rv=0
	fi
	atf_check_equal $rv 0
}

atf_test_case set_pkg

set_pkg_head() {
	atf_set "descr" "xbps-alternatives: set all alternative groups from pkg"
}
set_pkg_body() {
	mkdir -p repo pkg_A/usr/bin pkg_B/usr/bin
	touch pkg_A/usr/bin/A1 pkg_A/usr/bin/A2 pkg_B/usr/bin/B1 pkg_B/usr/bin/B2
	cd repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" --alternatives "1:/usr/bin/1:/usr/bin/A1 2:/usr/bin/2:/usr/bin/A2" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.1_1 -s "B pkg" --alternatives "1:/usr/bin/1:/usr/bin/B1 2:/usr/bin/2:/usr/bin/B2" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=repo -ydv A B
	atf_check_equal $? 0
	xbps-alternatives -r root -s B
	atf_check_equal $? 0

	rv=1
	if [ -e root/usr/bin/B1 ]; then
		lnk=$(readlink -f root/usr/bin/1)
		if [ "$lnk" = "$PWD/root/usr/bin/B1" ]; then
			rv=0
		fi
		echo "A lnk: $lnk"
	fi
	atf_check_equal $rv 0

	rv=1
	if [ -e root/usr/bin/B2 ]; then
		lnk=$(readlink -f root/usr/bin/2)
		if [ "$lnk" = "$PWD/root/usr/bin/B2" ]; then
			rv=0
		fi
		echo "A lnk: $lnk"
	fi
	atf_check_equal $rv 0

	xbps-alternatives -r root -s A
	atf_check_equal $? 0

	rv=1
	if [ -e root/usr/bin/A1 ]; then
		lnk=$(readlink -f root/usr/bin/1)
		if [ "$lnk" = "$PWD/root/usr/bin/A1" ]; then
			rv=0
		fi
		echo "A lnk: $lnk"
	fi
	atf_check_equal $rv 0

	rv=1
	if [ -e root/usr/bin/A2 ]; then
		lnk=$(readlink -f root/usr/bin/2)
		if [ "$lnk" = "$PWD/root/usr/bin/A2" ]; then
			rv=0
		fi
		echo "A lnk: $lnk"
	fi
	atf_check_equal $rv 0
}

atf_test_case set_pkg_group

set_pkg_group_head() {
	atf_set "descr" "xbps-alternatives: set one alternative group from pkg"
}
set_pkg_group_body() {
	mkdir -p repo pkg_A/usr/bin pkg_B/usr/bin
	touch pkg_A/usr/bin/A1 pkg_A/usr/bin/A2 pkg_B/usr/bin/B1 pkg_B/usr/bin/B2
	cd repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" --alternatives "1:/usr/bin/1:/usr/bin/A1 2:/usr/bin/2:/usr/bin/A2" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.1_1 -s "B pkg" --alternatives "1:/usr/bin/1:/usr/bin/B1 2:/usr/bin/2:/usr/bin/B2" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=repo -ydv A B
	atf_check_equal $? 0
	xbps-alternatives -r root -s A -g 1
	atf_check_equal $? 0
	xbps-alternatives -r root -s B -g 2
	atf_check_equal $? 0

	rv=1
	if [ -e root/usr/bin/B1 ]; then
		lnk=$(readlink -f root/usr/bin/1)
		if [ "$lnk" = "$PWD/root/usr/bin/A1" ]; then
			rv=0
		fi
		echo "A lnk: $lnk"
	fi
	atf_check_equal $rv 0

	rv=1
	if [ -e root/usr/bin/B2 ]; then
		lnk=$(readlink -f root/usr/bin/2)
		if [ "$lnk" = "$PWD/root/usr/bin/B2" ]; then
			rv=0
		fi
		echo "A lnk: $lnk"
	fi
	atf_check_equal $rv 0

}

atf_test_case update_pkgs

update_pkgs_head() {
	atf_set "descr" "xbps-alternatives: preserve order in updates"
}
update_pkgs_body() {
	mkdir -p repo pkg_A/usr/bin pkg_B/usr/bin
	touch pkg_A/usr/bin/A1 pkg_B/usr/bin/B1
	cd repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" --alternatives "1:1:/usr/bin/A1" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.1_1 -s "B pkg" --alternatives "1:1:/usr/bin/B1" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=repo -ydv A B
	atf_check_equal $? 0

	rv=1
	if [ -e root/usr/bin/A1 ]; then
		lnk=$(readlink -f root/usr/bin/1)
		if [ "$lnk" = "$PWD/root/usr/bin/A1" ]; then
			rv=0
		fi
		echo "lnk: $lnk"
	fi
	atf_check_equal $rv 0

	cd repo
	xbps-create -A noarch -n A-1.2_1 -s "A pkg" --alternatives "1:1:/usr/bin/A1" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.2_1 -s "B pkg" --alternatives "1:1:/usr/bin/B1" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-alternatives -r root -s B
	atf_check_equal $? 0

	xbps-install -r root --repository=repo -yuvd
	atf_check_equal $? 0

	rv=1
	if [ -e root/usr/bin/B1 ]; then
		lnk=$(readlink -f root/usr/bin/1)
		if [ "$lnk" = "$PWD/root/usr/bin/B1" ]; then
			rv=0
		fi
		echo "lnk: $lnk"
	fi
	atf_check_equal $rv 0
}

atf_test_case less_entries

less_entries_pkgs_head() {
	atf_set "descr" "xbps-alternatives: remove symlinks not provided by the new alternative"
}
less_entries_body() {
	mkdir -p repo pkg_A/usr/bin pkg_B/usr/bin
	touch pkg_A/usr/bin/A1 pkg_A/usr/bin/A2 pkg_B/usr/bin/B1
	cd repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" --alternatives "1:1:/usr/bin/A1 1:2:/usr/bin/A2" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.1_1 -s "B pkg" --alternatives "1:1:/usr/bin/B1" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=repo -ydv A B
	atf_check_equal $? 0

	xbps-alternatives -r root -s B
	atf_check_equal $? 0

	rv=1
	[ -e root/usr/bin/2 ] || rv=0
	atf_check_equal $rv 0
}

atf_test_case useless_switch

useless_switch_head() {
	atf_set "descr" "xbps-alternatives: avoid useless switches on package removal"
}
useless_switch_body() {
	mkdir -p repo pkg_A/usr/bin pkg_B/usr/bin
	touch pkg_A/usr/bin/fileA pkg_B/usr/bin/fileB
	cd repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" --alternatives "file:/usr/bin/file:/usr/bin/fileA" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.1_1 -s "B pkg" --alternatives "file:/usr/bin/file:/usr/bin/fileB" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=repo -ydv A
	atf_check_equal $? 0
	rv=1
	if [ -e root/usr/bin/fileA ]; then
		lnk=$(readlink -f root/usr/bin/file)
		if [ "$lnk" = "$PWD/root/usr/bin/fileA" ]; then
			rv=0
		fi
		echo "A lnk: $lnk"
	fi
	atf_check_equal $rv 0

	xbps-install -r root --repository=repo -ydv B
	atf_check_equal $? 0
	rv=1
	if [ -e root/usr/bin/fileA -a -e root/usr/bin/fileB ]; then
		lnk=$(readlink -f root/usr/bin/file)
		if [ "$lnk" = "$PWD/root/usr/bin/fileA" ]; then
			rv=0
		fi
		echo "B lnk: $lnk"
	fi
	atf_check_equal $rv 0
	
	ln -sf /dev/null root/usr/bin/file
	xbps-remove -r root -ydv B
	atf_check_equal $? 0
	lnk=$(readlink -f root/usr/bin/file)
	rv=1
	if [ "$lnk" = "/dev/null" ]; then
		rv=0
	fi
	echo "X lnk: $lnk"
	atf_check_equal $rv 0
}

atf_test_case remove_defprovider

remove_defprovider_head() {
	atf_set "descr" "xbps-alternatives: removing default provider"
}
remove_defprovider_body() {
	mkdir -p repo pkg_A/usr/bin pkg_B/usr/bin
	touch pkg_A/usr/bin/fileA pkg_B/usr/bin/fileB
	cd repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" --alternatives "file:/usr/bin/file:/usr/bin/fileA" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.1_1 -s "B pkg" --alternatives "file:/usr/bin/file:/usr/bin/fileB" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=repo -ydv A
	atf_check_equal $? 0
	xbps-install -r root --repository=repo -ydv B
	atf_check_equal $? 0

	xbps-alternatives -s B
	atf_check_equal $? 0

	xbps-remove -r root -ydv B
	atf_check_equal $? 0
	lnk=$(readlink -f root/usr/bin/file)
	rv=0
	if [ "$lnk" = "$PWD/root/usr/bin/fileB" ]; then
		rv=1
	fi
	echo "lnk: $lnk"
	atf_check_equal $rv 0
}

atf_init_test_cases() {
	atf_add_test_case register_one
	atf_add_test_case register_one_dangling
	atf_add_test_case register_one_relative
	atf_add_test_case register_dups
	atf_add_test_case register_multi
	atf_add_test_case unregister_one
	atf_add_test_case unregister_one_relative
	atf_add_test_case unregister_multi
	atf_add_test_case set_pkg
	atf_add_test_case set_pkg_group
	atf_add_test_case update_pkgs
	atf_add_test_case less_entries
	atf_add_test_case useless_switch
	atf_add_test_case remove_defprovider
}
