#! /usr/bin/python

import zmq
import sys
import uuid
import struct
from superfasthash import SuperFastHash
from time import sleep

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

context = zmq.Context()
socket = context.socket(zmq.PUB)
socket.bind ("tcp://*:10080")
sleep(1)

msgid = gen_uuid()
send_msg(socket, [sys.argv[1], msgid, sys.argv[2], sys.argv[3]])

