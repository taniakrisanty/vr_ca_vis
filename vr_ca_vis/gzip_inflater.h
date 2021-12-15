#pragma once

//#include <cstdio>
//#include <string> 
//#include <cstring>
//#include <cstdlib>
//#include <fstream>
//#include <zlib.h>
//#include <zconf.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "zlib.h"

#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

#define CHUNK 16384

class gzip_inflater
{
public:
	//gzip_inflater() = delete;
	//gzip_inflater(const gzip_inflater&) = delete;

    gzip_inflater(const std::string& in_file_name, const std::string& out_file_name)
    {
        FILE *input = fopen(in_file_name.c_str(), "rb");
        FILE* output = fopen(out_file_name.c_str(), "w+");

        /* avoid end-of-line conversions */
		SET_BINARY_MODE(input);
		SET_BINARY_MODE(output);

        int ret = inf(input, output);
		if (ret != Z_OK)
			zerr(ret);
		
        // Read the gz file into a string
        //std::ifstream file(in_file_name);
        //std::string compressedBytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        //if (compressedBytes.size() == 0) {
        //    uncompressedBytes = compressedBytes;
        //    return;
        //}

        //uncompressedBytes.clear();

        //unsigned full_length = compressedBytes.size();
        //unsigned half_length = compressedBytes.size() / 2;

        //unsigned uncompLength = full_length;
        //char* uncomp = (char*)calloc(sizeof(char), uncompLength);

        //z_stream strm;
        //strm.next_in = (Bytef*)compressedBytes.c_str();
        //strm.avail_in = compressedBytes.size();
        //strm.total_out = 0;
        //strm.zalloc = Z_NULL;
        //strm.zfree = Z_NULL;

        //bool done = false;

        //if (inflateInit2(&strm, (16 + MAX_WBITS)) != Z_OK) {
        //    free(uncomp);
        //    return;
        //}

        //while (!done) {
        //    // If our output buffer is too small  
        //    if (strm.total_out >= uncompLength) {
        //        // Increase size of output buffer  
        //        char* uncomp2 = (char*)calloc(sizeof(char), uncompLength + half_length);
        //        memcpy(uncomp2, uncomp, uncompLength);
        //        uncompLength += half_length;
        //        free(uncomp);
        //        uncomp = uncomp2;
        //    }

        //    strm.next_out = (Bytef*)(uncomp + strm.total_out);
        //    strm.avail_out = uncompLength - strm.total_out;

        //    // Inflate another chunk.  
        //    int err = inflate(&strm, Z_SYNC_FLUSH);
        //    if (err == Z_STREAM_END) done = true;
        //    else if (err != Z_OK) {
        //        break;
        //    }
        //}

        //if (inflateEnd(&strm) != Z_OK) {
        //    free(uncomp);
        //    return;
        //    //return false;
        //}

        //for (size_t i = 0; i < strm.total_out; ++i) {
        //    uncompressedBytes += uncomp[i];
        //}
        //free(uncomp);
    }

    /* Decompress from file source to file dest until stream ends or EOF.
       inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
       allocated for processing, Z_DATA_ERROR if the deflate data is
       invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
       the version of the library linked do not match, or Z_ERRNO if there
       is an error reading or writing the files. */
    int inf(FILE* source, FILE* dest)
    {
        int ret;
        unsigned have;
        z_stream strm;
        unsigned char in[CHUNK];
        unsigned char out[CHUNK];

        /* allocate inflate state */
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        strm.avail_in = 0;
        strm.next_in = Z_NULL;
        ret = inflateInit(&strm);
        if (ret != Z_OK)
            return ret;

        /* decompress until deflate stream ends or end of file */
        do {
            strm.avail_in = fread(in, 1, CHUNK, source);
            if (ferror(source)) {
                (void)inflateEnd(&strm);
                return Z_ERRNO;
            }
            if (strm.avail_in == 0)
                break;
            strm.next_in = in;

            /* run inflate() on input until output buffer not full */
            do {
                strm.avail_out = CHUNK;
                strm.next_out = out;
                //ret = inflate(&strm, Z_NO_FLUSH);
                ret = inflate(&strm, Z_FINISH);
                assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
                switch (ret) {
                case Z_NEED_DICT:
                    ret = Z_DATA_ERROR;     /* and fall through */
                case Z_DATA_ERROR:
                case Z_MEM_ERROR:
                    (void)inflateEnd(&strm);
                    return ret;
                }
                have = CHUNK - strm.avail_out;
                if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                    (void)inflateEnd(&strm);
                    return Z_ERRNO;
                }
            } while (strm.avail_out == 0);

            /* done when inflate() says it's done */
        } while (ret != Z_STREAM_END);

        /* clean up and return */
        (void)inflateEnd(&strm);
        return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
    }

    /* report a zlib or i/o error */
    void zerr(int ret)
    {
        fputs("zpipe: ", stderr);
        switch (ret) {
        case Z_ERRNO:
            if (ferror(stdin))
                fputs("error reading stdin\n", stderr);
            if (ferror(stdout))
                fputs("error writing stdout\n", stderr);
            break;
        case Z_STREAM_ERROR:
            fputs("invalid compression level\n", stderr);
            break;
        case Z_DATA_ERROR:
            fputs("invalid or incomplete deflate data\n", stderr);
            break;
        case Z_MEM_ERROR:
            fputs("out of memory\n", stderr);
            break;
        case Z_VERSION_ERROR:
            fputs("zlib version mismatch!\n", stderr);
        }
    }
};
