#! /usr/bin/python

import zmq
import uuid
import struct
import subprocess
from superfasthash import SuperFastHash
from time import sleep
import unittest

"""
Test all the protocol configs, one by one.
Victor Perron <victor.perron@locarise.com>
"""

device_id = "8e9bf550554211e282fa1803731606fa"
pub_endpoint = "tcp://localhost:10080"
pull_endpoint = "tcp://localhost:10081"

def hash_msg(msg):
    _sum = 0
    for part in msg:
        _sum = SuperFastHash(part, _sum)
    return struct.pack('I', _sum)

def send_msg(socket,msg):
    _sum = hash_msg(msg)
    msg.append(_sum)
    socket.send_multipart(msg)
    sleep(1)

def gen_uuid():
    return uuid.uuid1().hex


class TestProtocol(unittest.TestCase):

    def setUp(self):
        subprocess.Popen(['../src/satan','-s',pub_endpoint,'-r',pull_endpoint,'-u',device_id])
        context = zmq.Context()
        self.pub_socket = context.socket(zmq.PUB)
        self.pub_socket.bind ("tcp://*:10080")
        self.pull_socket = context.socket(zmq.PULL)
        self.pull_socket.bind ("tcp://*:10081")
        sleep(1) # Stabilize

    def test_checksum(self):
        msgid = gen_uuid()
        send_msg(self.pub_socket, [device_id, msgid, 'STATUS'])
        ans = self.pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGACCEPTED')

    def test_status(self):
        msgid = gen_uuid()
        send_msg(self.pub_socket, [device_id, msgid, 'STATUS'])
        ans = self.pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGACCEPTED')



if __name__ == '__main__':
    unittest.main()

