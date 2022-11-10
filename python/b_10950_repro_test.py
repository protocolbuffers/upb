"""Test to reproduce https://github.com/protocolbuffers/protobuf/issues/10950"""

import unittest
import b_10950_repro_pb2

class B10950ReproTest(unittest.TestCase):

  def test_b10950(self):
    person = b_10950_repro_pb2.Person()
    person.TaskType = 32
    person.Name = "Name1"
    self.assertEqual(32, person.TaskType)
    serialized = person.SerializeToString()
    person2 = b_10950_repro_pb2.Person()
    person2.ParseFromString(serialized)
    self.assertEqual(32, person2.TaskType)


if __name__ == '__main__':
  unittest.main()
