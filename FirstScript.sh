#!/bin/bash
export INSTALLDIR="$PWD/tceinstallloc"
echo $TRAVIS_OS_NAME
if [[ "$TRAVIS_OS_NAME" == "linux" ]]; then 
wget --no-check-certificate http://cmake.org/files/v3.4/cmake-3.4.3-Linux-x86_64.tar.gz
tar -xzf cmake-3.4.3-Linux-x86_64.tar.gz
export PATH=$PWD/cmake-3.4.3-Linux-x86_64/bin:$PATH
rm cmake-3.4.3-Linux-x86_64.tar.gz
fi
if [ "$LLVM_VER_BUILD" == "3.8" ] || [ "$LLVM_VER_BUILD" == "3.9" ]; then
sed -e "s/cmake -G \"Unix Makefiles\"/cmake -G \"Ninja\"/g" tce/tools/scripts/install_llvm_$LLVM_VER_BUILD.sh > tce/tools/scripts/install_llvm_$LLVM_VER_BUILD.sh.tmp && mv tce/tools/scripts/install_llvm_$LLVM_VER_BUILD.sh.tmp tce/tools/scripts/install_llvm_$LLVM_VER_BUILD.sh
sed -e "s/make -j4 CXXFLAGS=\"-std=c++11\" REQUIRES_RTTI=1/CXXFLAGS=\"-std=c++11\" REQUIRES_RTTI=1 ninja/g" tce/tools/scripts/install_llvm_$LLVM_VER_BUILD.sh > tce/tools/scripts/install_llvm_$LLVM_VER_BUILD.sh.tmp && mv tce/tools/scripts/install_llvm_$LLVM_VER_BUILD.sh.tmp tce/tools/scripts/install_llvm_$LLVM_VER_BUILD.sh
sed -e "s/make install/ninja install/g" tce/tools/scripts/install_llvm_$LLVM_VER_BUILD.sh > tce/tools/scripts/install_llvm_$LLVM_VER_BUILD.sh.tmp && mv tce/tools/scripts/install_llvm_$LLVM_VER_BUILD.sh.tmp tce/tools/scripts/install_llvm_$LLVM_VER_BUILD.sh
chmod +x tce/tools/scripts/install_llvm_$LLVM_VER_BUILD.sh
fi
  # Implement a platform-independent timeout function.
  function timeout() { perl -e 'alarm shift; exec @ARGV' "$@"; }
TIMEOUT=2400
#- if [[ "$TRAVIS_OS_NAME" == "linux" ]]; then wget --no-check-certificate http://llvm.org/releases/3.4.2/clang+llvm-3.4.2-x86_64-unknown-ubuntu12.04.xz ;fi
#- if [[ "$TRAVIS_OS_NAME" == "linux" ]]; then xz -d -c clang+llvm-3.4.2-x86_64-unknown-ubuntu12.04.xz | tar -x ;fi 
#- if [[ "$TRAVIS_OS_NAME" == "linux" ]]; then export PATH=$PWD/clang+llvm-3.4.2-x86_64-unknown-ubuntu12.04/bin:$PATH ;fi
#- if [[ "$TRAVIS_OS_NAME" == "linux" ]]; then rm clang+llvm-3.4.2-x86_64-unknown-ubuntu12.04.xz ;fi

if [[ "$TRAVIS_OS_NAME" == "osx" ]]; then 
brew update;
brew install ccache;
brew install boost;
brew install xerces-c;
brew install llvm;
brew install ninja;
export PATH=/usr/local/opt/llvm/bin:$PATH
fi

if [ "$CC"  == "clang" ]; then 
ln -s $(which ccache) $HOME/clang && ln -s $(which ccache) $HOME/clang++ && export PATH=$HOME:$PATH;
export CC='clang -Qunused-arguments -fcolor-diagnostics'
export CXX='clang++ -Qunused-arguments -fcolor-diagnostics' 
fi

if [ "$CC"  == "gcc" ]; then 
export CC='gcc-4.8'; export CXX='g++-4.8'
fi
ccache -z -M 500M
#patch configure.ac
sed -i 's/LLVM_SHARED_LIB_FILE=\/usr\/lib\/x86_64-linux-gnu\/libLLVM-$LLVM_VERSION$LIBRARY_SUFFIX/"$INSTALLDIR"\/llvm\/lib\/libLLVM$LIBRARY_SUFFIX/g' tce/configure.ac

timeout $TIMEOUT ./InstallScript.sh
RESULT=$?; if [ $RESULT -eq 0 ] || [ $RESULT -eq 142 ]; then true; else false; fi;



