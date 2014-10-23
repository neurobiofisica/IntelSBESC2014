#!/usr/bin/env python
# -*- encoding: utf-8 -*-

import tornado.ioloop
import os, sys, struct, time, traceback

import json

# Same numbers used on the driver (see defines)
__FIFO_DAT__ = '/dev/rtf0'
__FIFO_CMD__ = '/dev/rtf1'

__CLEAR_FIFO__ = 0

# TODO: Perguntar numero de canais em uma interface bela
__NUM_SPK__ = 8

ticks_per_second = 1.e6

def fifo_send(devnode, data):
    sys.stderr.write('fifo_send devnode=%s, data=%s\n' % (devnode, data.encode('hex')))
    sys.stderr.flush()
    fd = os.open(devnode, os.O_WRONLY)
    os.write(fd, data)
    os.close(fd)

'''def menu_window():
    global msg
    msg = QMessageBox(QMessageBox.Information, "Send command", "")
    msg.setModal(False)
    btn = msg.addButton("Reset FIFO", QMessageBox.ResetRole)
    def reset_fifo():
        print("Clearing FIFO_CMD")
        fifo_send(__FIFO_CMD__, struct.pack('<I', __CLEAR_FIFO__))
        menu_window()
    btn.connect(btn, SIGNAL("clicked(bool)"), reset_fifo)
    msg.show()
'''
def start(handlers):
    """ Initialize FIFOs and add the given receivers. """
    global __FIFO_DAT__
    global __recv_notifier__

    # Clear FIFO_DAT buffer
    print("Clearing FIFO_DAT buffer...")
    fifo_send(__FIFO_CMD__, struct.pack('<I', __CLEAR_FIFO__))

    # Open Menu Window
    # menu_window()

     # Open save files
    dest = '/home/neurostorm/acquisitions/' + time.strftime('%Y%m%d-%H%M%S')
    os.makedirs(dest)
    os.chdir(dest)
    spkdest = []
    for i in range(1, __NUM_SPK__+1):
        spkdest.append(open('spike%d.dat'%i, 'w'))

    fd = os.open(__FIFO_DAT__, os.O_RDONLY)
    def callback(fd_, event):
        """ Callback for the receiving FIFO. """
        # Receive data
        dataraw = os.read(fd_, 8)
        if dataraw == '':
            return
        data, = struct.unpack('<Q', dataraw)

        # Interpret data
        flags = (data & 0xffffffff)
        rawts = (data >> 32) 
        ts = float(rawts) / ticks_per_second

        # Dispatch to handlers
        # Objects of a class that contains the 'process' method
        if flags != 0x100:
            for x in handlers:
                try:
                    x.process(json.dumps( [flags, ts] ))
                except: 
                    traceback.print_exc()
        
        # Save data
        # Nota: Modifiquei pois nao ha a entrada da tela!!
        for i in range(__NUM_SPK__):
            if ((1 << i) & flags) != 0:
                spkdest[i].write('%u\n'%rawts)
    
    io_loop = tornado.ioloop.IOLoop.instance()
    io_loop.add_handler(fd, callback, io_loop.READ)
    return io_loop

    # TODO Verificar como ativar e desativar interrupcoes do hardware por FIFOs


#app = QApplication(sys.argv)
