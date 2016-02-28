#!/bin/sh

test -d m4 || mkdir m4
AUTOMAKE=${AUTOMAKE:-automake}

set -ex

${ACLOCAL:-aclocal$AM_VERSION} ${ACLOCAL_ARG}
${AUTOHEADER:-autoheader$AC_VERSION} --force
AUTOMAKE=$AUTOMAKE libtoolize -c --automake --force
AUTOMAKE=$AUTOMAKE intltoolize -c --automake --force
$AUTOMAKE --add-missing --copy --include-deps
${AUTOCONF:-autoconf$AC_VERSION}

rm -rf autom4te.cache
