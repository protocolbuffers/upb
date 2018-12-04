#!/bin/bash

cd $(dirname $0)/../..
bazel test :all
bazel test --test_output=all :cmake_build
bazel test --test_output=all :test_generated_files
