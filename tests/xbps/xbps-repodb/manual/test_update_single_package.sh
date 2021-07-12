#!/usr/bin/sh

DESTDIR="${DESTDIR:-/}"

cd $(dirname ${0})
WORKDIR="$(mktemp -d)"
cp repos-fromempty.tar.xz "${WORKDIR}"
cd "$WORKDIR"
tar xf repos-fromempty.tar.xz
for i in repos/*x86_64-repodata; do
	(cd $i &&
		pwd &&
		tar -xf x86_64-repodata
		sed -i index.plist -e 's/>zlib-1.2.11_/&1/' -e 's/>zlib-dbg-1.2.11_/&1/'
		tar -zcf x86_64-stagedata index.plist index-meta.plist
	)
done
LD_LIBRARY_PATH="${DESTDIR}/usr/local/lib" "${DESTDIR}/usr/local/bin/xbps-repodb" -i repos/*
