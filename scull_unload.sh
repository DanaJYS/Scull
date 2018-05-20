#!/bin/bash

if [ "$1" != "" ];then
	module=$1
else
	module=Scull
fi

if [ "$2" != "" ];then
	moduleNum=$2
else
	moduleNum=1
fi

for((i=0; i<${moduleNum}; i++));do
	rm -f /dev/${module}$i
done

rmmod $module.ko || exit 1
