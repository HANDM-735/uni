#!/bin/sh
TXT=$(cat config/version.txt)
VER_NUM=${TXT:4}
VER_STR=V${VER_NUM}

echo "<build>start building the application......"
rm -rf release
mkdir release
mkdir release/bin
mkdir release/log
mkdir release/ota
chmod 777 release

cd uni-dc
make rebuild PLATFORM=PC
mv ./uni-dc ../release/bin
cd ../
cp -f config/* release/bin
cp -f ota/otafile/* release/ota

cd release
tar -zcvf uni-${VER_STR}.tar.gz bin log ota



