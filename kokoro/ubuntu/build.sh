#!/bin/bash

cd $(dirname $0)/../..
# bazel build :conformance_proto_upb_srcs.cc
bazel test :all
ls bazel-bin/google/protobuf
ls bazel-out/k8-fastbuild/bin/google/protobuf/
