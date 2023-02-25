#! /usr/bin/env atf-sh
# Test that xbps-sign(1) works as expected.

atf_test_case generate_errors

errors_head() {
	atf_set "descr" "xbps-sign(1): error conditions"
}

run() {
	local e="ignore"
	local o="ignore"
	local s
	while getopts e:o:s: f; do
		case "$f" in
		e) e="$OPTARG" ;;
		o) o="$OPTARG" ;;
		s) s="$OPTARG" ;;
		esac
	done
	shift $(( OPTIND - 1 ))
	# atf_check ${s+-s "$s"} -e "$e" -o "$o" -- gdb --return-child-result -q -batch -ex r -ex bt -args "$@"
	${Atf_Check} ${s+-s "$s"} -e "$e" -o "$o" -- "$@"
	local rv=$?
	if [ $rv != 0 ]; then
		[ -f core.* ] && gdb -q -batch -ex r -ex 'bt f' "$1" core.*
		atf_fail "atf-check failed; see the output of the test for details"
	fi 
}

errors_body() {
	touch existing

	# generate
	atf_check -s exit:1 -e match:"missing secret-key" xbps-sign -G
	atf_check -s exit:1 -e match:"missing secret-key" xbps-sign -G -p test.pub
	# atf_check -s exit:1 -e match:"missing public" xbps-sign -G -s test.key
	atf_check -s exit:1 -e match:"existing" xbps-sign -G -s existing -p test.pub
	# atf_check -s exit:1 -e match:"existing" xbps-sign -G -s test.key -p existing
	# atf_check -s exit:1 -e match:"failed to read passphrase file" \
	# 	xbps-sign -G -s test.key -p test.pub --passphrase-file fail

	# sign
	atf_check -s exit:1 -e match:"missing secret-key" xbps-sign -S -m existing
	atf_check -s exit:1 -e match:"missing file to sign" xbps-sign -S -s existing
	atf_check -s exit:1 -e match:"failed to read secret-key file" xbps-sign -S -s missing -m existing
	atf_check -s exit:1 -e match:"failed to hash file" xbps-sign -S -s existing -m missing

	#verify
}

atf_test_case unencrypted

unencrypted_head() {
	atf_set "descr" "xbps-sign(1): generate and use unencrypted key pair"
}

unencrypted_body() {
	echo abc123 >testfile
	echo 123abc >badfile
	run -e match:"unencrypted" xbps-sign -G -s test.key -p test.pub
	run test -r test.key
	run xbps-sign -d -S -p test.pub -s test.key -m testfile
	run xbps-sign -d -V -p test.pub -m testfile
	run -s exit:1 xbps-sign -d -V -p test.pub -m badfile -x testfile.minisig
}

atf_test_case encrypted

encrypted_head() {
	atf_set "descr" "xbps-sign(1): generate and use encrypted key pair"
}

encrypted_body() {
	atf_skip "wtf not fully implemented"
	echo "abc123" > passphrase
	atf_check -s exit:0 xbps-sign -G -s test.key -p test.pub --passphrase-file passphrase
	test -r test.key || atf_fail "test.key does not exist"
	test -r test.pub || atf_fail "test.pub does not exist"
	atf_check -s exit:0 xbps-sign -S -s test.key
}

atf_test_case minisign

minisign_head() {
	atf_set "descr" "xbps-sign(1): test minisign compatibility"
}

minisign_body() {
	atf_require_prog minisign
	echo "abc123" > test

	# minisign verify using xbps-sign generated keypair and signature
	run xbps-sign -G -s test.key -p test.pub
	run xbps-sign -S -s test.key -m test
	run minisign -V -p test.pub -m test

	# minisign sign using xbps-generated keypair and verify using xbps-sign
	rm test.minisig
	run minisign -S -s test.key -m test
	run xbps-sign -V -p test.pub -m test

	# minising generated keypair
	rm test.pub test.key test.minisig
	run minisign -GW -s test.key -p test.pub
	run minisign -S -s test.key -m test
	run xbps-sign -V -p test.pub -m test
}

atf_init_test_cases() {
	atf_add_test_case errors
	atf_add_test_case unencrypted
	atf_add_test_case encrypted
	atf_add_test_case minisign
}
