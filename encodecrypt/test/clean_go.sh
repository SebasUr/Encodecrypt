#!/bin/sh
unzip -o ./go-master.zip
find go-master -type f -size -1200c -print -delete
