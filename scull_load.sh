#!/bin/bash

if [ "$1" != "" ];then			#if [ -n "$1" ] is also ok; if [ -n $1 ] is not ok
	module=$1
else
	module="Scull"
fi

if [ "$2" != "" ];then
	moduleNum=$2
else
	moduleNum=1
fi

if [ $moduleNum -gt 2 ];then
	exit 1
fi

rm -f /dev/$module[0-$moduleNum]

insmod $module.ko $3 $4 || exit 1

major=$(awk "\$2 == \"$module\" {print \$1}" /proc/devices)

for ((i=0; i<${moduleNum}; i++));do		#i<$(moduleNum) is not ok
	mknod /dev/${module}$i c $major $i
done
#mknod /dev/${module}_1 c $major 1
