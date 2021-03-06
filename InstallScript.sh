#!/bin/bash

export WORKDIR=$PWD

if [[ "$TRAVIS_OS_NAME" == "linux" ]]; then
export SHAREDLIBEXT=".so"
else
export SHAREDLIBEXT=".dylib"
fi

if [[ "$TRAVIS_OS_NAME" == "linux" ]]; then
#tcl tk install region
alias make="make -s"
wget --no-check-certificate http://prdownloads.sourceforge.net/tcl/tcl8.6.6-src.tar.gz
tar -xzf tcl8.6.6-src.tar.gz
cd tcl8.6.6/unix
./configure --prefix=$INSTALLDIR/tcl
make install
export PATH="$INSTALLDIR/tcl/bin:$PATH"
wget --no-check-certificate http://prdownloads.sourceforge.net/tcl/tk8.6.6-src.tar.gz
tar -xzf tk8.6.6-src.tar.gz
cd tk8.6.6/unix
./configure --prefix=$INSTALLDIR/tcl
make install
#tcl tk install region
unalias make
fi
cd $WORKDIR/tce

if [[ "$TRAVIS_OS_NAME" == "osx" ]]; then
sleep 30m;killall make;exit 1 &
sleep 30m;killall cmake; exit 1 &
fi

./tools/scripts/install_llvm_$LLVM_VER_BUILD.sh $INSTALLDIR/llvm
export PATH="$HOME:$INSTALLDIR/llvm/bin:$PATH"
ln -s $INSTALLDIR/llvm/lib/libLLVM$SHAREDLIBEXT $INSTALLDIR/llvm/lib/libLLVM-$LLVM_VER_BUILD$SHAREDLIBEXT


ln -s $(which ccache) $HOME/clang && ln -s $(which ccache) $HOME/clang++ && export PATH=$HOME:$PATH;
export CC='clang -Qunused-arguments -fcolor-diagnostics'
export CXX='clang++ -Qunused-arguments -fcolor-diagnostics' 

./gen_llvm_shared_lib.sh
./autogen.sh

if [[ "$TRAVIS_OS_NAME" == "linux" ]]; then

./configure --prefix=$INSTALLDIR --with-tcl=$INSTALLDIR/tcl
else
./configure --prefix=$INSTALLDIR
fi


make
make install

wait
