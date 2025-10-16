#!/bin/sh
set -e

# Ensure maintainer identity is set (dpkg-buildpackage requires it)
if [ -z "${DEBFULLNAME:-}" ] || [ -z "${DEBEMAIL:-}" ]; then
  echo "Please set DEBFULLNAME and DEBEMAIL environment variables before building (e.g. export DEBFULLNAME=\"Your Name\"; export DEBEMAIL=\"you@example.com\")"
  exit 1
fi

# Build Debian binary package using dpkg-buildpackage
# This will invoke debian/rules which uses debhelper (dh) + cmake buildsystem.
dpkg-buildpackage -us -uc -b