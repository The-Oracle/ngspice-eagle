#!/bin/bash
# MacOS build script for NGSPICE

# Considering a MacOS 10.14 system, there are some prerequisites to be satisfied:
# 1) Install an X11 system of your choice. XQuartz from SourceForge is fine: https://www.xquartz.org
# 2) Install automake, autoconf, libtool and an updated version of bison by using the method you prefer.
#    From sources, from 'brew' or from 'MacPorts' are the known ones and I prefer using MacPorts,
#    available at this address: https://www.macports.org .
#    You can install from a tarball or you can use the installer, which will also configure the PATH.
#
# Said that, the script is quite linear and simple.


# Build the ngspice flat installation mac OS package

export PATH=/usr/bin:/usr/sbin:/bin:/sbin:${PATH}

make clean
# Autoconf configuration
./autogen.sh

# Check if destination directory exists, if not create it and copy assets to the directory.
export DESTDIR="./root-tree/Applications/ngspice"
if [ ! -e ${DESTDIR} ]; then
  mkdir -p /root-tree/Applications/ngspice
  # Add manual, release notes and source package
  cp Icon* ReleaseNotes.txt ngspice-31-manual.pdf ngspice-31.tar.gz ./root-tree/Applications/ngspice/
fi

# Run Automake configuration
./configure --enable-xspice --enable-cider --enable-pss --disable-debug --exec-prefix=/Users/diradmin/Downloads/ngspice-31/root-tree/Applications/ngspice --prefix=/Users/diradmin/Downloads/ngspice-31/root-tree/Applications/ngspice
# Build ngspice
make -j8
# Install ngspice
make install
# Build and Sign Flat Package
pkgbuild --root $(pwd)/root-tree --identifier net.sourceforge.ngspice --scripts "$(pwd)/scripts" --install-location / --sign 6468B63FF38F78C3159271B7449E040EDA76171F ngspice.pkg

exit 0
