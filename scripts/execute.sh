#!/bin/bash

cd ..
cmake .
make

in="in.fa"
out="out.fa"

./cryfa -k pass.txt $in > ENCRYPTED
./cryfa -dk pass.txt ENCRYPTED > $out

cmp $in $out

cd scripts