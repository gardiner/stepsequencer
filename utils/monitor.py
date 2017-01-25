import simplecoremidi
import sys
import time

inp = simplecoremidi.MIDIDestination("simple core midi destination")
ping = 0
beat = 0

while True:
    data = inp.recv()
    if data:
        for byte in data:
            if byte == 250:
                ping = 0
                beat = 0
                print 'GO'
            elif byte == 252:
                ping = 0
                beat = 0
                print 'STOP'
            elif byte == 248:
                ping += 1
                sys.stdout.write('.')
                sys.stdout.flush()
                if ping % (24 * 4) == 0:
                    beat += 1
                    print 'BEAT %s' % beat
            else:
                print byte,
    time.sleep(120./60/24/10)