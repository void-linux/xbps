#!/usr/bin/env atf-sh

atf_test_case hooks_basic

hooks_basic_head() {
	atf_set "descr" "Tests for basic hooks"
}

hooks_basic_body() {
	mkdir -p repo pkg_A root/etc/xbps.d/hooks

	cat <<-EOF > root/etc/xbps.d/hooks/00-pre-install-hook.hook
	[Hook]
	When = PreTransaction
	Exec = sh -c echo\ pre-install-hook\ >&2

	[Match]
	PackageInstall = A
	EOF

	cat <<-EOF > root/etc/xbps.d/hooks/00-post-install-hook.hook
	[Hook]
	When = PostTransaction
	Exec = sh -c echo\ post-install-hook\ >&2

	[Match]
	PackageInstall = A
	EOF

	cat <<-EOF > root/etc/xbps.d/hooks/00-pre-update-hook.hook
	[Hook]
	When = PreTransaction
	Exec = sh -c echo\ pre-update-hook\ >&2

	[Match]
	PackageUpdate = A
	EOF

	cat <<-EOF > root/etc/xbps.d/hooks/00-post-update-hook.hook
	[Hook]
	When = PostTransaction
	Exec = sh -c echo\ post-update-hook\ >&2

	[Match]
	PackageUpdate = A
	EOF

	cat <<-EOF > root/etc/xbps.d/hooks/00-pre-remove-hook.hook
	[Hook]
	When = PreTransaction
	Exec = sh -c echo\ pre-remove-hook\ >&2

	[Match]
	PackageRemove = A
	EOF

	cat <<-EOF > root/etc/xbps.d/hooks/00-post-remove-hook.hook
	[Hook]
	When = PostTransaction
	Exec = sh -c echo\ post-remove-hook\ >&2

	[Match]
	PackageRemove = A
	EOF

	cd repo
	atf_check -o ignore -- xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check -o ignore -- xbps-rindex -a $PWD/*.xbps
	cd ..

	atf_check \
		-o ignore \
		-e match:pre-install-hook \
		-e match:post-install-hook \
		-e not-match:pre-update-hook \
		-e not-match:post-update-hook \
		-e not-match:pre-remove-hook \
		-e not-match:post-remove-hook \
		-- xbps-install -r root --repository=repo -y A

	cd repo
	atf_check -o ignore -- xbps-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check -o ignore -e ignore -- xbps-rindex -a $PWD/*.xbps
	cd ..

	atf_check \
		-o ignore \
		-e not-match:pre-install-hook \
		-e not-match:post-install-hook \
		-e match:pre-update-hook \
		-e match:post-update-hook \
		-e not-match:pre-remove-hook \
		-e not-match:post-remove-hook \
		-- xbps-install -r root --repository=repo -yu

	atf_check \
		-o ignore \
		-e not-match:pre-install-hook \
		-e not-match:post-install-hook \
		-e not-match:pre-update-hook \
		-e not-match:post-update-hook \
		-e match:pre-remove-hook \
		-e match:post-remove-hook \
		-- xbps-remove -r root -y A
}

atf_init_test_cases() {
	atf_add_test_case hooks_basic
}
