#!/bin/bash

set -e

if [ ! -d $(pwd)/build ];then
    mkdir  $(pwd)/build
fi
rm -rf $(pwd)/build/*
cd $(pwd)/build && cmake ..
make 

#回到项目根目录
cd ..

#将头文件拷贝到/usr/include/Kenmuduo so库拷贝到/usr/lib
if [ ! -d /usr/include/Kenmuduo ]; then
    mkdir /usr/include/Kenmuduo
fi

for header in $(ls *.h)
do
    cp $header /usr/include/Kenmuduo
done

cp $(pwd)/lib/libKenmuduo.so /usr/lib/

#有新的链接库的时候，需要使用下面命令刷新下动态库缓存
ldconfig
