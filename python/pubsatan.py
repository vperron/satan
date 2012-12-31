#! /usr/bin/python

import zmq
from time import sleep

context = zmq.Context()
socket = context.socket(zmq.PUB)
socket.bind ("tcp://*:10080")

msg0 = ['foodevice1', 'msgid', 'command', 'checksum']

sleep(1)
socket.send_multipart(msg0)
sleep(1)

