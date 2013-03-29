#! /usr/bin/python

import zmq
import uuid
import struct
import os
import subprocess
from superfasthash import SuperFastHash
from time import sleep
import unittest

"""
Test all the protocol configs, one by one.
Victor Perron <victor@iso3103.net>

If you want to run a single test, run:

    python -m unittest satan_tester.TestProtocol.<test_method>

"""

device_id = "testtesttesttesttesttesttesttest"
pub_endpoint = "tcp://localhost:10080"
pull_endpoint = "tcp://localhost:10081"
with open("/dev/urandom") as f:
    binarydata = f.readline()
    for i in xrange(100):
        binarydata += f.readline()

print "\nUsing %d bytes of random binary data.\n" % len(binarydata)

print "################################################################################"
print "#"
print "#   Please run the following process BEFORE tests :"
print "#"
print "#   satan -s "+pub_endpoint+" -p "+pull_endpoint+" -u "+device_id
print "#"
print "################################################################################"
context = zmq.Context()
pub_socket = context.socket(zmq.PUB)
pub_socket.bind ("tcp://*:10080")
pull_socket = context.socket(zmq.PULL)
pull_socket.bind ("tcp://*:10081")
sleep(1) # Stabilize

def hash_msg(msg):
    _sum = 0
    for part in msg:
        _sum = SuperFastHash(part, _sum)
    return struct.pack('I', _sum)

def send_msg(socket,msg):
    _sum = hash_msg(msg)
    msg.append(_sum)
    socket.send_multipart(msg)

def gen_uuid():
    return uuid.uuid4().hex


class TestProtocol(unittest.TestCase):

    def test_push_0(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "PUSH"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGPARSEERROR')
    def test_push_1(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "PUSH", binarydata])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGACCEPTED')
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGCOMPLETED')
    def test_push_2(self):
        msgid = gen_uuid()
        os.remove("stupid")
        send_msg(pub_socket, [device_id, msgid, "PUSH", binarydata, "stupid"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGACCEPTED')
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGCOMPLETED')

    def test_exec_0(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "EXEC"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGPARSEERROR')
    def test_exec_1(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "EXEC", "stuff there and there"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGACCEPTED')
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGTASK')
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGCOMPLETED')
    def test_exec_2(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "EXEC", "testme", "stupid"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGPARSEERROR')
    def test_exec_3(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "EXEC", "echo machin"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGACCEPTED')
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGTASK')
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGCMDOUTPUT')
        self.assertEqual(ans[3], 'machin\n')
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGCOMPLETED')


if __name__ == '__main__':
    unittest.main()

