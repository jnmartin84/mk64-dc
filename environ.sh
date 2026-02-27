export KOS_ARCH="dreamcast"
if [ -z "${KOS_SUBARCH}" ] ; then
    export KOS_SUBARCH="pristine"
else
    export KOS_SUBARCH
fi
export KOS_BASE="/opt/toolchains/dc/kos"
export KOS_PORTS="${KOS_BASE}/../kos-ports"
export KOS_CC_BASE="/opt/toolchains/dc/sh-elf"
export KOS_CC_PREFIX="sh-elf"
export DC_ARM_BASE="/opt/toolchains/dc/arm-eabi"
export DC_ARM_PREFIX="arm-eabi"
export DC_TOOLS_BASE="/opt/toolchains/dc/bin"
export KOS_CMAKE_TOOLCHAIN="${KOS_BASE}/utils/cmake/kallistios.toolchain.cmake"
export KOS_GENROMFS="${KOS_BASE}/utils/genromfs/genromfs"
export KOS_MAKE="make"
export KOS_LOADER="dc-tool -x"
export KOS_INC_PATHS=""
export KOS_CFLAGS=""
export KOS_CPPFLAGS=""
export KOS_LDFLAGS=""
export KOS_AFLAGS=""
export DC_ARM_LDFLAGS=""
export KOS_CFLAGS="${KOS_CFLAGS} -O3 -flto=auto -ffat-lto-objects"
export KOS_CFLAGS="${KOS_CFLAGS} -fomit-frame-pointer"
export KOS_CFLAGS="${KOS_CFLAGS} -fbuiltin -ffast-math -ffp-contract=fast -mfsrra -mfsca"
export KOS_SH4_PRECISION="-m4-single-only"
. ${KOS_BASE}/environ_base.sh
