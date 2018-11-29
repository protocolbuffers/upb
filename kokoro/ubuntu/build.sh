#!/bin/bash

cd $(dirname $0)/../..
# bazel build :conformance_proto_upb_srcs.cc
bazel test :all
ls -R
ls bazel-bin/google/protobuf
ls bazel-out/k8-fastbuild/bin/google/protobuf/
ls -R bazel-bin
ls -R bazel-out/k8-fastbuild/bin
