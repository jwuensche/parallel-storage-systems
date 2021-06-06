#!/bin/sh

set -e

echo "#### File Tests"

echo "touch mnt/foo"
touch mnt/foo

echo "echo bar > mnt/foo"
echo bar > mnt/foo

echo "cat mnt/foo"
cat mnt/foo

echo "#### Directory Tests"

echo "mkdir mnt/dir"
mkdir mnt/dir

echo "touch mnt/dir/foo"
touch mnt/dir/foo

echo "ls mnt/dir"
ls mnt/dir

echo "echo foobarbaz > mnt/dir/foo"
echo foobarbaz > mnt/dir/foo

echo "cat mnt/dir/foo"
cat mnt/dir/foo
