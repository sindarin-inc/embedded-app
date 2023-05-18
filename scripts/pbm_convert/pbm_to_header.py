from os import listdir
from os.path import isfile, join
import subprocess

#output_dir = '../../src/UI/Bitmaps'
output_dir = './out/'
input_dir = './'
pbms = [f for f in listdir(input_dir) if isfile(join(input_dir, f))]

for pbm in pbms:
    if '.pbm' in pbm:
        parts = pbm.split('.')
        base = parts[0]
        cmd = 'xxd -i ' + join(input_dir, pbm) + ' > ' + join(output_dir, base) + '.hpp'
        subprocess.run(cmd, shell=True)