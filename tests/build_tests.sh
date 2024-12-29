#!/bin/sh

compiler="g++"
compile_flags="-std=c++2b -I../source/ -O3 -lssl -lcrypto"
torr_path="../libtorr.a"

if [ ! -f $torr_path ]; then
    echo "${torr_path} not found, please compile torr first"
    exit
fi

find "./" -type f -name "*.cpp" -print0 | while read -d $'\0' file
do
    compile="${compiler} ${compile_flags} ${file} ${torr_path}"
    echo "compiling and running ${file}"
    eval " $compile"
    ./a.out
done
