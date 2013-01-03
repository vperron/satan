#! /usr/bin/python

import zmq
import uuid
import struct

from superfasthash import SuperFastHash

from time import sleep

context = zmq.Context()
socket = context.socket(zmq.PUB)
socket.bind ("tcp://*:10080")

device_id = '8e9bf550554211e282fa1803731606fa'
msg_id = uuid.uuid1().hex
command = "STATUS"

total = device_id + msg_id + command

chksum = SuperFastHash(device_id, 0)
chksum = SuperFastHash(msg_id, chksum)
chksum = SuperFastHash(command, chksum)
chksum_str = struct.pack('I', chksum)

msg0 = [device_id, msg_id, command, chksum_str]
print msg0

sleep(1)
socket.send_multipart(msg0)
sleep(1)

