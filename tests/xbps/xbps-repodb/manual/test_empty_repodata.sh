#!/usr/bin/sh

DESTDIR="${DESTDIR:-/}"

cd $(dirname ${0})
WORKDIR="$(mktemp -d)"
cp repos-2021-07-06.tar.xz "${WORKDIR}"
cd "$WORKDIR"
tar xf repos-2021-07-06.tar.xz
for i in repos/*; do
	echo $i
	arch="$(echo ${i%-repodata} | rev | cut -d/ -f1 | cut -d_ -f1 | rev)"
	case $arch in
		64*) arch="x86_${arch%-repodata}"
	esac
	[ -z "$arch" ] && continue
	mkdir -p $arch
	ln -sr $i $arch
	echo $arch-stagedata
	(cd $i; tar -zcvf $arch-stagedata index.plist index-meta.plist)
	echo
done

rm repos-2021-07-06.tar.xz
for arch in *; do
	[ "$arch" = repos ] && continue
	echo
	echo repodb $arch
	env XBPS_ARCH=$arch LD_LIBRARY_PATH="${DESTDIR}/usr/local/lib" "${DESTDIR}/usr/local/bin/xbps-repodb" -i $arch/*
done
cd ..
rm -r "$WORKDIR"


