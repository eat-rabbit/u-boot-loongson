#!/bin/bash

#set -x

abi1_toolchain_list=("/opt/loongson-gnu-toolchain-x86_64-loongarch64-linux-gnu" \
					"/opt/loongson-gnu-toolchain-8.3-x86_64-loongarch64-linux-gnu-rc1.4" \
					"/opt/loongson-gnu-toolchain-8.3-x86_64-loongarch64-linux-gnu-rc1.3-1" \
					"/opt/loongson-gnu-toolchain-8.3-x86_64-loongarch64-linux-gnu-rc1.2" \
					"/opt/toolchain-loongarch64-linux-gnu-gcc8-host-x86_64-2022-07-18")

abi2_toolchain_list=("/opt/loongarch64-linux-gnu-gcc13.3" \
					"/opt/loongarch64-linux-gnu-gcc14.2")

real_target=0
real_toolchain=""

function setup_loongarch_env()
{
	echo "====>setup env for LoongArch..."
	CC_PREFIX=$real_toolchain
	export PATH=$PATH:$CC_PREFIX/bin
	export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CC_PREFIX/lib
	export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CC_PREFIX/loongarch64-linux-gnu/lib64

	export ARCH=loongarch
	export CROSS_COMPILE=loongarch64-linux-gnu-
}

for i in ${abi1_toolchain_list[*]}
do
	real_toolchain=$i
	if [ -d $real_toolchain ]; then
		real_target=1
		break;
	fi
done

if [ $# -eq 1 ] ; then
	if [[ "$1" == "abi2" ]]; then
		real_target=0
		real_toolchain=""
		for i in ${abi2_toolchain_list[*]}
		do
			real_toolchain=$i
			if [ -d $real_toolchain ]; then
				real_target=1
				break;
			fi
		done
	fi
fi

if [ $real_target -eq 0 ]; then
	echo "没有找到相关工具链的文件夹"
fi
echo "当前声明的工具链为:"
echo $real_toolchain

setup_loongarch_env

#set +x
