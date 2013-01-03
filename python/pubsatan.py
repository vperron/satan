#! /usr/bin/python

import zmq
import uuid
import struct

from time import sleep

import binascii
import sys

def get16bits(data):
    """Returns the first 16bits of a string"""
    return int(binascii.hexlify(data[1::-1]), 16)

def superFastHash(data, hash):
    length = len(data)
    if length == 0:
        return 0

    rem = length & 3
    length >>= 2

    while length > 0:
        hash += get16bits(data) & 0xFFFFFFFF
        tmp = (get16bits(data[2:])<< 11) ^ hash
        hash = ((hash << 16) & 0xFFFFFFFF) ^ tmp
        data = data[4:]
        hash += hash >> 11
        hash = hash & 0xFFFFFFFF
        length -= 1

    if rem == 3:
        hash += get16bits (data)
        hash ^= (hash << 16) & 0xFFFFFFFF
        hash ^= (int(binascii.hexlify(data[2]), 16) << 18) & 0xFFFFFFFF
        hash += hash >> 11
    elif rem == 2:
        hash += get16bits (data)
        hash ^= (hash << 11) & 0xFFFFFFFF
        hash += hash >> 17
    elif rem == 1:
        hash += int(binascii.hexlify(data[0]), 16)
        hash ^= (hash << 10) & 0xFFFFFFFF
        hash += hash >> 1

    hash = hash & 0xFFFFFFFF
    hash ^= (hash << 3) & 0xFFFFFFFF
    hash += hash >> 5
    hash = hash & 0xFFFFFFFF
    hash ^= (hash << 4) & 0xFFFFFFFF
    hash += hash >> 17
    hash = hash & 0xFFFFFFFF
    hash ^= (hash << 25) & 0xFFFFFFFF
    hash += hash >> 6
    hash = hash & 0xFFFFFFFF

    return hash

context = zmq.Context()
socket = context.socket(zmq.PUB)
socket.bind ("tcp://*:10080")

device_id = '8e9bf550554211e282fa1803731606fa'
msg_id = uuid.uuid1().hex
command = "STATUS"

total = device_id + msg_id + command

chksum = superFastHash(device_id, 0)
chksum = superFastHash(msg_id, chksum)
chksum = superFastHash(command, chksum)
chksum_str = struct.pack('I', chksum)

msg0 = [device_id, msg_id, command, chksum_str]
print msg0

sleep(1)
socket.send_multipart(msg0)
sleep(1)

