import sys
import os
from os import listdir
from os.path import isfile, join
import subprocess

if len(sys.argv) != 3:
    print("usage: pbm_to_header.py <input_dir> <output_dir>")
    exit(1)

input_dir = sys.argv[1]
output_dir = sys.argv[2]

pbms = [f for f in listdir(input_dir) if isfile(join(input_dir, f))]

for pbm in pbms:
    if '.pbm' in pbm:
        parts = pbm.split('.')
        base = parts[0]
        inputfilename = join(input_dir, pbm)
        outputfilename = join(output_dir, base) + '.hpp'

        # check for existence of partial header
        hppfilename = join(input_dir, base + '.hpp')
        if os.path.isfile(hppfilename):
            #print(hppfilename)
            cmd = 'cp ' + hppfilename + ' ' + outputfilename
            subprocess.run(cmd, shell=True)
            cmd = 'cat ' + inputfilename + ' | xxd -i >> ' + outputfilename
            subprocess.run(cmd, shell=True)
            cmd = 'echo "};" >> ' + outputfilename
            subprocess.run(cmd, shell=True)
        else:
            print("no header")
            cmd = 'xxd -i ' + inputfilename + ' > ' + outputfilename
            subprocess.run(cmd, shell=True)