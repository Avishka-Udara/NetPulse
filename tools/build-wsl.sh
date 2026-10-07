#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
test -f vendor/sqlite3.c || { echo 'Fetch SQLite first using tools/fetch-sqlite.ps1'; exit 1; }
cc=x86_64-w64-mingw32-gcc
flags='-Os -std=c11 -Wall -Wextra -Wno-misleading-indentation -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 -Isrc -Ivendor'
$cc -Os -w -c -DSQLITE_THREADSAFE=0 -DSQLITE_DEFAULT_MEMSTATUS=0 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_DQS=0 vendor/sqlite3.c -o build/sqlite3.o
x86_64-w64-mingw32-windres -I src src/netpulse.rc -O coff -o build/netpulse.res
$cc $flags -municode -mwindows -static src/main.c src/core.c src/network.c src/db.c src/settings.c src/ui.c src/taskbar.c src/startup.c build/sqlite3.o build/netpulse.res -o build/NetPulse.exe -s -luser32 -lgdi32 -lshell32 -ladvapi32 -liphlpapi -lcomdlg32 -ldwmapi -lcomctl32 -lole32 -loleaut32 -luuid
$cc $flags -static tests/test_core.c src/core.c -o build/test_core.exe
$cc $flags -static tests/test_db.c src/db.c build/sqlite3.o -o build/test_db.exe
$cc $flags -static tests/test_network.c src/network.c src/core.c -o build/test_network.exe -liphlpapi
$cc $flags -static tests/test_placement.c -o build/test_placement.exe
$cc $flags -static tests/test_settings.c src/settings.c src/core.c -o build/test_settings.exe -ladvapi32
$cc $flags -static tests/test_startup.c src/startup.c -o build/test_startup.exe -ladvapi32
cp config.ini build/config.ini
echo 'Built Windows executables. Run build/test_core.exe, build/test_db.exe, and build/test_network.exe on Windows.'
