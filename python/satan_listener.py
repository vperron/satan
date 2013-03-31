#!/usr/bin/env python

import zmq
from time import sleep

context = zmq.Context()
socket = context.socket(zmq.PULL)
socket.bind ("tcp://*:10081")
while True:
    s = socket.recv_multipart()
    print s

sleep(1)

