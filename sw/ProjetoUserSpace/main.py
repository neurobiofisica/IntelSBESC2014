from appInterrupts import *
from TornadoCel import *
import tornado.web

import os

__dirname__ = os.path.dirname(os.path.abspath(__file__))
print __dirname__

application = tornado.web.Application([
    (r"/socket", TornadoRecv),
    (r"/(.*)", tornado.web.StaticFileHandler, {"path": __dirname__+"/Cliente"}),
])

io_loop = start(handlers) #imported from TornadoCel

if __name__=="__main__":
    application.listen(8888)
    io_loop.start()
