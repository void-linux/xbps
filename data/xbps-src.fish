set -l commands binary-bootstrap bootstrap bootstrap-update consistency-check chroot clean-repocache fetch extract patch configure build check install pkg clean list remove remove-autodeps purge-distfiles show show-avail show-build-deps show-check-deps show-deps show-files show-hostmakedepends show-makedepends show-options show-shlib-provides show-shlib-requires show-var show-repo-updates show-sys-updates show-local-updates sort-dependencies update-bulk update-sys update-local update-check update-hash-cache zap

complete xbps-src -f
complete xbps-src -fn "not __fish_seen_subcommand_from $commands" -a "$commands"
complete xbps-src -f -s 1 -s b -s C -s E -s f -s G -s g -s h -s I -s i -s L -s N -s n -s Q -s K -s q -s s -s t -s v -s V
complete xbps-src -x -s H -s r -s m -a "(__fish_complete_directories)"
complete xbps-src -x -s j -s p

function __fish_xbps_src_complete_pkg
	command -v xdistdir >/dev/null || return
	set -l distdir (xdistdir 2>/dev/null) || return
	ls "$distdir"/srcpkgs
end
complete xbps-src -n "__fish_seen_subcommand_from fetch extract patch configure build check install pkg clean remove show show-avail show-build-deps show-check-deps show-deps show-files show-hostmakedepends show-makedepends show-options show-shlib-provides show-shlib-requires sort-dependencies update-check" -a "(__fish_xbps_src_complete_pkg)"

function __fish_xbps_src_complete_arch
	command -v xdistdir >/dev/null || return
	set -l distdir (xdistdir 2>/dev/null) || return
	path basename "$distdir"/common/cross-profiles/*.sh | string replace .sh ''
end
complete xbps-src -x -s A -s a -a "(__fish_xbps_src_complete_arch)"

function __fish_xbps_src_complete_conf
	command -v xdistdir >/dev/null || return
	set -l distdir (xdistdir 2>/dev/null) || return
	path basename "$distdir"/etc/conf.* | string replace conf. ''
end
complete xbps-src -xs c -a "(__fish_xbps_src_complete_conf)"

function __fish_xbps_src_complete_opts
	command -v xdistdir >/dev/null || return
	set -l distdir (xdistdir 2>/dev/null) || return
	set -l args (__fish_print_cmd_args_without_options)
	test (count $args) -gt 2 || return
	set -l opts ("$distdir"/xbps-src show-options "$args[3]" 2>/dev/null | tail -n+2 | string trim | string split -f1 :)

	set -l token (commandline -t)
	if string match -qr ^-o -- $token
		set token (string sub -s 3 -- $token)
	end
	set -l prefix (string split , -- $token)[1..-2]
	for opt in $opts
		if ! string match -qr -- ~?$opt $prefix
			string join -- , $prefix $opt
			string join -- , $prefix ~$opt
		end
	end
end
complete xbps-src -xs o -a "(__fish_xbps_src_complete_opts)"
