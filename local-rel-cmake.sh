#! /bin/sh

if [ -f CMakeLists.txt ]; then
    echo "This script should be run in a build sub-directory"
    exit
fi

CORES=$(grep -c ^processor /proc/cpuinfo)

VERBOSE=0

CONF_OPTS="-DWITH_LTO=ON -DCMAKE_UNITY_BUILD=ON"

BUILD=0
BUILD_OPTS="--parallel=$CORES"

INSTALL=0
#INSTALL_OPTS=""
SUDO=""

EXEC=0
#EXEC_ARGS=""

while getopts 'hvbcisxC:B:' OPT; do
    case "$OPT" in
        h)
            echo "Usage $(basename "$0") OPTS"
            echo "where: OPTS are"
            echo "  -h    - this help"
            echo "  -v    - verbose"
            echo "  -b    - also build"
            echo "  -c    - only with '-b': clean tree before build"
            echo "  -i    - also install"
            echo "  -s    - only with '-i': do sudo install"
            echo "  -x    - execute from build tree"
            echo "  -C COPT"
            echo "        - add 'COPT' to cmake configure options"
            echo "  -B BOPT"
            echo "        - add 'BOPT' to cmake build options"
            # echo "  -I IOPT"
            # echo "        - add 'IOPT' to cmake install options"
            # echo "  -A AARG"
            # echo "        - add 'AARG' to execute arguments"
            ;;
        v)
            VERBOSE=1
            ;;
        b)
            BUILD=1
            ;;
        c)
            BUILD_OPTS="$BUILD_OPTS --clean-before"
            ;;
        i)
            INSTALL=1
            ;;
        s)
            SUDO="sudo"
            ;;

        x)
            EXEC=1
            ;;
        C)
            CONF_OPTS="$CONF_OPTS $OPTARG"
            ;;
        B)
            BUILD_OPTS="$BUILD_OPTS $OPTARG"
            ;;
        # I)
        #     INSTALL_OPTS="$INSTALL_OPTS $OPTARG"
        #     ;;
        # A)
        #     EXEC_ARGS="$EXEC_ARGS $OPTARG"
        #     ;;
        *)
            echo "Unknown option: '$OPT'"
            exit 1
    esac
done

if [ $VERBOSE -ne 0 ] ; then
    set -x
fi
   
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr/local \
      "$CONF_OPTS" .. || exit

if [ $BUILD -ne 0 ] ; then
    cmake --build . "$BUILD_OPTS" || exit
fi

if [ $INSTALL -ne 0 ] ; then
    "$SUDO" cmake --install . || exit
fi

if [ $EXEC -ne 0 ] ; then
    # run in build directory
    ./filters
fi