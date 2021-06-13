#!/usr/bin/env bash
set -e

for i in *.md
do
    name=$(echo $i | cut -d '.' -f 1)
    pandoc $i -o $name.pdf
done
