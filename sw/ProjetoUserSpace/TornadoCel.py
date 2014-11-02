#!/usr/bin/env python
# -*- encoding: utf-8 -*-

import tornado.websocket
import json

handlers = set()
data_till_now = []

class TornadoRecv(tornado.websocket.WebSocketHandler):
    def open(self):
        """Gets the timestamp data and send to client."""
        handlers.add(self)
        print "Web socket opened"
        self.write_message(json.dumps(data_till_now))

    def process(self, data):
        #print "Sending data to client... %s"%data
        self.write_message(data)

    def on_message(self, message):
        #print "Message received %s"%message
        pass

    def on_close(self):
        print "Web socket closed"
        handlers.remove(self)

    def check_origin(self, origin):
        return True