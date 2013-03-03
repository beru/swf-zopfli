/*
Copyright 2011 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Author: lode.vandevenne@gmail.com (Lode Vandevenne)
Author: jyrki.alakuijala@gmail.com (Jyrki Alakuijala)
*/

/*
Zopfli compressor program. It can output gzip-, zlib- or deflate-compatible
data. By default it creates a .gz file. This tool can only compress, not
decompress. Decompression can be done by any standard gzip, zlib or deflate
decompressor.
*/

/*
 swf-Zopfli compresses Macromedia Flash SWF file.
 The following conditions must be met.
 Signature byte of SWF file header must be "C" or "F".
 Version field of SWF file header must be equal or greater than 6.

Author: berupon@gmail.com
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zlib_container.h"

/*
Loads a file into a memory array.
*/
static void LoadFile(const char* filename, unsigned char** out, size_t* outsize) {
  FILE* file;

  *out = 0;
  *outsize = 0;
  file = fopen(filename, "rb");
  if (!file) return;

  fseek(file , 0 , SEEK_END);
  *outsize = ftell(file);
  rewind(file);

  *out = (unsigned char*)malloc(*outsize);

  if (*outsize && (*out)) {
    size_t testsize = fread(*out, 1, *outsize, file);
    if (testsize != *outsize) {
      /* It could be a directory */
      free(*out);
      *out = 0;
      *outsize = 0;
    }
  }

  assert(!(*outsize) || out);  /* If size is not zero, out must be allocated. */
  fclose(file);
}

#pragma pack(push, 1)
typedef struct {
  char signatures[3];
  unsigned char version;
  unsigned int fileLength;
}SWFHeader;
#pragma pack(pop)

#define MINIZ_HEADER_FILE_ONLY
#include "miniz.c"

/*
outfilename: filename to write output to
*/
void CompressFile(const Options* options, const char* infilename, const char* outfilename) {
  unsigned char* in;
  size_t insize;
  unsigned char* out = 0;
  size_t outsize = 0;
  LoadFile(infilename, &in, &insize);
  if (insize == 0) {
    fprintf(stderr, "Invalid filename: %s\n", infilename);
    return;
  }
	
	// God forgives 4 space tab...
	{
		SWFHeader header = *(const SWFHeader*)in;
		const char s0 = header.signatures[0];
		unsigned char* wrk;
		size_t wrkSize;
		if (s0 == 'F' || s0 == 'C') {
			if (header.version < 6) {
				free(in);
				fprintf(stderr, "SWF version must be equal or greater than 6\n");
				return;
			}
			wrkSize = header.fileLength - sizeof(SWFHeader);
			wrk = malloc(wrkSize);
			// zlib compressed
			if (s0 == 'C') {
				// decompress zlib data
				int ret = mz_uncompress(wrk, &wrkSize, in+sizeof(header), insize-sizeof(SWFHeader));
				if (ret != MZ_OK) {
					fprintf(stderr, "Failed to uncompress zlib data: %s\n", mz_error(ret));
				}
			}else {
				wrk = in + sizeof(header);
				wrkSize = insize-sizeof(SWFHeader);
			}
			{
				FILE* file = fopen(outfilename, "wb" );
				assert(file);
				header.signatures[0] = 'C';
				fwrite(&header, sizeof(header), 1, file);
				ZlibCompress(options, wrk, wrkSize, &out, &outsize);
				fwrite((char*)out, 1, outsize, file);
				fclose(file);
			}
			free(out);
			free(in);
			if (s0 == 'C') {
				free(wrk);
			}
		}else {
			free(in);
			fprintf(stderr, "Unsupported SWF signature: %c\n", s0);
		}
	}
}

/*
Add two strings together. Size does not matter. Result must be freed.
*/
static char* AddStrings(const char* str1, const char* str2) {
  size_t len = strlen(str1) + strlen(str2);
  char* result = (char*)malloc(len + 1);
  if (!result) exit(-1); /* Allocation failed. */
  strcpy(result, str1);
  strcat(result, str2);
  return result;
}

static char StringsEqual(const char* str1, const char* str2) {
  return strcmp(str1, str2) == 0;
}

int main(int argc, char* argv[]) {
  Options options;
  const char* filename = 0;
  int i;

  InitOptions(&options);

  for (i = 1; i < argc; i++) {
    if (StringsEqual(argv[i], "-v")) options.verbose = 1;
    else if (StringsEqual(argv[i], "--i5")) options.numiterations = 5;
    else if (StringsEqual(argv[i], "--i10")) options.numiterations = 10;
    else if (StringsEqual(argv[i], "--i15")) options.numiterations = 15;
    else if (StringsEqual(argv[i], "--i25")) options.numiterations = 25;
    else if (StringsEqual(argv[i], "--i50")) options.numiterations = 50;
    else if (StringsEqual(argv[i], "--i100")) options.numiterations = 100;
    else if (StringsEqual(argv[i], "--i250")) options.numiterations = 250;
    else if (StringsEqual(argv[i], "--i500")) options.numiterations = 500;
    else if (StringsEqual(argv[i], "--i1000")) options.numiterations = 1000;
    else if (StringsEqual(argv[i], "-h")) {
      fprintf(stderr,
          "Usage: swf-zopfli [OPTION]... FILE\n"
          "  -h    gives this help\n"
          "  -v    verbose mode\n"
          "  --i5  less compression, but faster\n"
          "  --i10  less compression, but faster\n"
          "  --i15  default compression, 15 iterations\n"
          "  --i25  more compression, but slower\n"
          "  --i50  more compression, but slower\n"
          "  --i100  more compression, but slower\n"
          "  --i250  more compression, but slower\n"
          "  --i500  more compression, but slower\n"
          "  --i1000  more compression, but slower\n");
      return 0;
    }
  }

  for (i = 1; i < argc; i++) {
    if (argv[i][0] != '-') {
      char* outfilename;
      filename = argv[i];
      outfilename = AddStrings(filename, ".zopfli");
      if (options.verbose && outfilename) {
        fprintf(stderr, "Saving to: %s\n", outfilename);
      }
      CompressFile(&options, filename, outfilename);
      free(outfilename);
    }
  }

  if (!filename) {
    fprintf(stderr,
            "Please provide filename\nFor help, type: %s -h\n", argv[0]);
  }

  return 0;
}
