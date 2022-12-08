#include "GUnzip.hpp"
//#include <Arduino.h>
#include <memory>
#include <stdio.h>

#include "last.h"

/* Possible inflate modes between inflate() calls */
typedef enum {
    HEAD = 16180, /* i: waiting for magic header */
    FLAGS,        /* i: waiting for method and flags (gzip) */
    TIME,         /* i: waiting for modification time (gzip) */
    OS,           /* i: waiting for extra flags and operating system (gzip) */
    EXLEN,        /* i: waiting for extra length (gzip) */
    EXTRA,        /* i: waiting for extra bytes (gzip) */
    NAME,         /* i: waiting for end of file name (gzip) */
    COMMENT,      /* i: waiting for end of comment (gzip) */
    HCRC,         /* i: waiting for header crc (gzip) */
    DICTID,       /* i: waiting for dictionary check value */
    DICT,         /* waiting for inflateSetDictionary() call */
    TYPE,         /* i: waiting for type bits, including last-flag bit */
    TYPEDO,       /* i: same, but skip check to exit inflate on new block */
    STORED,       /* i: waiting for stored size (length and complement) */
    COPY_,        /* i/o: same as COPY below, but only first time in */
    COPY,         /* i/o: waiting for input or output to copy stored block */
    TABLE,        /* i: waiting for dynamic block table lengths */
    LENLENS,      /* i: waiting for code length code lengths */
    CODELENS,     /* i: waiting for length/lit and distance code lengths */
    LEN_,         /* i: same as LEN below, but only first time in */
    LEN,          /* i: waiting for length/lit/eob code */
    LENEXT,       /* i: waiting for length extra bits */
    DIST,         /* i: waiting for distance code */
    DISTEXT,      /* i: waiting for distance extra bits */
    MATCH,        /* o: waiting for output space to copy string */
    LIT,          /* o: waiting for output space to write literal */
    CHECK,        /* i: waiting for 32-bit check value */
    LENGTH,       /* i: waiting for 32-bit length (gzip) */
    DONE,         /* finished check, done -- remain here until reset */
    BAD,          /* got a data error -- remain here until reset */
    MEM,          /* got an inflate() memory error -- remain here until reset */
    SYNC          /* looking for synchronization bytes to restart inflate() */
} inflate_mode;

typedef struct _inflate_state {
    z_streamp strm;      /* pointer back to this zlib stream */
    inflate_mode mode;   /* current inflate mode */
    int last;            /* true if processing last block */
    int wrap;            /* bit 0 true for zlib, bit 1 true for gzip,
                            bit 2 true to validate check value */
    int havedict;        /* true if dictionary provided */
    int flags;           /* gzip header method and flags (0 if zlib) */
    unsigned dmax;       /* zlib header max distance (INFLATE_STRICT) */
    unsigned long check; /* protected copy of check value */
    unsigned long total; /* protected copy of output count */
    gz_headerp head;     /* where to save gzip header information */
    /* sliding window */
    unsigned wbits;            /* log base 2 of requested window size */
    unsigned wsize;            /* window size or zero if not using window */
    unsigned whave;            /* valid bytes in the window */
    unsigned wnext;            /* window write index */
    unsigned char FAR *window; /* allocated sliding window, if needed */
        /* bit accumulator */} inflate_state;

        void GUnzip::open() {

            auto buffer = std::make_unique<uint8_t[]>(32768 + 7168);
            inflateBuf_ = buffer.get();

            int err = Z_OK;

            zstr_.zalloc = (alloc_func)0;
            zstr_.zfree = (free_func)0;
            zstr_.opaque = (voidpf)0;
            zstr_.state = (struct internal_state *)&inflateBuf_[32768];
            inflate_state *st = (inflate_state *)zstr_.state;
            st->window = inflateBuf_;

            zstr_.avail_in = 0;
            zstr_.next_in = Z_NULL;

            err = inflateInit2(&zstr_, 16 + MAX_WBITS);
            // err = inflateInit2_(NULL, 0, 0, 0);
            // err = inflateDummy();

            /*
            if (err == Z_OK) {
                printf("zlib init ok\n");
            } else {
                printf("zlib error: %d\n", err);
            }
            */
        }

        int GUnzip::decompress(uint8_t *input, int input_size, uint8_t *output, int output_size,
                               int &input_read, int &output_avail) {

            zstr_.next_in = input;
            zstr_.avail_in = input_size;
            zstr_.next_out = output;
            zstr_.avail_out = output_size;

            int in_before = zstr_.total_in;
            int out_before = zstr_.total_out;

            int err = Z_OK;
            err = inflate(&zstr_, Z_SYNC_FLUSH);

            input_read = zstr_.total_in - in_before;
            output_avail = zstr_.total_out - out_before;
            return err;
        }

        void GUnzip::decompressFile(FileSystem &fileSystem, const std::string &inputFileName,
                                    const std::string &outputFileName) {

            File in = fileSystem.open(inputFileName);
            File out = fileSystem.open(outputFileName, "w", true);

            bool done = false;
            uint8_t inBuf[1024];
            uint8_t outBuf[1024];
            int input_avail = 0;
            int input_index = 0;
            int input_read = 0;
            int output_avail = 0;
            int block = 0;
            bool last = false;
            while (!done) {

                if (input_avail == 0) {

                    if (last) {
                        done = true;
                        continue;
                    }

                    int readBytes = in.readBytes((char *)inBuf, 1024);
                    if (readBytes == 0) {
                        log_i("done reading!");
                        done = true;
                        continue;
                    }

                    if (readBytes < 1024) {
                        last = true;
                    }

                    input_avail = readBytes;
                    input_index = 0;
                }

                int err = decompress(&inBuf[input_index], input_avail, outBuf, 1024, input_read,
                                     output_avail);

                // write block
                if (output_avail > 0) {
                    out.write(outBuf, output_avail);
                }

                // continue reading until no more output available
                while (output_avail == 1024) {
                    int ir = 0;
                    err = decompress(inBuf, 0, outBuf, 1024, ir, output_avail);

                    // write block
                    if (output_avail > 0) {
                        out.write(outBuf, output_avail);
                    }
                }

                // increment input
                input_index += input_read;
                input_avail -= input_read;
            }

            log_i("done, closing.");
            in.close();
            out.close();
        }

        void GUnzip::decompressMemory() {
            uint8_t *fileptr = (uint8_t *)___data_last_txt_gz;
            int filelen = ___data_last_txt_gz_len;
            int curr_file_byte = 0;

            bool done = false;
            uint8_t inBuf[1024];
            uint8_t outBuf[1024];
            int input_avail = 0;
            int input_index = 0;
            int input_read = 0;
            int output_avail = 0;
            int block = 0;
            bool last = false;
            while (!done) {

                if (input_avail == 0) {

                    if (last) {
                        done = true;
                        continue;
                    }

                    // int readBytes = in.readBytes((char *)inBuf, 1024);

                    int bytes_to_read = 1024;
                    if ((filelen - curr_file_byte) < 1024) {
                        bytes_to_read = filelen - curr_file_byte;
                    }

                    memcpy(inBuf, fileptr, bytes_to_read);
                    fileptr += bytes_to_read;
                    curr_file_byte += bytes_to_read;
                    int readBytes = bytes_to_read;

                    if (readBytes == 0) {
                        log_i("done reading!");
                        done = true;
                        continue;
                    }

                    if (readBytes < 1024) {
                        last = true;
                    }

                    input_avail = readBytes;
                    input_index = 0;
                }

                int err = decompress(&inBuf[input_index], input_avail, outBuf, 1024, input_read,
                                     output_avail);

                // continue reading until no more output available
                while (output_avail == 1024) {
                    int ir = 0;
                    err = decompress(inBuf, 0, outBuf, 1024, ir, output_avail);
                }

                // increment input
                input_index += input_read;
                input_avail -= input_read;
            }

            log_i("done, closing.");
        }