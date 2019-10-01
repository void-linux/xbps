#! /usr/bin/env atf-sh

atf_test_case one_outmoding

one_outmoding_head(){
	atf_set "descr" "xbps-install(1): outmoding by one package"
}

one_outmoding_body() {
	mkdir -p some_repo pkg_{A,B}
	touch pkg_{A,B}/file00
	cd some_repo
	xbps-create -A noarch -n gksu-1.0_2 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n lxqt-sudo-1.1_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -y gksu
	atf_check_equal $? 0

	xbps-query -r root gksu
	atf_check_equal $? 0
	xbps-query -r root lxqt-sudo
	atf_check_equal $? 2

	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -y -u
	atf_check_equal $? 0

	xbps-query -r root gksu
	atf_check_equal $? 2
	xbps-query -r root lxqt-sudo
	atf_check_equal $? 0
}

atf_test_case two_outmodings

two_outmoding_head(){
	atf_set "descr" "xbps-install(1): outmoding by two packages"
}

two_outmoding_body() {
	mkdir -p some_repo pkg_{A,B,C}
	for i in A B C; do
		touch pkg_${i}/file${i}
	done
	cd some_repo
	xbps-create -A noarch -n profont-1.0_2 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n dina-font-1.1_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-create -A noarch -n source-sans-pro-1.1_1 -s "C pkg" ../pkg_C
	atf_check_equal $? 0
	xbps-rindex -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -y profont
	atf_check_equal $? 0

	xbps-query -r root profont
	atf_check_equal $? 0
	xbps-query -r root dina-font
	atf_check_equal $? 2
	xbps-query -r root source-sans-pro
	atf_check_equal $? 2

	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -y -u
	atf_check_equal $? 0

	xbps-query -r root profont
	atf_check_equal $? 2
	xbps-query -r root dina-font
	atf_check_equal $? 0
	xbps-query -r root source-sans-pro
	atf_check_equal $? 0
}

atf_test_case no_outmoding

no_outmoding_head(){
	atf_set "descr" "xbps-install(1): outmoding without replacement"
}

no_outmoding_body() {
	mkdir -p some_repo pkg_A
	touch pkg_A/file00
	cd some_repo
	xbps-create -A noarch -n oksh-0.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -y oksh
	atf_check_equal $? 0

	xbps-query -r root oksh
	atf_check_equal $? 0

	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -y -u
	atf_check_equal $? 0

	xbps-query -r root oksh
	atf_check_equal $? 2
}

atf_test_case readded

readded_head(){
	atf_set "descr" "xbps-install(1): readding outmoded package"
}

readded_body() {
	mkdir -p some_repo pkg_A
	touch pkg_A/file00
	cd some_repo
	xbps-create -A noarch -n tweeny_3-1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -y tweeny
	atf_check_equal $? 0

	xbps-query -r root tweeny
	atf_check_equal $? 0

	xbps-install -r root -C empty.conf --repository=$PWD/some_repo -y -u
	atf_check_equal $? 0

	xbps-query -r root tweeny
	atf_check_equal $? 0
}

atf_init_test_cases() {
	atf_add_test_case one_outmoding
	atf_add_test_case two_outmoding
	atf_add_test_case no_outmoding
	atf_add_test_case readded
}
