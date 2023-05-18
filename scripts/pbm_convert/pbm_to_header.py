import sys
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
        cmd = 'xxd -i ' + join(input_dir, pbm) + ' > ' + join(output_dir, base) + '.hpp'
        subprocess.run(cmd, shell=True)