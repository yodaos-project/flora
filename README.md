# flora

[![Build Status](https://travis-ci.com/yodaos-project/flora.svg?branch=master)](https://travis-ci.com/yodaos-project/flora)

作者: chen.zhang@rokid.com

版本: 1

更新时间: 2018.08.09 15:44

## 概述

跨进程/设备消息广播协议

## 编译

* 需要cmake 3.0以上

* 支持交叉编译

```
./config <参数>  配置编译参数，生成makefiles
cd ${makefiles生成目录}
make
make install
```

### 依赖模块

[mutils](https://github.com/Rokid/aife-mutils)  结构体序列化工具及log工具

[cmake-modules](https://github.com/Rokid/aife-cmake-modules)  cmake脚本功能模块

### 编译配置参数说明

* --build-dir=\*  指定cmake生成makefiles的目录

* --prefix=\*  指定安装目录

* --cmake-modules=\*  指定[cmake-modules](https://github.com/Rokid/aife-cmake-modules)仓库目录

* --find-root-path=\*  附加的动态库/静态库/头文件搜索路径

* --mutils=\*  指定[mutils](https://github.com/Rokid/aife-mutils)动态库/头文件搜索路径

* --toolchain=\*  交叉编译工具链安装目录

* --cross-prefix=\*  交叉编译命令前缀

### 编译命令示例

```
以a113平台交叉编译为例
假定工具链编译器路径为/home/codefarmer/a113/toolchain/gcc/linux-x86/aarch64/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-g++
mutils-caps库路径为/home/codefarmer/buildroot/a113/usr/lib/libcaps.so
mutils-rlog库路径为/home/codefarmer/buildroot/a113/usr/lib/librlog.so
mutils头文件路径为/home/codefarmer/buildroot/a113/usr/include/caps/caps.h
                  /home/codefarmer/buildroot/a113/usr/include/log/rlog.h
(注: 编译mutils时指定./config --prefix=/home/codefarmer/buildroot/a113/usr，make install后即为此种状态)
cmake-modules仓库路径为/home/codefarmer/cmake-modules

./config \
	--build-dir=a113-build \
	--cmake-modules=/home/codefarmer/cmake-modules \
	--toolchain=/home/codefarmer/a113/toolchain/gcc/linux-x86/aarch64/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu \
	--cross-prefix=aarch64-linux-gnu- \
	--find-root-path=/home/codefarmer/buildroot/a113 \
	--prefix=你喜欢的安装路径(如不指定，默认为/usr)

cd a113-build
make
make install
```

## 使用说明

[c++接口](./docs/cpp-api.md)

[c接口](./docs/c-api.md)
