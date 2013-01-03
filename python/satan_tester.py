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
with open("/dev/urandom") as f:
    binarydata = f.readline()
    for i in xrange(100):
        binarydata += f.readline()

print "\nUsing %d bytes of random binary data.\n" % len(binarydata)

print "################################################################################"
print "#"
print "#   Please run the following process BEFORE tests :"
print "#"
print "#   ../src/satan -s "+pub_endpoint+" -r "+pull_endpoint+" -u "+device_id
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
    return uuid.uuid1().hex


class TestProtocol(unittest.TestCase):

    def test_status(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, 'STATUS'])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGACCEPTED')

    def test_uciline_0(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "UCILINE"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGPARSEERROR')
    def test_uciline_1(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "UCILINE", "uciline"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGACCEPTED')
    def test_uciline_2(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "UCILINE", "uciline", "stupid"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGPARSEERROR')

    def test_checksum(self):
        msgid = gen_uuid()
        msg = [device_id, msgid, 'STATUS']
        crc = hash_msg(msg)
        msg[1] = device_id  # Change msgid->device_id
        msg.append(crc)
        pub_socket.send_multipart(msg)
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], device_id)
        self.assertEqual(ans[2], 'MSGBADCRC')

    def test_unreadable_0(self):
        send_msg(pub_socket, [device_id, "foo", 'STATUS'])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], '0'*32)
        self.assertEqual(ans[2], 'MSGUNREADABLE')

    def test_unreadable_1(self):
        send_msg(pub_socket, [device_id])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], '0'*32)
        self.assertEqual(ans[2], 'MSGUNREADABLE')

    def test_parseerror_0(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "goo"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGPARSEERROR')

    def test_urlfirm_0(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "URLFIRM"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGPARSEERROR')
    def test_urlfirm_1(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "URLFIRM", "http://truc"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGPARSEERROR')
    def test_urlfirm_2(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "URLFIRM", "http://truc", "stupid"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGPARSEERROR')
    def test_urlfirm_3(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "URLFIRM", "http://truc", "0xbe"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGACCEPTED')

    def test_urlpak_0(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "URLPAK"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGPARSEERROR')
    def test_urlpak_1(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "URLPAK", "http://truc"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGACCEPTED')
    def test_urlpak_2(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "URLPAK", "http://truc", "executable"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGACCEPTED')
    def test_urlpak_3(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "URLPAK", "http://truc", "exec1", "exec2", "exec3"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGACCEPTED')

    def test_urlfile_0(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "URLFILE"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGPARSEERROR')
    def test_urlfile_1(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "URLFILE", "http://truc"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGPARSEERROR')
    def test_urlfile_2(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "URLFILE", "http://truc", "destination"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGACCEPTED')
    def test_urlfile_3(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "URLFILE", "http://truc", "destination", "foo"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGPARSEERROR')

    def test_urlscript_0(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "URLSCRIPT"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGPARSEERROR')
    def test_urlscript_1(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "URLSCRIPT", "http://truc"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGACCEPTED')
    def test_urlscript_2(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "URLSCRIPT", "http://truc", "stupid"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGPARSEERROR')

    def test_binfirm_0(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "BINFIRM"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGPARSEERROR')
    def test_binfirm_1(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "BINFIRM", binarydata])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGPARSEERROR')
    def test_binfirm_2(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "BINFIRM", binarydata, "stupid"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGPARSEERROR')
    def test_binfirm_3(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "BINFIRM", binarydata, "0xbe"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGACCEPTED')

    def test_binpak_0(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "BINPAK"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGPARSEERROR')
    def test_binpak_1(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "BINPAK", binarydata])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGACCEPTED')
    def test_binpak_2(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "BINPAK", binarydata, "executable"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGACCEPTED')
    def test_binpak_3(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "BINPAK", binarydata, "exec1", "exec2", "exec3"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGACCEPTED')

    def test_binfile_0(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "BINFILE"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGPARSEERROR')
    def test_binfile_1(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "BINFILE", binarydata])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGPARSEERROR')
    def test_binfile_2(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "BINFILE", binarydata, "destination"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGACCEPTED')
    def test_binfile_3(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "BINFILE", binarydata, "destination", "foo"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGPARSEERROR')

    def test_binscript_0(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "BINSCRIPT"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGPARSEERROR')
    def test_binscript_1(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "BINSCRIPT", binarydata])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGACCEPTED')
    def test_binscript_2(self):
        msgid = gen_uuid()
        send_msg(pub_socket, [device_id, msgid, "BINSCRIPT", binarydata, "stupid"])
        ans = pull_socket.recv_multipart()
        self.assertEqual(ans[1], msgid)
        self.assertEqual(ans[2], 'MSGPARSEERROR')


if __name__ == '__main__':
    unittest.main()

