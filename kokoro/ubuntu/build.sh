#!/bin/bash

cd $(dirname $0)/../..
bazel build :conformance_proto_upb_srcs.cc
# bazel test :all
ls -R
