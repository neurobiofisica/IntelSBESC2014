import os, sys, time, json, random

import tornado.ioloop as ioloop
import tornado.web as web
import tornado.websocket as websocket
from tornado.escape import json_encode, json_decode

import logging
logging.basicConfig(level=logging.DEBUG)

file_path = os.path.dirname(os.path.realpath(__file__))
static_path = os.path.join(file_path, 'static')

data_till_now = []
class AcqWebSocket(websocket.WebSocketHandler):
    listeners = set([])
    def open(self):
        logging.log(logging.DEBUG, '<%s> connected' % repr(self))
        AcqWebSocket.listeners.add(self)
        self.write_message(json_encode(data_till_now))
    def on_message(self, message):
        logging.log(logging.INFO, '<%s> received: %s' % (repr(self), repr(message)))
    def on_close(self):
        logging.log(logging.DEBUG, '<%s> disconnected' % repr(self))
        AcqWebSocket.listeners.discard(self)
    @staticmethod
    def write_all(message, **kwargs):
        for listener in AcqWebSocket.listeners:
            listener.write_message(message, **kwargs)

class ControlHandler(web.RequestHandler):
    @web.removeslash
    def put(self, command):
        body = self.request.body
        body = json_decode(body) if body else None
        logging.log(logging.INFO, '<%s> command <%s>: %s' %
                    (repr(self), command, repr(body)))
        if command == 'start':
            clear_acq()

def clear_acq():
    global t0, data_till_now
    t0, data_till_now = 0, []
    AcqWebSocket.write_all(json_encode({"kind": "clear"}))

t0 = 0
def send_data_test():
    global t0, data_till_now
    ts = time.time() - t0
    if ts > 300:
        clear_acq()
        t0, ts = time.time(), 0
    datum = [random.randint(0,(1<<10)-1),ts]
    AcqWebSocket.write_all(json_encode([datum]))
    data_till_now.append(datum)

def send_stim_test():
    AcqWebSocket.write_all(json_encode({
        "kind": "stim",
        "data": [(1 << random.randint(0,7)) for i in xrange(2048)]
    }))

application = web.Application([
    (r"/()$", web.StaticFileHandler, {'path': os.path.join(static_path, 'index.html')}),
    (r"/socket", AcqWebSocket),
    (r"/control/(.+)", ControlHandler),
    (r"/(.+)", web.StaticFileHandler, {'path': static_path})
])

if __name__ == '__main__':
    application.listen(8888)
    ioloop.PeriodicCallback(send_data_test, 10).start()
    ioloop.PeriodicCallback(send_stim_test, 4000).start()
    ioloop.IOLoop.instance().start()
