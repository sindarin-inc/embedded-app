from os import listdir
from os.path import isfile, join

bitmap_dir = '../../src/UI/Bitmaps'
bitmaps = [f for f in listdir(bitmap_dir) if isfile(join(bitmap_dir, f))]

fullscreen_dim = 256 * 256
fullscreen_byte_size = fullscreen_dim / 8

for bmp in bitmaps:
    fp = open(join(bitmap_dir, bmp), 'r')
    fileparts = bmp.split('.')
    base = fileparts[0]
    outfile = base + ".pbm"

    fp_out = open(outfile, 'w')
    lines = fp.readlines()

    width = 0
    height = 0
    bmpbytes = []
    for line in lines:
        if '_width ' in line or '_WIDTH' in line or 'Width' in line:
            parts = line.split()
            width = int(parts[len(parts)-1])
        if '_height ' in line or '_HEIGHT' in line or 'Height' in line:
            parts = line.split()
            height = int(parts[len(parts)-1])

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
        else:
            print("error in " + bmp + ", could not parse dimensions")
            continue

    #write header
    fp_out.writelines("P4\n%d %d\n" % (width, height))
    fp_out.close()

    fp_out = open(outfile, 'ab+')
    for b in bmpbytes:
        byte_val = b.to_bytes(1, 'big')
        fp_out.write(byte_val)
    fp_out.close()
    fp.close()