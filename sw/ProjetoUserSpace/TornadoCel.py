#!/usr/bin/env python
# -*- encoding: utf-8 -*-

import tornado.websocket

handlers = set()

class TornadoRecv(tornado.websocket.WebSocketHandler):
    def open(self):
        """Gets the timestamp data and send to client."""
        handlers.add(self)
        print "Web socket opened"

    def process(self, data):
        print "Sending data to client... %s"%data
        self.write_message(data)

    def on_message(self, message):
        print "Message received %s"%message

    def on_close(self):
        print "Web socket closed"
        handlers.remove(self)

    def check_origin(self, origin):
        return True
