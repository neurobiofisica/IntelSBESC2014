from appInterrupts import *
from TornadoCel import *
import tornado.web
import json

import os

__dirname__ = os.path.dirname(os.path.abspath(__file__))
static_path = os.path.join(__dirname__, 'Cliente')

def send_all(a):
    json_data = json.dumps(a)
    for x in handlers:
        try:
            x.process(json_data)
        except:
            traceback.print_exc()

def clear_acq():
    global first_time, data_till_now, last_stim_sent
    last_stim_sent = -10*time_per_stim_chunk
    first_time, data_till_now = None, []
    send_all({"kind": "clear"})

zigzag_stim = [
    12, 6, 3, 3, 6, 12, 24, 48, 96, 192, 192, 96, 48, 24, 12, 6, 3, 3, 6,
    12, 24, 48, 96, 192, 192, 96, 48, 24, 12, 6, 3, 3, 6, 12, 24, 48, 96,
    192, 192, 96, 48, 24, 12, 6, 3, 3, 6, 12, 24, 48, 96, 192, 192, 96,
    48, 24, 12, 6, 3, 3, 6, 12, 24, 48, 96, 192, 192, 96, 48, 24, 12, 6,
    3, 3, 6, 12, 24, 48, 96, 192, 192, 96, 48, 24, 12, 6, 3, 3, 6, 12, 24,
    48, 96, 192, 192, 96, 48, 24, 12, 6
]

pattern, binsize = '', 1

class ControlHandler(tornado.web.RequestHandler):
    @tornado.web.removeslash
    def put(self, command):
        global pattern, binsize

        body = self.request.body
        body = json.loads(body) if body else None
        print 'command <%s>: %s' % (command, repr(body))

        if command == 'start':
            print "Starting acquisition..."
            clear_acq()
            open_save_files()
            fifo_send(__FIFO_CMD__, struct.pack('<I', __START_ACQ__))
        elif command == 'stop':
            fifo_send(__FIFO_CMD__, struct.pack('<I', __STOP_ACQ__))
        elif command == 'zigzag':
            global stim_data_arr
            stim_data_arr = list(zigzag_stim)
            fifo_send(__FIFO_ARG__, struct.pack('<I', 0))  # STIM_ZIGZAG
            fifo_send(__FIFO_CMD__, struct.pack('<I', __LSTIM_TYPE__))
        elif command == 'binsize':
            binsize = int(body)
            send_pattern(pattern, binsize)
        elif command == 'word':
            pattern = str(body)
            send_pattern(pattern, binsize)
        elif command == 'disablefeedback':
            send_pattern('', 1)
        elif command == 'briefstim':
            send_brief_stimulus(body)
        elif command == 'longstim':
            global stim_data_arr
            stim_data_arr = list(body)
            fifo_send(__FIFO_CMD__, struct.pack('<I', __CLR_FIFO_STM__))
            fifo_send(__FIFO_STM__, ''.join([struct.pack('<B', x) for x in body]))
            fifo_send(__FIFO_ARG__, struct.pack('<I', 1))  # STIM_FIFO
            fifo_send(__FIFO_CMD__, struct.pack('<I', __LSTIM_TYPE__))
        else:
            raise ValueError('not implemented')

application = tornado.web.Application([
    (r"/()$", tornado.web.StaticFileHandler, {'path': os.path.join(static_path,
'index.html')}),
    (r"/socket", TornadoRecv),
    (r"/control/(.+)", ControlHandler),
    (r"/(.*)", tornado.web.StaticFileHandler, {"path": static_path}),
])

io_loop = start(handlers, data_till_now) #imported from TornadoCel

if __name__=="__main__":
    application.listen(8888)
    io_loop.start()
