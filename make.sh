#!/bin/sh

cd "$(dirname $0)"

ARCH_SUFFIX='-linux-gnu'

# ffmpeg with '-vf setsar=' to disable SAR
LOGO_FILE=logo.png
LOGO_BIN=logo-image.o
CONFIG_STATIC=lbml_static_config.toml
CONFIG_STATIC_BIN=lbml_static_config.o
BINS="$LOGO_BIN"
TARGET=linit
SOURCES="*.c"
OBJS="*.o"

if [ "_$1" = '_clean' ];then
	rm -rf $OBJS "$TARGET" "$LOGO_BIN"
	( cd musl && make clean )
	( cd zlib && make clean )
	( cd libfbpads && make clean )
	exit
fi

set -e
trap 'echo "[BUILD FAILURE]";' EXIT

if [ -z "$PWD" ]; then
	echo '[Error]: $PWD is empty'
	exit 1
fi

CC='gcc'
LD='ld'
CFLAGS="$CFLAGS -DLBMLVER=\"v1.0\" -I. -Ilibudev-zero -Isash -include ${PWD}/sash_defs.h"
LDFLAGS="$LDFLAGS -static -lm -lc"
# LDFLAGS='-lm
	# -pthread -ldl -lrt -Wl,--whole-archive -lpthread -Wl,--no-whole-archive
# '

ARCH=`uname -m`
MULTIARCH_NAME="${ARCH}${ARCH_SUFFIX}"

if [ ! -z "$1" ]; then
	ARCH="${1}"
	MULTIARCH_NAME="${ARCH}${ARCH_SUFFIX}"
	_P="${MULTIARCH_NAME}-"
	export CC="${_P}gcc"
	export LD="${_P}ld"
	export CHOST="${ARCH}"
	export AR="${_P}ar"
	export RANLIB="${_P}ranlib"
fi

export MUSL_ROOT="${PWD}/musl"

CFLAGS="${CFLAGS} -isystem ${PWD}/musl/include -isystem /usr/include -isystem /usr/include/${MULTIARCH_NAME}"
CCSPECS=''
# if musl exists, compile musl and static link using musl libc
if [ -d musl ]; then
	if [ ! -f musl/lib/libc.a ];then
		( cd musl && ./configure && make -j4 )
	fi
	MUSL_LIBPATH="${MUSL_ROOT}/lib"
	CFLAGS="$CFLAGS -I${PWD}/musl/obj/include -I${PWD}/musl/arch/generic -I${PWD}/musl/arch/$ARCH -I${PWD}/musl/include"
	LDFLAGS="$LDFLAGS -L${MUSL_LIBPATH}"
	CCSPECS="-specs ${PWD}/musl-gcc.specs"
fi

if [ -d zlib ]; then
	if [ ! -f zlib/libz.a ];then
		( export CFLAGS='-U_FORTIFY_SOURCE -fno-stack-protector -fno-mudflap'; cd zlib && ./configure && make -j4 )
	fi
	CFLAGS="$CFLAGS -I${PWD}/zlib"
	LDFLAGS="$LDFLAGS -L${PWD}/zlib"
fi
LDFLAGS="${LDFLAGS} -lz"

if [ -d libfbpads ]; then
	# if [ ! -f libfbpads/libfbpads.a ]; then
		( cd libfbpads && make -j4 CC="$CC" LD="$LD" CFLAGS="$CFLAGS" LDFLAGS="$LDFLAGS $CCSPECS" )
	# fi
	CFLAGS="$CFLAGS -DLBML_USE_EXTFB -Ilibfbpads"
	LDFLAGS="$LDFLAGS -Llibfbpads -lfbpads"
fi

if [ -f "$CONFIG_STATIC" ]; then
	echo "Using static config ${CONFIG_STATIC}"
	"$LD" -r -b binary -o "$CONFIG_STATIC_BIN" "$CONFIG_STATIC"
	CFLAGS="$CFLAGS -DLBML_CONFIG_STATIC"
	BINS="$BINS $CONFIG_STATIC_BIN"
else
	echo "Using dynamic config file /lbml.toml"
fi

SASH_SOURCES='sash/*.c'
LIBUDEV_SOURCES='libudev-zero/*.c'

"$LD" -r -b binary -o "$LOGO_BIN" "$LOGO_FILE"
"$CC" $CFLAGS $SOURCES $SASH_SOURCES $LIBUDEV_SOURCES $BINS $LDFLAGS -o "$TARGET" $CCSPECS


trap '' EXIT
echo '[BUILD SUCCESS]'
