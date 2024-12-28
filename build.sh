#!/bin/sh

echo -e "\033[32;1;4mBuilding libtorr.a\033[0m"
ninja

if [ "$1" != "release" ]
then
    echo -e "\033[32;1;4mBuilding and running tests\033[0m"
    cd ./tests/
    ninja
    ./a.out

    ninja -t clean
    cd ..
fi


if [ "$1" == "release" ]
then
    cp ./source ./include -r
    find "./include" -type f -name "*.cpp" -print0 | while read -d $'\0' file
    do
        rm $file
    done

    mkdir release
    mv libtorr.a release
    mv ./include/ ./release/
    echo -e "\033[32;1;4mRelease built\033[0m"
    echo -e "\033[34;1minclude/ and libtorr.a found in ./release/\033[0m"
fi

# TODO: single file header
#if [ "$1" == "release" ]
#then
#    find "./source" -type f -name "*.hpp" -print0 | while read -d $'\0' file
#    do
#        tail -n +2 "$file" >> torr.hpp
#    done

#    find "./source" -type f -name "*.hpp" -print0 | while read -d $'\0' file
#    do
#        file="${file//\//\\\/}"
#        file=${file:11}
#        echo $file
#        sed -i '/^#include <'$file'>/d' torr.hpp
#    done
    
#    sed -i '/^#include "/d' torr.hpp
#    sed '/^#include/d' torr.hpp > temp
#    mv temp torr.hpp
#    sed '/^#pragma/d' torr.hpp > temp
#    echo '#pragma once' | cat - temp > torr.hpp
#    rm temp
#fi

ninja -t clean
