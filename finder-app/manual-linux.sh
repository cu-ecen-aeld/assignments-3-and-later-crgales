#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE_DIR=/opt/arm-gnu-toolchain/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu
CROSS_COMPILE=$CROSS_COMPILE_DIR/bin/aarch64-none-linux-gnu-
CROSS_COMPILE_ARCH=aarch64-none-linux-gnu

echo "Cross compiler is at $CROSS_COMPILE_DIR"

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

pushd $OUTDIR

if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    make CROSS_COMPILE=$CROSS_COMPILE ARCH=$ARCH defconfig && make CROSS_COMPILE=$CROSS_COMPILE ARCH=$ARCH -j40 Image modules dtbs
    cd $OUTDIR
fi

echo "Adding the Image in outdir"
cp ./linux-stable/arch/$ARCH/boot/Image .

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

mkdir rootfs && cd rootfs
mkdir bin sbin lib lib64 etc dev proc sys tmp var root home

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
else
    cd busybox
fi

# TODO: Make and install busybox
make CROSS_COMPILE=$CROSS_COMPILE defconfig && make CROSS_COMPILE=$CROSS_COMPILE CONFIG_PREFIX=$OUTDIR/rootfs install

cd ${OUTDIR}/rootfs

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
cp $CROSS_COMPILE_DIR/$CROSS_COMPILE_ARCH/libc/lib/* lib
cp $CROSS_COMPILE_DIR/$CROSS_COMPILE_ARCH/libc/lib64/* lib64

# TODO: Make device nodes
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 666 dev/console c 5 1

# TODO: Clean and build the writer utility
make CROSS_COMPILE=$CROSS_COMPILE -C ${FINDER_APP_DIR} all

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp -r ${FINDER_APP_DIR}/* home/

# TODO: Chown the root directory


# TODO: Create initramfs.cpio.gz
find . | cpio -ov -H newc --owner root:root | gzip > ../initramfs.cpio.gz

#qemu-system-aarch64 -M virt -cpu cortex-a53 -nographic -smp 1 -kernel /tmp/aeld/linux-stable/arch/arm64/boot/Image

