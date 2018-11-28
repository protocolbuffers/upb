#!/bin/bash

cd $(dirname $0)/../..
bazel test :all
ls -R
