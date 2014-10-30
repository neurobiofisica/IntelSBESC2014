#!/usr/bin/env python
# -*- encoding: utf-8 -*-

import tornado.ioloop
import os, sys, struct, time, traceback

import json

# Same numbers used on the driver (see defines)
__FIFO_DAT__ = '/dev/rtf0'
__FIFO_CMD__ = '/dev/rtf1'
__FIFO_STM__ = '/dev/rtf2'
__FIFO_ARG__ = '/dev/rtf3'

__STOP_ACQ__ = 0
__START_ACQ__ = 1
__CLR_FIFO_STM__ = 2
__CHANGE_BFSTM__ = 3
__CHANGE_PATT__ = 4
__LSTIM_TYPE__ = 5

# TODO: Perguntar numero de canais em uma interface bela
__NUM_SPK__ = 8

ticks_per_second = 1.e6

first_time = None

def fifo_send(devnode, data):
    sys.stderr.write('fifo_send devnode=%s, data=%s\n' % (devnode, data.encode('hex')))
    sys.stderr.flush()
    fd = os.open(devnode, os.O_WRONLY)
    os.write(fd, data)
    os.close(fd)

def send_brief_stimulus(positions):
    assert(len(positions) <= 4096)
    posdata = ''.join([struct.pack('<B', pos) for pos in positions])
    fifo_send(__FIFO_ARG__, struct.pack('<I', len(posdata)) + posdata)
    fifo_send(__FIFO_CMD__, struct.pack('<I', 3))

def send_pattern(pattern, binsize):
    assert(len(pattern) <= 64)
    pattern_mask = (1<<len(pattern))-1
    pattern_bits = int(pattern, 2)
    # take care! struct.pack here must have alignment=none
    fifo_send(__FIFO_ARG__, struct.pack('<IQQ', binsize, pattern_bits, pattern_mask))
    fifo_send(__FIFO_CMD__, struct.pack('<I', 4))

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
def start(handlers, data_till_now):
    """ Initialize FIFOs and add the given receivers. """

    # Start acquisition
    send_brief_stimulus(4*[0x80,0x01])
    send_pattern('1101', 1)
    
    print("Starting acquisition...")
    fifo_send(__FIFO_CMD__, struct.pack('<I', __START_ACQ__))

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
        global first_time

        # Receive data
        dataraw = os.read(fd_, 8)
        if dataraw == '':
            return
        data, = struct.unpack('<Q', dataraw)

        # Interpret data
        flags = (data & 0xffffffff)
        rawts = (data >> 32) 
        ts = float(rawts) / ticks_per_second

        if first_time == None:
            first_time = ts

        # Dispatch to handlers
        # Objects of a class that contains the 'process' method
        if flags != 0x100:
            arr_data = [flags, ts - first_time]
            data_till_now.append(  arr_data  )
            json_data = json.dumps( [ arr_data ] )
            for x in handlers:
                try:
                    x.process(json_data)
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
