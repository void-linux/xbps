#!/usr/bin/env atf-sh

atf_test_case conflicts_trans

conflicts_trans_head() {
	atf_set "descr" "Tests for pkg conflicts: conflicting pkgs in transaction"
}

conflicts_trans_body() {
	mkdir some_repo
	mkdir -p pkg_{A,B}/usr/bin
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" --conflicts "B>=0" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0

	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/some_repo -dy A B
	# EAGAIN, conflicts.
	atf_check_equal $? 11
	# 0 pkgs installed.
	xbps-query -r root -l|wc -l
	atf_check_equal $(xbps-query -r root -l|wc -l) 0
}

atf_test_case conflicts_installed

conflicts_installed_head() {
	atf_set "descr" "Tests for pkg conflicts: installed pkg conflicts with pkg in transaction"
}

conflicts_installed_body() {
	mkdir some_repo
	mkdir -p pkg_{A,B}/usr/bin
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" --conflicts "B>=0" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0

	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/some_repo -dy A
	atf_check_equal $? 0
	xbps-install -r root --repository=$PWD/some_repo -dy B
	atf_check_equal $? 11
	xbps-query -r root -l|wc -l
	atf_check_equal $(xbps-query -r root -l|wc -l) 1
}

atf_test_case conflicts_trans_installed

conflicts_trans_installed_head() {
	atf_set "descr" "Tests for pkg conflicts: pkg in transaction conflicts with installed pkg"
}

conflicts_trans_installed_body() {
	mkdir some_repo
	mkdir -p pkg_{A,B}/usr/bin
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" --conflicts "B>=0" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0

	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/some_repo -dy B
	atf_check_equal $? 0
	xbps-install -r root --repository=$PWD/some_repo -dy A
	atf_check_equal $? 11
	xbps-query -r root -l|wc -l
	atf_check_equal $(xbps-query -r root -l|wc -l) 1
}

atf_init_test_cases() {
	atf_add_test_case conflicts_trans
	atf_add_test_case conflicts_trans_installed
	atf_add_test_case conflicts_installed
}
