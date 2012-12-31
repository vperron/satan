#! /usr/bin/python

import zmq
from time import sleep

context = zmq.Context()
socket = context.socket(zmq.REP)
socket.bind ("tcp://*:10081")
s = socket.recv()
print s

sleep(1)

