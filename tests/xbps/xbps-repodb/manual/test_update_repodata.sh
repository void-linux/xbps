#!/usr/bin/sh

DESTDIR="${DESTDIR:-/}"

cd $(dirname ${0})
WORKDIR="$(mktemp -d)"
cp repos-2021-07-06.tar.xz "${WORKDIR}"
cp repos-2020-12-27.tar.xz "${WORKDIR}"
cp outdated-* "${WORKDIR}"
cd "$WORKDIR"

compress_index() {
	tarball="$1"
	stage="$2"
	find -name '*.plist' -delete
	tar xf "$tarball"
	for i in repos/*; do
		echo $i
		[ -e "$i/index.plist" ] || continue
		arch="$(echo ${i%-repodata} | rev | cut -d/ -f1 | cut -d_ -f1 | rev)"
		case $arch in
			64*) arch="x86_${arch%-repodata}"
		esac
		mkdir -p $arch
		[ -e $arch/$(basename $i) ] || ln -sr $i $arch
		(
			cd $i;
			tar -zcvf $arch-repodata index.plist index-meta.plist
			if [ "$stage" ]; then
				XBPS_ARCH=$arch LD_LIBRARY_PATH="${DESTDIR}/usr/local/lib" "${DESTDIR}/usr/local/bin/xbps-checkvers" -D $(xdistdir) -e --format='%n-%r.noarch.xbps' -i --repository=$PWD |
					xargs env XBPS_ARCH=$arch LD_LIBRARY_PATH="${DESTDIR}/usr/local/lib" "${DESTDIR}/usr/local/bin/xbps-rindex" -R >/dev/null
				cat ../../outdated*$arch,* | sed -e 's/$/.'$arch'.xbps/' |
					xargs env XBPS_ARCH=$arch LD_LIBRARY_PATH="${DESTDIR}/usr/local/lib" "${DESTDIR}/usr/local/bin/xbps-rindex" -R
				mv $arch-repodata $arch-stagedata
			fi
		)
		echo
	done
}

compress_index repos-2021-07-06.tar.xz stage
compress_index repos-2020-12-27.tar.xz
rm repos-2021-07-06.tar.xz repos-2020-12-27.tar.xz outdated-*
for arch in *; do
	[ "$arch" = repos ] && continue
	echo
	echo repodb $arch
	env XBPS_ARCH=$arch LD_LIBRARY_PATH="${DESTDIR}/usr/local/lib" "${DESTDIR}/usr/local/bin/xbps-repodb" -i $arch/*
done
cd ..
rm -r "$WORKDIR"
