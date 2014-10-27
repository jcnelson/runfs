#!/usr/bin/python

import sys
import os 
import time

pidfile_path = sys.argv[1]

fd = open(pidfile_path, "w")
fd.write( "%s" % os.getpid() )
fd.close()

print "pidfile written, waiting to die..."

while True:
    time.sleep(1)

