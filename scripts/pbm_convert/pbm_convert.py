import sys
from os import listdir
from os.path import isfile, join

if len(sys.argv) != 3:
    print("usage: pbm_convert.py <bitmap_header_dir> <image_dir>")
    exit(1)

bitmap_dir = sys.argv[1]
image_dir = sys.argv[2]
bitmaps = [f for f in listdir(bitmap_dir) if isfile(join(bitmap_dir, f))]

fullscreen_dim = 256 * 256
fullscreen_byte_size = fullscreen_dim / 8

for bmp in bitmaps:
    if bmp.endswith('.hpp') == False:
        continue

    print(bmp)
    fp = open(join(bitmap_dir, bmp), 'r')
    fileparts = bmp.split('.')
    base = fileparts[0]
    outfile = join(image_dir, base) + ".pbm"

    fp_out = open(outfile, 'w')
    lines = fp.readlines()

    fields_filename = join(image_dir, base) + '.hpp'
    fields_fp = open(fields_filename, 'w')

    width = 0
    height = 0
    width_name = ''
    height_name = ''
    data_name = ''
    bmpbytes = []
    for line in lines:
        if '_width ' in line or '_WIDTH' in line or 'Width' in line:
            parts = line.split()
            width_name = parts[1]
            width = int(parts[len(parts)-1])
        if '_height ' in line or '_HEIGHT' in line or 'Height' in line:
            parts = line.split()
            height_name = parts[1]
            height = int(parts[len(parts)-1])
        if '[]' in line:
            parts = line.split()
            for p in parts:
                if '[]' in p:
                    pp = p.split('[')
                    data_name = pp[0]

        # parse binary data
        if '0x' in line:
            translation_table = dict.fromkeys(map(ord, '{,};'), None)
            line = line.translate(translation_table)
            parts = line.split()
            for p in parts:
                if '0x' in p:
                    b = int(p, 16)
                    flipped = 0
                    for i in range(0, 8):
                        flipped <<= 1
                        if b & 1<<i != 0:
                            flipped |= 1
                    bmpbytes.append(flipped)

    #verify height, width parsed properly
    if width == 0 or height == 0:
        # check size of bmpbytes, could be full screen?
        if len(bmpbytes) == fullscreen_byte_size:
            width = 256
            height = 256
            width_name = data_name + '_width'
            height_name = data_name + '_height'
        else:
            print("error in " + bmp + ", could not parse dimensions")
            continue

    #write top of header
    fields_fp.write('#pragma once\n')
    fields_fp.write('#define '+width_name+' '+str(width)+'\n')
    fields_fp.write('#define '+height_name+' '+str(height)+'\n')
    fields_fp.write('static const unsigned char ' + data_name + '[] = {\n')
    fields_fp.close()

    #write header
    fp_out.writelines("P4\n%d %d\n" % (width, height))
    fp_out.close()

    fp_out = open(outfile, 'ab+')
    for b in bmpbytes:
        byte_val = b.to_bytes(1, 'big')
        fp_out.write(byte_val)
    fp_out.close()
    fp.close()

print("done.")