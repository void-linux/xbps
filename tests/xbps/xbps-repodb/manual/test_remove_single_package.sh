#!/usr/bin/sh

DESTDIR="${DESTDIR:-/}"

cd $(dirname ${0})
WORKDIR="$(mktemp -d)"
cp repos-fromempty.tar.xz "${WORKDIR}"
cd "$WORKDIR"
tar xf repos-fromempty.tar.xz
for i in repos/*x86_64-repodata; do
	(cd $i &&
		cp x86_64-repodata x86_64-stagedata
		LD_LIBRARY_PATH="${DESTDIR}/usr/local/lib" "${DESTDIR}/usr/local/bin/xbps-rindex" --stage --remove zlib-1.2.11_3.x86_64.xbps
	)
done
LD_LIBRARY_PATH="${DESTDIR}/usr/local/lib" "${DESTDIR}/usr/local/bin/xbps-repodb" -i repos/*
