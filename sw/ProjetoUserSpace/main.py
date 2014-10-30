from appInterrupts import *
from TornadoCel import *
import tornado.web

import os

__dirname__ = os.path.dirname(os.path.abspath(__file__))
static_path = os.path.join(__dirname__, 'Cliente')

application = tornado.web.Application([
    (r"/()$", tornado.web.StaticFileHandler, {'path': os.path.join(static_path,
'index.html')}),
    (r"/socket", TornadoRecv),
    (r"/(.*)", tornado.web.StaticFileHandler, {"path": static_path}),
])

io_loop = start(handlers, data_till_now) #imported from TornadoCel

if __name__=="__main__":
    application.listen(8888)
    io_loop.start()
