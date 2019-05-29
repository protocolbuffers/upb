#!/usr/bin/env python3

from os import listdir
from os.path import isfile, join
import subprocess

path = "../JSONTestSuite/test_parsing"
files = [f for f in listdir(path) if isfile(join(path, f))]

good = 0
bad = 0

for filename in files:
  should_succeed = filename.startswith("y_")
  should_fail = filename.startswith("n_")
  filename = join(path, filename)
  args = ["bazel-bin/test_json_generic", filename]
  proc = subprocess.Popen(args, stdout=subprocess.PIPE)
  out, err = proc.communicate()
  return_code = proc.returncode

  if return_code == 0 and should_fail:
    print("Should have failed but succeeded:")
    print(" ".join(args))
    print(out.decode("utf-8"))
    bad += 1
  elif return_code != 0 and should_succeed:
    print("Should have succeeded but failed:")
    print(" ".join(args))
    print(out.decode("utf-8"))
    bad += 1
  else:
    good += 1

print("Good: %s, Bad: %d" % (good, bad))
