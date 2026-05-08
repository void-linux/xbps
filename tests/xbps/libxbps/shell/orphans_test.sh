#!/usr/bin/env atf-sh

atf_test_case tc1

tc1_head() {
	atf_set "descr" "Tests for pkg orphans: https://github.com/void-linux/xbps/issues/234"
}

tc1_body() {
	mkdir -p repo pkg_A
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "A>=0" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n C-1.0_1 -s "C pkg" --dependencies "B>=0" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n D-1.0_1 -s "D pkg" --dependencies "C>=0" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root --repo=repo -yd D
	atf_check_equal $? 0
	out="$(xbps-query -r root -m)"
	atf_check_equal $? 0
	atf_check_equal "$out" "D-1.0_1"
	xbps-remove -r root -Ryd D
	atf_check_equal $? 0
	out="$(xbps-query -r root -l|wc -l)"
	atf_check_equal $? 0
	atf_check_equal "$out" "0"

	xbps-install -r root --repo=repo -yd A
	atf_check_equal $? 0
	out="$(xbps-query -r root -m)"
	atf_check_equal $? 0
	atf_check_equal "$out" "A-1.0_1"
	xbps-install -r root --repo=repo -yd D
	atf_check_equal $? 0
	xbps-remove -r root -Ryd D
	atf_check_equal $? 0
	out="$(xbps-query -r root -m)"
	atf_check_equal $? 0
	atf_check_equal "$out" "A-1.0_1"

	xbps-install -r root --repo=repo -yd D
	atf_check_equal $? 0
	xbps-pkgdb -r root -m auto A
	atf_check_equal $? 0
	xbps-remove -r root -Ryd D
	atf_check_equal $? 0
	out="$(xbps-query -r root -l|wc -l)"
	atf_check_equal $? 0
	atf_check_equal "$out" "0"

}


atf_test_case tc2

tc2_head() {
	atf_set "descr" "Tests for pkg orphans: "
}

tc2_body() {
	mkdir -p repo pkg

	atf_expect_fail "orphans broken"

	cd repo
	atf_check -o ignore -- xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n B-1.0_1 -s "B pkg" -D "A>=0" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n C-1.0_1 -s "C pkg" -D "B>=0" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n D-1.0_1 -s "D pkg" -D "C>=0" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n X-1.0_1 -s "X pkg" -D "X1>=0" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n X1-1.0_1 -s "X pkg" -D "X2>=0 X2>=0" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n X2-1.0_1 -s "X pkg" -D "X3>=0 X3>=0" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n X3-1.0_1 -s "X pkg" -D "X3>=0 X1>=0 X4>=0 X5>=0" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n X4-1.0_1 -s "X pkg" -D "X4>=0" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n X5-1.0_1 -s "X pkg" -D "X3>=0" ../pkg
	atf_check -o ignore -e ignore -- xbps-rindex -d -a $PWD/*.xbps
	cd ..

	atf_check -o ignore -e ignore -- xbps-install -r root -R repo -y A D X
	atf_check -o ignore -e ignore -- xbps-remove -r root -Rdy X1
	atf_check -o inline:"ii X-1.0_1 X pkg\n" -- xbps-query -r root -l
}

atf_test_case tc3

tc3_head() {
	atf_set "descr" "Tests for pkg orphans: "
}

tc3_body() {
	mkdir -p repo pkg

	atf_expect_fail "orphans broken"

	cd repo
	atf_check -o ignore -- xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n B-1.0_1 -s "B pkg" -D "A>=0" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n C-1.0_1 -s "C pkg" -D "B>=0" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n D-1.0_1 -s "D pkg" -D "C>=0" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n X-1.0_1 -s "X pkg" -D "X1>=0" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n X1-1.0_1 -s "X pkg" -D "X2>=0 X2>=0" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n X2-1.0_1 -s "X pkg" -D "X3>=0 X3>=0" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n X3-1.0_1 -s "X pkg" -D "X3>=0 X1>=0 X4>=0 X5>=0" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n X4-1.0_1 -s "X pkg" -D "X4>=0" ../pkg
	atf_check -o ignore -- xbps-create -A noarch -n X5-1.0_1 -s "X pkg" -D "X3>=0" ../pkg
	atf_check -o ignore -e ignore -- xbps-rindex -d -a $PWD/*.xbps
	cd ..

	atf_check -o ignore -e ignore -- xbps-install -r root -R repo -y A D X
	cat <<-EOF >expect
	ii A-1.0_1  A pkg
	ii B-1.0_1  B pkg
	ii C-1.0_1  C pkg
	ii D-1.0_1  D pkg
	ii X-1.0_1  X pkg
	ii X1-1.0_1 X pkg
	ii X2-1.0_1 X pkg
	ii X3-1.0_1 X pkg
	ii X4-1.0_1 X pkg
	ii X5-1.0_1 X pkg
	EOF
	atf_check -o file:expect -- xbps-query -r root -l
	atf_check -o ignore -e ignore -- xbps-remove -r root -dy X
	cat <<-EOF >expect
	ii A-1.0_1  A pkg
	ii B-1.0_1  B pkg
	ii C-1.0_1  C pkg
	ii D-1.0_1  D pkg
	ii X1-1.0_1 X pkg
	ii X2-1.0_1 X pkg
	ii X3-1.0_1 X pkg
	ii X4-1.0_1 X pkg
	ii X5-1.0_1 X pkg
	EOF
	atf_check -o file:expect -- xbps-query -r root -l
	atf_check -o ignore -e ignore -- xbps-remove -r root -Rdyo
	cat <<-EOF >expect
	ii A-1.0_1 A pkg
	ii B-1.0_1 B pkg
	ii C-1.0_1 C pkg
	ii D-1.0_1 D pkg
	EOF
	atf_check -o file:expect -- xbps-query -r root -l
}

atf_init_test_cases() {
	atf_add_test_case tc1
	atf_add_test_case tc2
	atf_add_test_case tc3
}
