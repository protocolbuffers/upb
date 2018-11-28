#!/bin/bash

cd $(dirname $0)/../..
bazel build :conformance_proto_upb
# bazel test :all
ls -R
