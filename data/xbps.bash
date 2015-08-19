_xbps_parse_help() {
	local IFS line word

	$1 --help 2>&1 | while IFS=$'\n' read -r line; do
		[[ $line == *([ $'\t'])-* ]] || continue

		IFS=$' \t,='
		for word in $line; do
			[[ $word == -* ]] || continue
			printf -- '%s\n' $word
		done
	done | sort | uniq
}

_xbps_all_packages() {
	xbps-query -Rs "$1*" | sed 's/^... \([^ ]*\)-.* .*/\1/'
}

_xbps_installed_packages() {
	xbps-query -l | sed 's/^.. \([^ ]*\)-.* .*/\1/'
}

_xbps_all_reply() {
	COMPREPLY=( $( compgen -W '$(_xbps_all_packages "$1")' -- "$1") )
}

_xbps_installed_reply() {
	COMPREPLY=( $( compgen -W '$(_xbps_installed_packages)' -- "$1") )
}

_xbps_complete() {
	local cur prev words cword

	_init_completion || return

	if [[ "$cur" == -* ]]; then
		COMPREPLY=( $( compgen -W '$( _xbps_parse_help "$1" )' -- "$cur") )
		return
	fi

	local common='C|-config|r|-rootdir'
	local morecommon="$common|c|-cachedir"

	local modes='auto manual hold unhold'
	local props='architecture
		archive-compression-type
		automatic-install
		build-options
		conf_files
		conflicts
		filename-sha256
		filename-size
		homepage
		install-date
		install-msg
		install-script
		installed_size
		license
		maintainer
		metafile-sha256
		packaged-with
		pkgver
		preserve
		provides
		remove-msg
		remove-script
		replaces
		repository
		shlib-provides
		shlib-requires
		short_desc
		source-revisions
		state'

	case $1 in
		xbps-dgraph)
			if [[ $prev != -@(c|o|r) ]]; then
				_xbps_installed_reply $cur
				return
			fi
			;;
		xbps-install)
			if [[ $prev != -@($morecommon) ]]; then
				_xbps_all_reply $cur
				return
			fi
			;;
		xbps-pkgdb)
			if [[ $prev == -@(m|-mode) ]]; then
				COMPREPLY=( $( compgen -W "$modes" -- "$cur") )
				return
			fi
			if [[ $prev != -@($common) ]]; then
				_xbps_installed_reply $cur
				return
			fi
			;;
		xbps-query)
			if [[ $prev == -@(p|-property) ]]; then
				COMPREPLY=( $( compgen -W "$props" -- "$cur") )
				return
			fi
			if [[ $prev != -@($morecommon|o|-ownedby) ]]; then
				local w
				for w in "${words[@]}"; do
					if [[ "$w" == -@(R|-repository) ]]; then
						_xbps_all_reply $cur
						return
					fi
				done
				_xbps_installed_reply $cur
				return
			fi
			;;
		xbps-reconfigure)
			if [[ $prev != -@($common) ]]; then
				_xbps_installed_reply $cur
				return
			fi
			;;
		xbps-remove)
			if [[ $prev != -@($morecommon) ]]; then
				_xbps_installed_reply $cur
				return
			fi
			;;
	esac

	_filedir
}

complete -F _xbps_complete xbps-checkvers xbps-create xbps-dgraph xbps-install \
	xbps-pkgdb xbps-query xbps-reconfigure xbps-remove xbps-rindex
