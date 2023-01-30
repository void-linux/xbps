#! /usr/bin/env atf-sh

atf_test_case remove_directory

remoe_directory_head() {
	atf_set "descr" "xbps-remove(1): remove nested directories"
}

remove_directory_body() {
	mkdir -p some_repo pkg_A/B/C
	touch pkg_A/B/C/file00
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -y A
	atf_check_equal $? 0
	xbps-remove -r root -C empty.conf -y A
	atf_check_equal $? 0
	test -d root/B
	atf_check_equal $? 1
}

atf_test_case remove_orphans

remove_orphans_head() {
	atf_set "descr" "xbps-remove(1): remove orphaned packages"
}

remove_orphans_body() {
	mkdir -p some_repo pkg_A/B/C
	touch pkg_A/
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -yA A
	atf_check_equal $? 0
	xbps-remove -r root -C empty.conf -yvdo
	atf_check_equal $? 0
	xbps-query -r root A
	atf_check_equal $? 2
}

atf_test_case clean_cache

clean_cache_head() {
	atf_set "descr" "xbps-remove(1): clean cache"
}

clean_cache_body() {
	mkdir -p repo pkg_A/B/C pkg_B
	touch pkg_A/
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n A-1.0_2 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	mkdir -p root/etc/xbps.d root/var/db/xbps/https___localhost_ root/var/cache/xbps
	cp repo/*-repodata root/var/db/xbps/https___localhost_
	atf_check_equal $? 0
	cp repo/*.xbps root/var/cache/xbps
	atf_check_equal $? 0
	echo "repository=https://localhost/" >root/etc/xbps.d/localrepo.conf
	xbps-install -r root -C etc/xbps.d -R repo -dvy B
	xbps-remove -r root -C etc/xbps.d -dvO
	atf_check_equal $? 0
	test -f root/var/cache/xbps/A-1.0_2.noarch.xbps
	atf_check_equal $? 0
	test -f root/var/cache/xbps/A-1.0_1.noarch.xbps
	atf_check_equal $? 1
	test -f root/var/cache/xbps/B-1.0_1.noarch.xbps
	atf_check_equal $? 0
}

atf_test_case clean_cache_dry_run

clean_cache_dry_run_head() {
	atf_set "descr" "xbps-remove(1): clean cache dry run"
}

clean_cache_dry_run_body() {
	mkdir -p repo pkg_A/B/C
	touch pkg_A/
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n A-1.0_2 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	mkdir -p root/etc/xbps.d root/var/db/xbps/https___localhost_ root/var/cache/xbps
	cp repo/*-repodata root/var/db/xbps/https___localhost_
	atf_check_equal $? 0
	cp repo/*.xbps root/var/cache/xbps
	atf_check_equal $? 0
	echo "repository=https://localhost/" >root/etc/xbps.d/localrepo.conf
	ls -lsa root/var/cache/xbps
	out="$(xbps-remove -r root -C etc/xbps.d -dvnO)"
	atf_check_equal $? 0
	atf_check_equal "$out" "Removed A-1.0_1.noarch.xbps from cachedir (obsolete)"
}

atf_test_case clean_cache_dry_run_perm

clean_cache_dry_run_perm_head() {
	atf_set "descr" "xbps-remove(1): clean cache dry run without read permissions"
}

clean_cache_dry_run_perm_body() {
	# this should print an error instead of dry deleting the files it can't read
	mkdir -p repo pkg_A/B/C
	touch pkg_A/
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n A-1.0_2 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	mkdir -p root/etc/xbps.d root/var/db/xbps/https___localhost_ root/var/cache/xbps
	cp repo/*-repodata root/var/db/xbps/https___localhost_
	atf_check_equal $? 0
	cp repo/*.xbps root/var/cache/xbps
	atf_check_equal $? 0
	chmod 0000 root/var/cache/xbps/*.xbps
	echo "repository=https://localhost/" >root/etc/xbps.d/localrepo.conf
	out="$(xbps-remove -r root -C etc/xbps.d -dvnO)"
	atf_check_equal $? 0
	atf_check_equal "$out" "Removed A-1.0_1.noarch.xbps from cachedir (obsolete)"
}

clean_cache_uninstalled_head() {
	atf_set "descr" "xbps-remove(1): clean uninstalled package from cache"
}

clean_cache_uninstalled_body() {
	mkdir -p repo pkg_A/B/C pkg_B
	touch pkg_A/
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n A-1.0_2 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	mkdir -p root/etc/xbps.d root/var/db/xbps/https___localhost_ root/var/cache/xbps
	cp repo/*-repodata root/var/db/xbps/https___localhost_
	atf_check_equal $? 0
	cp repo/*.xbps root/var/cache/xbps
	atf_check_equal $? 0
	echo "repository=https://localhost/" >root/etc/xbps.d/localrepo.conf
	xbps-install -r root -C etc/xbps.d -R repo -dvy B
	atf_check_equal $? 0
	xbps-remove -r root -C etc/xbps.d -dvOO
	atf_check_equal $? 0
	test -f root/var/cache/xbps/A-1.0_2.noarch.xbps
	atf_check_equal $? 1
	test -f root/var/cache/xbps/A-1.0_1.noarch.xbps
	atf_check_equal $? 1
	test -f root/var/cache/xbps/B-1.0_1.noarch.xbps
	atf_check_equal $? 0
}

atf_init_test_cases() {
	atf_add_test_case remove_directory
	atf_add_test_case remove_orphans
	atf_add_test_case clean_cache
	atf_add_test_case clean_cache_dry_run
	atf_add_test_case clean_cache_dry_run_perm
	atf_add_test_case clean_cache_uninstalled
}
