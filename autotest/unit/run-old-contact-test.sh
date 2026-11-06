#!/bin/sh
# Builds and runs the old-file test (OF-001): a contact file written by an older version must keep its meaning.
#
#   autotest/unit/run-old-contact-test.sh <build-folder>
#
# Built like the unit suite (run-messages-tests.sh): against the application's own objects, without its main, and run
# headless in an empty home folder. The two old contact files are copied into a work folder, because the test writes its
# own copy there.

set -eu

here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)
build=${1:?usage: run-old-contact-test.sh <build-folder>}
build=$(cd "$build" && pwd)

TZ=Europe/London
export TZ

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

moc="$(qmake6 -query QT_INSTALL_LIBEXECS)/moc"
qtcflags=$(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets Qt6Network Qt6Xml Qt6Multimedia Qt6Test Qt6Core5Compat)
qtlibs=$(pkg-config --libs Qt6Core Qt6Gui Qt6Widgets Qt6Network Qt6Xml Qt6Multimedia Qt6Test Qt6Core5Compat)

g++ -std=gnu++17 -fPIC -DQT_NO_DEBUG -I"$repo" -I"$repo/../Lib" -I"$build" $qtcflags \
    -c "$here/oldfile/old_contact_test.cpp" -o "$work/old_contact_test.o"

objects=""
for o in "$build"/*.o; do
    case "$(basename "$o")" in
        main.o) ;;
        *) objects="$objects $o" ;;
    esac
done

g++ -o "$work/old_contact_test" "$work/old_contact_test.o" $objects $qtlibs -lpthread -lGLX -lOpenGL

mkdir -p "$work/home" "$work/data"
cp "$here/oldfile/old-contact.contact" "$here/oldfile/old-other.contact" "$work/data/"
cd "$work"
env HOME="$work/home" QT_QPA_PLATFORM=offscreen CM_TEST_SECRET="${CM_TEST_SECRET:-announcement-bench-only}" \
    "$work/old_contact_test" "$work/data"
