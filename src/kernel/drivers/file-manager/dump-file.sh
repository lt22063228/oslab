#!/bin/bash
xxd -c 12 -s 0 -ps $1 | xxd -r -ps | xxd -c 12 -i >> a.txt
