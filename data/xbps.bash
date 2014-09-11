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
	xbps-query -s "$1*" | sed 's/^... \([^ ]*\)-.* .*/\1/'
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

	local common='-C|--config|-r|--rootdir'
	local morecommon="$common|-c|--cachedir"

	case $1 in
		xbps-dgraph)
			if [[ $prev != @(-c|-o|-r) ]]; then
				_xbps_installed_reply $cur
				return
			fi
			;;
		xbps-install)
			if [[ $prev != @($morecommon) ]]; then
				_xbps_all_reply $cur
				return
			fi
			;;
		xbps-pkgdb)
			if [[ $prev == @(-m|--mode) ]]; then
				COMPREPLY=( $( compgen -W 'auto manual hold unhold' -- "$cur") )
				return
			fi
			if [[ $prev != @($common) ]]; then
				_xbps_installed_reply $cur
				return
			fi
			;;
		xbps-query)
			if [[ $prev != @($morecommon|-p|--property|-o|--ownedby) ]]; then
				_xbps_all_reply $cur
				return
			fi
			;;
		xbps-reconfigure)
			if [[ $prev != @($common) ]]; then
				_xbps_installed_reply $cur
				return
			fi
			;;
		xbps-remove)
			if [[ $prev != @($morecommon) ]]; then
				_xbps_installed_reply $cur
				return
			fi
			;;
	esac

	_filedir
}

complete -F _xbps_complete xbps-checkvers xbps-create xbps-dgraph xbps-install \
	xbps-pkgdb xbps-query xbps-reconfigure xbps-remove xbps-rindex
