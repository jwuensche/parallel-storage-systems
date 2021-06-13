#!/bin/sh

ex() {
    echo "$@"
    echo "$@" | xargs | sh
}

set -e

echo "#### File Tests"

ex "touch mnt/foo"

ex "echo bar > mnt/foo"

ex "test \"bar\" = $(cat mnt/foo)"

ex "mv mnt/foo mnt/fob"

ex "mkdir -p mnt/meh/bar"
ex "touch mnt/meh/bar/baz"
ex "echo here > mnt/meh/bar/baz"
ex "mv mnt/meh mnt/baz"
ex "test \"here\" = $(cat mnt/baz/bar/baz)"

ex "test \"bar\" = $(cat mnt/fob)"

ex "test ! -e mnt/foo"

echo "#### Directory Tests"

ex "mkdir mnt/dir"

ex "touch mnt/dir/foo"

ex "ls mnt/dir"

ex "echo foobarbaz > mnt/dir/foo"

ex "cat mnt/dir/foo"

echo "#### File Size Tests"

ex "dd if=/dev/urandom of=./mnt/here bs=1k count=1024"

ex "truncate --size 5120 mnt/here"

ex "truncate --size 50120 mnt/here"

ex "cat mnt/here > /dev/null"
