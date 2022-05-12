#!/bin/bash

function config()
{
    autoreconf -i -f || return $?
    ./configure
}

SAVED_PWD=$PWD
version=`grep '\[VERSION_NUMBER], \[.*\]' version.m4 |grep '[0-9]\+\.[0-9]\+\.[0-9]\+' -o`

trap "cd $SAVED_PWD; rm -rf ding-libs-${version} ding-libs-${version}.tar" EXIT

git archive --format=tar --prefix=ding-libs-${version}/ ${version} > ding-libs-${version}.tar
if [ $? -ne 0 ]; then
    echo "Cannot perform git-archive, check if tag $version is present in git tree"
    exit 1
fi
tar xf ding-libs-${version}.tar

pushd ding-libs-${version}
config || exit 1
make dist-gzip || exit 1  # also builds docs
popd

mv ding-libs-${version}/ding-libs-${version}.tar.gz .
gpg --detach-sign --armor ding-libs-${version}.tar.gz

