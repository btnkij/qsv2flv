/*
 * yamdi.c
 *
 * Copyright (c) 2007+, Ingo Oppermann
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * -----------------------------------------------------------------------------
 *
 * Compile with:
 * gcc yamdi.c -o yamdi -Wall -O2
 *
 * -----------------------------------------------------------------------------
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>

#ifdef __MINGW32__
	#define off_t off64_t
	#define fseeko(stream, offset, origin) fseeko64(stream, offset, origin)
	#define ftello(stream) ftello64(stream)
#endif

#define YAMDI_VERSION			"1.9"

#define YAMDI_OK			0
#define YAMDI_ERROR			1
#define YAMDI_FILE_TOO_SMALL		2
#define YAMDI_INVALID_SIGNATURE		3
#define YAMDI_INVALID_FLVVERSION	4
#define YAMDI_INVALID_DATASIZE		5
#define YAMDI_READ_ERROR		6
#define YAMDI_INVALID_PREVIOUSTAGSIZE	7
#define YAMDI_OUT_OF_MEMORY		8
#define YAMDI_H264_USELESS_NALU		9
#define YAMDI_RENAME_OUTPUT		10
#define YAMDI_INVALID_TAGTYPE		11

#define FLV_SIZE_HEADER			9
#define FLV_SIZE_PREVIOUSTAGSIZE	4
#define FLV_SIZE_TAGHEADER		11

#define FLV_TAG_AUDIO			8
#define FLV_TAG_VIDEO			9
#define FLV_TAG_SCRIPTDATA		18

#define FLV_PACKET_H263VIDEO		2
#define FLV_PACKET_SCREENVIDEO		3
#define	FLV_PACKET_VP6VIDEO		4
#define	FLV_PACKET_VP6ALPHAVIDEO	5
#define FLV_PACKET_SCREENV2VIDEO	6
#define FLV_PACKET_H264VIDEO		7

#define FLV_UI32(x) (unsigned int)(((*(x)) << 24) + ((*(x + 1)) << 16) + ((*(x + 2)) << 8) + (*(x + 3)))
#define FLV_UI24(x) (unsigned int)(((*(x)) << 16) + ((*(x + 1)) << 8) + (*(x + 2)))
#define FLV_UI16(x) (unsigned int)(((*(x)) << 8) + (*(x + 1)))
#define FLV_UI8(x) (unsigned int)(*(x))
#define FLV_TIMESTAMP(x) (int)(((*(x + 3)) << 24) + ((*(x)) << 16) + ((*(x + 1)) << 8) + (*(x + 2)))

typedef struct {
	unsigned char *data;
	size_t size;
	size_t used;
} buffer_t;

typedef struct {
	off_t offset;			// Offset from the beginning of the file

	// FLV spec v10
	unsigned int tagtype;
	size_t datasize;		// Size of the data contained in this tag
	int timestamp;
	short keyframe;			// Is this tag a keyframe?

	size_t tagsize;		// Size of the whole tag including header and data
} FLVTag_t;

typedef struct {
	size_t nflvtags;
	FLVTag_t *flvtag;
} FLVIndex_t;

typedef struct {
	FLVIndex_t index;

	int hascuepoints;
	int canseektoend;			// Set to 1 if the last video frame is a keyframe

	short hasaudio;
	struct {
		short analyzed;			// Are the audio specs complete and valid?

		// Audio specs
		short codecid;
		short samplerate;
		short samplesize;
		short delay;
		short stereo;

		// Calculated values
		size_t ntags;			// # of audio tags
		double datarate;		// datasize / duration
		uint64_t datasize;		// Size of the audio data
		uint64_t size;			// Size of the audio tags (header + data)
		int keyframerate;		// Store every x tags a keyframe. Only used for -a
		int keyframedistance;		// The time between two keyframes. Only used for -a

		int lasttimestamp;
		size_t lastframeindex;
	} audio;

	short hasvideo;
	struct {
		short analyzed;			// Are the video specs complete and valid?

		// Video specs
		short codecid;
		int height;
		int width;

		// Calculated values
		size_t ntags;			// # of video tags
		double framerate;		// ntags / duration
		double datarate;		// datasize / duration
		uint64_t datasize;		// Size of the video data
		uint64_t size;			// Size of the video tags (header + data)

		int lasttimestamp;
		size_t lastframeindex;
	} video;

	short haskeyframes;
	struct {
		size_t lastkeyframeindex;
		int lastkeyframetimestamp;
		off_t lastkeyframelocation;

		size_t nkeyframes;		// # of key frames
		off_t *keyframelocations;	// Array of the filepositions of the keyframes (in the target file!)
		int *keyframetimestamps;	// Array of the timestamps of the keyframes
	} keyframes;

	uint64_t datasize;			// Size of all audio and video tags (header + data + FLV_SIZE_PREVIOUSTAGSIZE)
	uint64_t filesize;			// [sic!]

	int lasttimestamp;

	int lastsecond;
	size_t lastsecondindex;

	struct {
		char creator[256];		// -c

		short addonlastkeyframe;	// -k
		short addonlastsecond;		// -s, -l (deprecated)
		short addonmetadata;		// defaults to 1, -M does change it
		short addaudiokeyframes;	// -a

		short keepmetadata;		// -m (not implemented)
		short stripmetadata;		// -M

		short xmlomitkeyframes;		// -X

		short overwriteinput;		// -w
	} options;

	buffer_t onmetadata;
	buffer_t onlastkeyframe;
	buffer_t onlastsecond;
} FLV_t;

typedef struct {
	unsigned char *bytes;
	size_t length;
	size_t byte;
	short bit;
} bitstream_t;

typedef struct {
	short valid;
	int width;
	int height;
} h264data_t;

int validateFLV(FILE *fp);
int initFLV(FLV_t *flv);
int indexFLV(FLV_t *flv, FILE *fp);
int finalizeFLV(FLV_t *flv, FILE *fp);
int writeFLV(FILE *out, FLV_t *flv, FILE *fp);
int freeFLV(FLV_t *flv);

void storeFLVFromStdin(FILE *fp);
int readFLVTag(FLVTag_t *flvtag, off_t offset, FILE *fp);
int readFLVTagData(unsigned char *ptr, size_t size, FLVTag_t *flvtag, FILE *stream);

int analyzeFLV(FLV_t *flv, FILE *fp);
int analyzeFLVH263VideoPacket(FLV_t *flv, FLVTag_t *flvtag, FILE *fp);
int analyzeFLVH264VideoPacket(FLV_t *flv, FLVTag_t *flvtag, FILE *fp);
int analyzeFLVScreenVideoPacket(FLV_t *flv, FLVTag_t *flvtag, FILE *fp);
int analyzeFLVVP6VideoPacket(FLV_t *flv, FLVTag_t *flvtag, FILE *fp);
int analyzeFLVVP6AlphaVideoPacket(FLV_t *flv, FLVTag_t *flvtag, FILE *fp);

int createFLVEvents(FLV_t *flv);
int createFLVEventOnMetaData(FLV_t *flv);
int createFLVEventOnLastKeyframe(FLV_t *flv);
int createFLVEventOnLastSecond(FLV_t *flv);

int writeBufferFLVScriptDataTag(buffer_t *buffer, int timestamp, size_t datasize);
int writeBufferFLVPreviousTagSize(buffer_t *buffer, size_t tagsize);
int writeBufferFLVScriptDataValueArray(buffer_t *buffer, const char *name, size_t len);
int writeBufferFLVScriptDataECMAArray(buffer_t *buffer, const char *name, size_t len);
int writeBufferFLVScriptDataVariableArray(buffer_t *buffer, const char *name);
int writeBufferFLVScriptDataVariableArrayEnd(buffer_t *buffer);
int writeBufferFLVScriptDataValueString(buffer_t *buffer, const char *name, const char *value);
int writeBufferFLVScriptDataValueBool(buffer_t *buffer, const char *name, int value);
int writeBufferFLVScriptDataValueDouble(buffer_t *buffer, const char *name, double value);
int writeBufferFLVScriptDataObject(buffer_t *buffer);
int writeBufferFLVScriptDataString(buffer_t *buffer, const char *s);
int writeBufferFLVScriptDataLongString(buffer_t *buffer, const char *s);
int writeBufferFLVBool(buffer_t *buffer, int value);
int writeBufferFLVDouble(buffer_t *buffer, double v);

int writeFLVHeader(FILE *fp, int hasaudio, int hasvideo);
int writeFLVDataTag(FILE *fp, int type, int timestamp, size_t datasize);
int writeFLVPreviousTagSize(FILE *fp, size_t tagsize);

void writeXMLMetadata(FILE *fp, const char *infile, const char *outfile, FLV_t *flv);

int readBytes(unsigned char *ptr, size_t size, FILE *stream);

int readH264NALUnit(h264data_t *h264data, unsigned char *nalu, int length);
void readH264SPS(h264data_t *h264data, bitstream_t *bitstream);

unsigned int readCodedU(bitstream_t *bitstream, int nbits, const char *name);
unsigned int readCodedUE(bitstream_t *bitstream, const char *name);
int readCodedSE(bitstream_t *bitstream, const char *name);

int readBits(bitstream_t *bitstream, int nbits);
int readBit(bitstream_t *bitstream);

int bufferInit(buffer_t *buffer);
int bufferFree(buffer_t *buffer);
int bufferReset(buffer_t *buffer);
int bufferAppendBuffer(buffer_t *dst, buffer_t *src);
int bufferAppendString(buffer_t *dst, const unsigned char *string);
int bufferAppendBytes(buffer_t *dst, const unsigned char *bytes, size_t nbytes);

int isBigEndian(void);

void printUsage(void);

int yamdi_main(int argc, char **argv) {
	FILE *fp_infile = NULL, *fp_outfile = NULL, *fp_xmloutfile = NULL;
	int c, unlink_infile = 0;
	char *infile, *outfile, *xmloutfile, *tempfile;
	FLV_t flv;

#ifdef DEBUG
	fprintf(stderr, "[core] sizeof size_t = %d\n", (int)sizeof(size_t));
	fprintf(stderr, "[core] sizeof off_t = %d\n", (int)sizeof(off_t));
	fprintf(stderr, "[core] sizeof uint64_t = %d\n", (int)sizeof(uint64_t));
	fprintf(stderr, "[core] sizeof long long = %d\n", (int)sizeof(long long));
	fprintf(stderr, "[core] sizeof double = %d\n", (int)sizeof(double));
#endif

	opterr = 0;

	infile = NULL;
	outfile = NULL;
	xmloutfile = NULL;
	tempfile = NULL;

	initFLV(&flv);

	while((c = getopt(argc, argv, ":i:o:x:t:c:a:lskMXwh")) != -1) {
		switch(c) {
			case 'i':
				infile = optarg;
				break;
			case 'o':
				outfile = optarg;
				break;
			case 'x':
				xmloutfile = optarg;
				break;
			case 't':
				tempfile = optarg;
				break;
			case 'c':
				strncpy(flv.options.creator, optarg, sizeof(flv.options.creator));
				break;
			case 'l':
			case 's':
				flv.options.addonlastsecond = 1;
				break;
			case 'k':
				flv.options.addonlastkeyframe = 1;
				break;
			case 'a':
				flv.options.addaudiokeyframes = 1;
				flv.audio.keyframedistance = (int)strtol(optarg, (char **)NULL, 10);
				if(flv.audio.keyframedistance <= 0) {
					flv.audio.keyframedistance = 0;
					flv.options.addaudiokeyframes = 0;
				}
				break;
/*
			case 'm':
				flv.options.keepmetadata = 1;
				break;
*/
			case 'M':
				flv.options.stripmetadata = 1;
				break;
			case 'X':
				flv.options.xmlomitkeyframes = 1;
				break;
			case 'w':
				flv.options.overwriteinput = 1;
				break;
			case 'h':
				printUsage();
				exit(YAMDI_ERROR);
				break;
			case ':':
				fprintf(stderr, "The option -%c expects a parameter. -h for help.\n", optopt);
				exit(YAMDI_ERROR);
				break;
			case '?':
				fprintf(stderr, "Unknown option: -%c. -h for help.\n", optopt);
				exit(YAMDI_ERROR);
				break;
			default:
				printUsage();
				exit(YAMDI_ERROR);
				break;
		}
	}

	if(infile == NULL) {
		fprintf(stderr, "Please use -i to provide an input file. -h for help.\n");
		exit(YAMDI_ERROR);
	}

	if(outfile == NULL && xmloutfile == NULL) {
		fprintf(stderr, "Please use -o or -x to provide at least one output file. -h for help.\n");
		exit(YAMDI_ERROR);
	}

	if(tempfile == NULL && !strcmp(infile, "-")) {
		fprintf(stderr, "Please use -t to specify a temporary file. -h for help.\n");
		exit(YAMDI_ERROR);
	}

	// Check input file
	if(!strcmp(infile, "-")) {		// Read from stdin
		// tempfile is available
		// Check if the possible outfiles collide with the tempfile
		if(outfile != NULL) {
			if(!strcmp(tempfile, outfile)) {
				fprintf(stderr, "The temporary file and the output file must not be the same.\n");
				exit(YAMDI_ERROR);
			}
		}

		if(xmloutfile != NULL) {
			if(!strcmp(tempfile, xmloutfile)) {
				fprintf(stderr, "The temporary file and the XML output file must not be the same.\n");
				exit(YAMDI_ERROR);
			}
		}
	}
	else {					// Read from file
		// infile is available
		// Check if the possible outfile collide with the infile
		if(outfile != NULL) {
			if(!strcmp(infile, outfile)) {
				fprintf(stderr, "The input file and the output file must not be the same.\n");
				exit(YAMDI_ERROR);
			}
		}

		if(xmloutfile != NULL) {
			if(!strcmp(infile, xmloutfile)) {
				fprintf(stderr, "The input file and the XML output file must not be the same.\n");
				exit(YAMDI_ERROR);
			}
		}
	}

	// Check output file
	if(outfile != NULL) {
		if(xmloutfile != NULL) {
			if(!strcmp(outfile, xmloutfile)) {
				fprintf(stderr, "The output file and the XML output file must not be the same.\n");
				exit(YAMDI_ERROR);
			}
		}
	}

	// All checks are done. Open the files.

	// Open the inputfile
	// Store data to tempfile if inputfile is stdin
	if(!strcmp(infile, "-")) {
		fp_infile = fopen(tempfile, "wb");
		if(fp_infile == NULL) {
			fprintf(stderr, "Couldn't open the tempfile %s.\n", tempfile);
			exit(YAMDI_ERROR);
		}

		// Store stdin to temporary file
		storeFLVFromStdin(fp_infile);

		// Close temporary file
		fclose(fp_infile);

		// Mimic normal input file, but don't forget to remove the temporary file
		infile = tempfile;
		unlink_infile = 1;
	}

	fp_infile = fopen(infile, "rb");
	if(fp_infile == NULL) {
		if(unlink_infile == 1)
			unlink(infile);

		exit(YAMDI_ERROR);
	}

	// Check if we have a valid FLV file
	if(validateFLV(fp_infile) != YAMDI_OK) {
		fclose(fp_infile);

		if(unlink_infile == 1)
			unlink(infile);

		exit(YAMDI_ERROR);
	}

	// Open the outfile
	fp_outfile = NULL;
	if(outfile != NULL) {
		if(strcmp(outfile, "-")) {
			fp_outfile = fopen(outfile, "wb");
			if(fp_outfile == NULL) {
				fprintf(stderr, "Couldn't open %s.\n", outfile);

				if(unlink_infile == 1)
					unlink(infile);

				exit(YAMDI_ERROR);
			}
		}
		else
			fp_outfile = stdout;
	}

	// Open the XML outputfile
	fp_xmloutfile = NULL;
	if(xmloutfile != NULL) {
		if(strcmp(xmloutfile, "-")) {
			fp_xmloutfile = fopen(xmloutfile, "wb");
			if(fp_xmloutfile == NULL) {
				fprintf(stderr, "Couldn't open %s.\n", xmloutfile);

				if(unlink_infile == 1)
					unlink(infile);

				exit(YAMDI_ERROR);
			}
		}
		else
			fp_xmloutfile = stdout;
	}

	// Check the options
	if(flv.options.stripmetadata == 1) {
		flv.options.addonlastkeyframe = 0;
		flv.options.addonlastsecond = 0;
		flv.options.addonmetadata = 0;
	}
	else
		flv.options.addonmetadata = 1;

	// Create an index of the FLV file
	if(indexFLV(&flv, fp_infile) != YAMDI_OK) {
		fclose(fp_infile);

		if(unlink_infile == 1)
			unlink(infile);

		exit(YAMDI_ERROR);
	}

	if(analyzeFLV(&flv, fp_infile) != YAMDI_OK) {
		fclose(fp_infile);

		if(unlink_infile == 1)
			unlink(infile);

		exit(YAMDI_ERROR);
	}

	if(finalizeFLV(&flv, fp_infile) != YAMDI_OK) {
		fclose(fp_infile);

		if(unlink_infile == 1)
			unlink(infile);

		exit(YAMDI_ERROR);
	}

#ifdef DEBUG
	fprintf(stderr, "[FLV] onmetadata = %d bytes (%d bytes allocated)\n", flv.onmetadata.used, flv.onmetadata.size);
	fprintf(stderr, "[FLV] onlastsecond = %d bytes (%d bytes allocated)\n", flv.onlastsecond.used, flv.onlastsecond.size);
	fprintf(stderr, "[FLV] onlastkeyframe = %d bytes (%d bytes allocated)\n", flv.onlastkeyframe.used, flv.onlastkeyframe.size);
#endif

	if(fp_outfile != NULL)
		writeFLV(fp_outfile, &flv, fp_infile);

	if(fp_xmloutfile != NULL)
		writeXMLMetadata(fp_xmloutfile, infile, outfile, &flv);

	fclose(fp_infile);

	// Remove the input file if it is the temporary file
	if(unlink_infile == 1)
		unlink(infile);

	if(fp_outfile != NULL && fp_outfile != stdout)
		fclose(fp_outfile);

	if(fp_xmloutfile != NULL && fp_xmloutfile != stdout)
		fclose(fp_xmloutfile);

	if(flv.options.overwriteinput == 1 && strcmp(infile, "-") && outfile != NULL && strcmp(outfile, "-")) {
		if(rename(outfile, infile) != 0)
			exit(YAMDI_RENAME_OUTPUT);
	}

	freeFLV(&flv);

	return YAMDI_OK;
}

int validateFLV(FILE *fp) {
	unsigned char buffer[FLV_SIZE_HEADER + FLV_SIZE_PREVIOUSTAGSIZE];
	off_t filesize;

	fseeko(fp, 0, SEEK_END);
	filesize = ftello(fp);

	// Check for minimal FLV file length
	if(filesize < (FLV_SIZE_HEADER + FLV_SIZE_PREVIOUSTAGSIZE))
		return YAMDI_FILE_TOO_SMALL;

	rewind(fp);

	if(readBytes(buffer, FLV_SIZE_HEADER + FLV_SIZE_PREVIOUSTAGSIZE, fp) != YAMDI_OK)
		return YAMDI_READ_ERROR;

	// Check the FLV signature
	if(buffer[0] != 'F' || buffer[1] != 'L' || buffer[2] != 'V')
		return YAMDI_INVALID_SIGNATURE;

	// Check the FLV version
	if(FLV_UI8(&buffer[3]) != 1)
		return YAMDI_INVALID_FLVVERSION;

	// Check the DataOffset
	if(FLV_UI32(&buffer[5]) != FLV_SIZE_HEADER)
		return YAMDI_INVALID_DATASIZE;

	// Check the PreviousTagSize0 value
	if(FLV_UI32(&buffer[FLV_SIZE_HEADER]) != 0)
		return YAMDI_INVALID_PREVIOUSTAGSIZE;

	return YAMDI_OK;
}

int initFLV(FLV_t *flv) {
	if(flv == NULL)
		return YAMDI_ERROR;

	memset(flv, 0, sizeof(FLV_t));

	return YAMDI_OK;
}

int indexFLV(FLV_t *flv, FILE *fp) {
	off_t offset;
	size_t nflvtags;
	FLVTag_t flvtag;

#ifdef DEBUG
	fprintf(stderr, "[FLV] indexing file ...\n");
#endif

	// Count how many tags are there in this FLV
	offset = FLV_SIZE_HEADER + FLV_SIZE_PREVIOUSTAGSIZE;
	nflvtags = 0;
	while(readFLVTag(&flvtag, offset, fp) == YAMDI_OK) {
		offset += (flvtag.tagsize + FLV_SIZE_PREVIOUSTAGSIZE);

		nflvtags++;
	}

	flv->index.nflvtags = nflvtags;

#ifdef DEBUG
	fprintf(stderr, "[FLV] nflvtags = %d\n", flv->index.nflvtags);
#endif

	if(nflvtags == 0)
		return YAMDI_OK;

	// Allocate memory for the tag metadata index
	flv->index.flvtag = (FLVTag_t *)calloc(flv->index.nflvtags, sizeof(FLVTag_t));
	if(flv->index.flvtag == NULL)
		return YAMDI_OUT_OF_MEMORY;

	// Store the tag metadata in the index
	offset = FLV_SIZE_HEADER + FLV_SIZE_PREVIOUSTAGSIZE;
	nflvtags = 0;
	while(readFLVTag(&flvtag, offset, fp) == YAMDI_OK) {
		flv->index.flvtag[nflvtags].offset = flvtag.offset;
		flv->index.flvtag[nflvtags].tagtype = flvtag.tagtype;
		flv->index.flvtag[nflvtags].datasize = flvtag.datasize;
		flv->index.flvtag[nflvtags].timestamp = flvtag.timestamp;
		flv->index.flvtag[nflvtags].tagsize = flvtag.tagsize;

		offset += (flv->index.flvtag[nflvtags].tagsize + FLV_SIZE_PREVIOUSTAGSIZE);

		nflvtags++;
#ifdef DEBUG
		if((nflvtags % 100) == 0)
			fprintf(stderr, "[FLV] storing metadata (tag %d of %d)\r", nflvtags, flv->index.nflvtags);
#endif
	}

#ifdef DEBUG
	fprintf(stderr, "[FLV] storing metadata (tag %d of %d)\n", nflvtags, flv->index.nflvtags);
#endif

	return YAMDI_OK;
}

void storeFLVFromStdin(FILE *fp) {
	char buf[4096];
	size_t bytes;

	while((bytes = fread(buf, 1, sizeof(buf), stdin)) > 0)
		fwrite(buf, 1, bytes, fp);

	return;
}

int freeFLV(FLV_t *flv) {
	if(flv->index.nflvtags != 0)
		free(flv->index.flvtag);

	if(flv->keyframes.keyframelocations != NULL)
		free(flv->keyframes.keyframelocations);

	if(flv->keyframes.keyframetimestamps != NULL)
		free(flv->keyframes.keyframetimestamps);

	bufferFree(&flv->onmetadata);
	bufferFree(&flv->onlastsecond);
	bufferFree(&flv->onlastkeyframe);

	memset(flv, 0, sizeof(FLV_t));

	return YAMDI_OK;
}

int analyzeFLV(FLV_t *flv, FILE *fp) {
	int rv;
	size_t i, index;
	unsigned char flags;
	FLVTag_t *flvtag;

#ifdef DEBUG
	fprintf(stderr, "[FLV] analyzing FLV ...\n");
#endif

	for(i = 0; i < flv->index.nflvtags; i++) {
		flvtag = &flv->index.flvtag[i];

		if(flvtag->tagtype == FLV_TAG_AUDIO) {
			flv->hasaudio = 1;

			flv->audio.ntags++;
			flv->audio.datasize += flvtag->datasize;
			flv->audio.size += flvtag->tagsize;

			flv->audio.lasttimestamp = flvtag->timestamp;
			flv->audio.lastframeindex = i;

			readFLVTagData(&flags, 1, flvtag, fp);

			if(flv->audio.analyzed == 0) {
				// SoundFormat
				flv->audio.codecid = (flags >> 4) & 0xf;

				// SoundRate
				flv->audio.samplerate = (flags >> 2) & 0x3;

				// SoundSize
				flv->audio.samplesize = (flags >> 1) & 0x1;

				// SoundType
				flv->audio.stereo = flags & 0x1;

				if(flv->audio.codecid == 4 || flv->audio.codecid == 5 || flv->audio.codecid == 6) {
					// Nellymoser
					flv->audio.stereo = 0;
				}
				else if(flv->audio.codecid == 10) {
					// AAC
					flv->audio.samplerate = 3;
					flv->audio.stereo = 1;
				}

				flv->audio.analyzed = 1;
			}
		}
		else if(flvtag->tagtype == FLV_TAG_VIDEO) {
			flv->hasvideo = 1;

			flv->video.ntags++;
			flv->video.datasize += flvtag->datasize;
			flv->video.size += flvtag->tagsize;

			flv->video.lasttimestamp = flvtag->timestamp;
			flv->video.lastframeindex = i;

			readFLVTagData(&flags, 1, flvtag, fp);

			// Keyframes
			flvtag->keyframe = (flags >> 4) & 0xf;
			if(flvtag->keyframe == 1) {
				flv->canseektoend = 1;
				flv->keyframes.nkeyframes++;
				flv->keyframes.lastkeyframeindex = i;
			}
			else
				flv->canseektoend = 0;

			if(flvtag->keyframe == 1 && flv->video.analyzed == 0) {
				// Video Codec
				flv->video.codecid = flags & 0xf;

				switch(flv->video.codecid) {
					case FLV_PACKET_H263VIDEO:
						rv = analyzeFLVH263VideoPacket(flv, flvtag, fp);
						break;
					case FLV_PACKET_SCREENVIDEO:
						rv = analyzeFLVScreenVideoPacket(flv, flvtag, fp);
						break;
					case FLV_PACKET_VP6VIDEO:
						rv = analyzeFLVVP6VideoPacket(flv, flvtag, fp);
						break;
					case FLV_PACKET_VP6ALPHAVIDEO:
						rv = analyzeFLVVP6AlphaVideoPacket(flv, flvtag, fp);
						break;
					case FLV_PACKET_SCREENV2VIDEO:
						rv = analyzeFLVScreenVideoPacket(flv, flvtag, fp);
						break;
					case FLV_PACKET_H264VIDEO:
						rv = analyzeFLVH264VideoPacket(flv, flvtag, fp);
						break;
					default:
						rv = YAMDI_ERROR;
						break;
				}

				if(rv == YAMDI_OK)
					flv->video.analyzed = 1;
			}
		}

		flv->lasttimestamp = flvtag->timestamp;

#ifdef DEBUG
		if((i % 100) == 0)
			fprintf(stderr, "[FLV] analyzing FLV (tag %d of %d)\r", i, flv->index.nflvtags);
#endif
	}

#ifdef DEBUG
	fprintf(stderr, "[FLV] analyzing FLV (tag %d of %d)\n", i, flv->index.nflvtags);

	fprintf(stderr, "[FLV] lasttimestamp = %d ms\n", flv->lasttimestamp);
#endif

	// Calculate the last second
	if(flv->lasttimestamp >= 1000) {
		flv->lastsecond = flv->lasttimestamp - 1000;
		i = flv->index.nflvtags;
		while(i != 0) {
			flvtag = &flv->index.flvtag[i - 1];

			if(flvtag->timestamp <= flv->lastsecond) {
				flv->lastsecond += 1;
				flv->lastsecondindex = (i - 1);

				break;
			}

			i--;
		}
	}
	else
		flv->options.addonlastsecond = 0;

#ifdef DEBUG
	fprintf(stderr, "[FLV] lastsecond = %d ms\n", flv->lastsecond);
	fprintf(stderr, "[FLV] lastsecondindex = %d\n", flv->lastsecondindex);
#endif

	// Calculate audio datarate
	if(flv->audio.datasize != 0)
		flv->audio.datarate = (double)flv->audio.datasize * 8.0 / 1024.0 / (double)flv->audio.lasttimestamp * 1000.0;

#ifdef DEBUG
	fprintf(stderr, "[FLV] audio.codecid = %d\n", flv->audio.codecid);
	fprintf(stderr, "[FLV] audio.lasttimestamp = %d ms\n", flv->audio.lasttimestamp);
	fprintf(stderr, "[FLV] audio.lastframeindex = %d\n", flv->audio.lastframeindex);
	fprintf(stderr, "[FLV] audio.ntags = %d\n", flv->audio.ntags);
	fprintf(stderr, "[FLV] audio.datasize = %" PRIu64 " kb\n", flv->audio.datasize);
	fprintf(stderr, "[FLV] audio.datarate = %f kbit/s\n", flv->audio.datarate);
#endif

	// Calculate video framerate
	if(flv->video.ntags != 0)
		flv->video.framerate = (double)flv->video.ntags / (double)flv->video.lasttimestamp * 1000.0;

	// Calculate video datarate
	if(flv->video.datasize != 0)
		flv->video.datarate = (double)flv->video.datasize * 8.0 / 1024.0 / (double)flv->lasttimestamp * 1000.0;

#ifdef DEBUG
	fprintf(stderr, "[FLV] video.codecid = %d\n", flv->video.codecid);
	fprintf(stderr, "[FLV] video.lasttimestamp = %d ms\n", flv->video.lasttimestamp);
	fprintf(stderr, "[FLV] video.lastframeindex = %d\n", flv->video.lastframeindex);
	fprintf(stderr, "[FLV] video.ntags = %d\n", flv->video.ntags);
	fprintf(stderr, "[FLV] video.framerate = %f fps\n", flv->video.framerate);
	fprintf(stderr, "[FLV] video.datasize = %" PRIu64 " kb\n", flv->video.datasize);
	fprintf(stderr, "[FLV] video.datarate = %f kbit/s\n", flv->video.datarate);
	fprintf(stderr, "[FLV] video.width = %d\n", flv->video.width);
	fprintf(stderr, "[FLV] video.height = %d\n", flv->video.height);
#endif

	// Calculate datasize
	flv->datasize = flv->audio.size + (flv->audio.ntags * FLV_SIZE_PREVIOUSTAGSIZE) + flv->video.size + (flv->video.ntags * FLV_SIZE_PREVIOUSTAGSIZE);

#ifdef DEBUG
	fprintf(stderr, "[FLV] datasize = %" PRIu64 " kb\n", flv->datasize);
#endif

	// Fake keyframes if we have only audio and the corresponding option has been set
	if(flv->options.addaudiokeyframes == 1 && flv->hasaudio == 1 && flv->hasvideo == 0) {
		// Add a keyframe at least every x milliseconds
		flv->audio.keyframerate = (int)((double)flv->audio.keyframedistance / (double)flv->audio.lasttimestamp * (double)flv->audio.ntags);

		// If every frame is longer than the intervalthen add every frame a keyframe
		if(flv->audio.keyframerate == 0)
			flv->audio.keyframerate = 1;
		else if(flv->audio.keyframerate >= flv->audio.ntags)
			flv->audio.keyframerate = flv->audio.ntags;

#ifdef DEBUG
		fprintf(stderr, "[FLV] audio.keyframerate = %d\n", flv->audio.keyframerate);
#endif

		// Mark all audio tags that should be considered as a keyframe
		index = 0;
		for(i = 0; i < flv->index.nflvtags; i++) {
			flvtag = &flv->index.flvtag[i];

			if(flvtag->tagtype == FLV_TAG_AUDIO) {
				if((index % flv->audio.keyframerate) == 0) {
					flvtag->keyframe = 1;
					flv->keyframes.nkeyframes++;
				}

				index++;
			}
		}

		// Add an extra keyframe for the last frame
		if(flv->index.flvtag[flv->audio.lastframeindex].keyframe == 0) {
			flv->index.flvtag[flv->audio.lastframeindex].keyframe = 1;
			flv->keyframes.nkeyframes++;
		}

		flv->canseektoend = 1;
		flv->keyframes.lastkeyframeindex = flv->audio.lastframeindex;
	}
	else
		flv->options.addaudiokeyframes = 0;

#ifdef DEBUG
	fprintf(stderr, "[FLV] keyframes.nkeyframes = %d\n", flv->keyframes.nkeyframes);
	fprintf(stderr, "[FLV] keyframes.lastkeyframeindex = %d\n", flv->keyframes.lastkeyframeindex);
#endif

	// Allocate some memory for the keyframe index
	if(flv->keyframes.nkeyframes != 0) {
		flv->haskeyframes = 1;

		flv->keyframes.keyframelocations = (off_t *)calloc(flv->keyframes.nkeyframes, sizeof(off_t));
		if(flv->keyframes.keyframelocations == NULL)
			return YAMDI_OUT_OF_MEMORY;

		flv->keyframes.keyframetimestamps = (int *)calloc(flv->keyframes.nkeyframes, sizeof(int));
		if(flv->keyframes.keyframetimestamps == NULL)
			return YAMDI_OUT_OF_MEMORY;
	}

	return YAMDI_OK;
}

int finalizeFLV(FLV_t *flv, FILE *fp) {
	size_t i, index;
	FLVTag_t *flvtag;

	// 2 passes
	// 1. create onmetadata event to get the size of it (it doesn't matter if the values are not yet correct)
	// 2. calculate the new keyframelocations and the final filesize
	//    pay attention to the size of these events
	//        onmetadata
	//        onlastsecond
	//        onlastkeyframe
	//        (oncuepoint) keep them from the input flv?
	// 3. recreate the onmetadata event with the correct values
	// filesize
	// keyframelocations
	// lastkeyframelocation

	// Create the metadata tags. Even though we don't have all values,
	// the size will not change. We need the size.
	createFLVEvents(flv);

	// Start calculating the final filesize
	flv->filesize = 0;

	// FLV header + PreviousTagSize
	flv->filesize += FLV_SIZE_HEADER + FLV_SIZE_PREVIOUSTAGSIZE;

	// onMetaData event
	if(flv->options.addonmetadata == 1)
		flv->filesize += flv->onmetadata.used;

	// Calculate the final filesize and update the keyframe index
	index = 0;
	for(i = 0; i < flv->index.nflvtags; i++) {
		flvtag = &flv->index.flvtag[i];

		// Skip every script tag (subject to change if we want to keep existing events)
		if(flvtag->tagtype != FLV_TAG_AUDIO && flvtag->tagtype != FLV_TAG_VIDEO)
			continue;

		// Take care of the onlastsecond event
		if(flv->options.addonlastsecond == 1 && flv->lastsecondindex == i)
			flv->filesize += flv->onlastsecond.used;

		// Update the keyframe index only if there are keyframes ...
		if(flv->haskeyframes == 1) {
			if(flvtag->tagtype == FLV_TAG_VIDEO || flvtag->tagtype == FLV_TAG_AUDIO) {
				// Keyframes
				if(flvtag->keyframe == 1) {
					// Take care of the onlastkeyframe event
					if(flv->options.addonlastkeyframe == 1 && flv->keyframes.lastkeyframeindex == i)
						flv->filesize += flv->onlastkeyframe.used;

					flv->keyframes.keyframelocations[index] = flv->filesize;
					flv->keyframes.keyframetimestamps[index] = flvtag->timestamp;

					index++;
				}
			}
		}

		flv->filesize += flvtag->tagsize + FLV_SIZE_PREVIOUSTAGSIZE;
	}

	if(flv->haskeyframes == 1) {
		flv->keyframes.lastkeyframetimestamp = flv->keyframes.keyframetimestamps[flv->keyframes.nkeyframes - 1];
		flv->keyframes.lastkeyframelocation = flv->keyframes.keyframelocations[flv->keyframes.nkeyframes - 1];
	}

#ifdef DEBUG
	fprintf(stderr, "[FLV] keyframes.lastkeyframetimestamp = %d ms\n", flv->keyframes.lastkeyframetimestamp);
	fprintf(stderr, "[FLV] keyframes.lastkeyframelocation = %" PRIi64 "\n", flv->keyframes.lastkeyframelocation);

	fprintf(stderr, "[FLV] filesize = %" PRIu64 " kb\n", flv->filesize);
#endif

	// Create the metadata tags with the correct values
	createFLVEvents(flv);

	return YAMDI_OK;
}

int writeFLV(FILE *out, FLV_t *flv, FILE *fp) {
	size_t i, datasize = 0;
	unsigned char *data = NULL, *d;
	FLVTag_t *flvtag;

	if(fp == NULL)
		return YAMDI_ERROR;

	// Write the header
	writeFLVHeader(out, flv->hasaudio, flv->hasvideo);
	writeFLVPreviousTagSize(out, 0);

	// Write the onMetaData tag
	if(flv->options.addonmetadata == 1)
		fwrite(flv->onmetadata.data, flv->onmetadata.used, 1, out);

	// Copy the audio and video tags
	for(i = 0; i < flv->index.nflvtags; i++) {
		flvtag = &flv->index.flvtag[i];

		// Skip every script tag (subject to change if we want to keep existing events)
		if(flvtag->tagtype != FLV_TAG_AUDIO && flvtag->tagtype != FLV_TAG_VIDEO)
			continue;

		// Write the onlastsecond event
		if(flv->options.addonlastsecond == 1 && flv->lastsecondindex == i)
			fwrite(flv->onlastsecond.data, flv->onlastsecond.used, 1, out);

		// Write the onlastkeyframe event
		if(flv->options.addonlastkeyframe == 1 && flv->keyframes.lastkeyframeindex == i)
			fwrite(flv->onlastkeyframe.data, flv->onlastkeyframe.used, 1, out);

		writeFLVDataTag(out, flvtag->tagtype, flvtag->timestamp, flvtag->datasize);

		// Read the data
		if(flvtag->datasize > datasize) {
			d = (unsigned char *)realloc(data, flvtag->datasize);
			if(d == NULL)
				return YAMDI_OUT_OF_MEMORY;

			data = d;
			datasize = flvtag->datasize;
		}

		if(readFLVTagData(data, flvtag->datasize, flvtag, fp) != YAMDI_OK)
			return YAMDI_READ_ERROR;

		fwrite(data, flvtag->datasize, 1, out);

		writeFLVPreviousTagSize(out, flvtag->tagsize);
	}

	if(data != NULL)
		free(data);

	// We are done!

	return YAMDI_OK;
}

int writeFLVHeader(FILE *fp, int hasaudio, int hasvideo) {
	unsigned char bytes[FLV_SIZE_HEADER];

	// Signature
	bytes[0] = 'F';
	bytes[1] = 'L';
	bytes[2] = 'V';

	// Version
	bytes[3] = 1;

	// Flags
	bytes[4] = 0;

	if(hasaudio == 1)
		bytes[4] |= 0x4;

	if(hasvideo == 1)
		bytes[4] |= 0x1;

	// DataOffset
	bytes[5] = ((FLV_SIZE_HEADER >> 24) & 0xff);
	bytes[6] = ((FLV_SIZE_HEADER >> 16) & 0xff);
	bytes[7] = ((FLV_SIZE_HEADER >>  8) & 0xff);
	bytes[8] = ((FLV_SIZE_HEADER >>  0) & 0xff);

	fwrite(bytes, FLV_SIZE_HEADER, 1, fp);

	return YAMDI_OK;
}

int createFLVEvents(FLV_t *flv) {
	if(flv->options.addonmetadata == 1)
		createFLVEventOnMetaData(flv);

	if(flv->options.addonlastkeyframe == 1)
		createFLVEventOnLastKeyframe(flv);

	if(flv->options.addonlastsecond == 1)
		createFLVEventOnLastSecond(flv);

	return YAMDI_OK;
}

int createFLVEventOnMetaData(FLV_t *flv) {
	int pass = 0;
	size_t i, length = 0;
	buffer_t b;

	bufferInit(&b);

onmetadatapass:
	bufferReset(&b);

	// ScriptDataObject
	writeBufferFLVScriptDataObject(&b);

	writeBufferFLVScriptDataECMAArray(&b, "onMetaData", length);

	length = 0;

	if(strlen(flv->options.creator) != 0) {
		writeBufferFLVScriptDataValueString(&b, "creator", flv->options.creator); length++;
	}

	writeBufferFLVScriptDataValueString(&b, "metadatacreator", "Yet Another Metadata Injector for FLV - Version " YAMDI_VERSION "\0"); length++;
	writeBufferFLVScriptDataValueBool(&b, "hasKeyframes", flv->haskeyframes); length++;
	writeBufferFLVScriptDataValueBool(&b, "hasVideo", flv->hasvideo); length++;
	writeBufferFLVScriptDataValueBool(&b, "hasAudio", flv->hasaudio); length++;
	writeBufferFLVScriptDataValueBool(&b, "hasMetadata", 1); length++;
	writeBufferFLVScriptDataValueBool(&b, "canSeekToEnd", flv->canseektoend); length++;

	writeBufferFLVScriptDataValueDouble(&b, "duration", (double)flv->lasttimestamp / 1000.0); length++;
	writeBufferFLVScriptDataValueDouble(&b, "datasize", (double)flv->datasize); length++;

	if(flv->hasvideo == 1) {
		writeBufferFLVScriptDataValueDouble(&b, "videosize", (double)flv->video.size); length++;
		writeBufferFLVScriptDataValueDouble(&b, "framerate", (double)flv->video.framerate); length++;
		writeBufferFLVScriptDataValueDouble(&b, "videodatarate", (double)flv->video.datarate); length++;

		if(flv->video.analyzed == 1) {
			writeBufferFLVScriptDataValueDouble(&b, "videocodecid", (double)flv->video.codecid); length++;
			writeBufferFLVScriptDataValueDouble(&b, "width", (double)flv->video.width); length++;
			writeBufferFLVScriptDataValueDouble(&b, "height", (double)flv->video.height); length++;
		}
	}

	if(flv->hasaudio == 1) {
		writeBufferFLVScriptDataValueDouble(&b, "audiosize", (double)flv->audio.size); length++;
		writeBufferFLVScriptDataValueDouble(&b, "audiodatarate", (double)flv->audio.datarate); length++;

		if(flv->audio.analyzed == 1) {
			writeBufferFLVScriptDataValueDouble(&b, "audiocodecid", (double)flv->audio.codecid); length++;
			writeBufferFLVScriptDataValueDouble(&b, "audiosamplerate", (double)flv->audio.samplerate); length++;
			writeBufferFLVScriptDataValueDouble(&b, "audiosamplesize", (double)flv->audio.samplesize); length++;
			writeBufferFLVScriptDataValueBool(&b, "stereo", flv->audio.stereo); length++;
		}
	}

	writeBufferFLVScriptDataValueDouble(&b, "filesize", (double)flv->filesize); length++;
	writeBufferFLVScriptDataValueDouble(&b, "lasttimestamp", (double)flv->lasttimestamp / 1000.0); length++;

	if(flv->haskeyframes == 1) {
		writeBufferFLVScriptDataValueDouble(&b, "lastkeyframetimestamp", (double)flv->keyframes.lastkeyframetimestamp / 1000.0); length++;
		writeBufferFLVScriptDataValueDouble(&b, "lastkeyframelocation", (double)flv->keyframes.lastkeyframelocation); length++;

		writeBufferFLVScriptDataVariableArray(&b, "keyframes"); length++;

			writeBufferFLVScriptDataValueArray(&b, "filepositions", flv->keyframes.nkeyframes);

			for(i = 0; i < flv->keyframes.nkeyframes; i++)
				writeBufferFLVScriptDataValueDouble(&b, NULL, (double)flv->keyframes.keyframelocations[i]);

			writeBufferFLVScriptDataValueArray(&b, "times", flv->keyframes.nkeyframes);

			for(i = 0; i < flv->keyframes.nkeyframes; i++)
				writeBufferFLVScriptDataValueDouble(&b, NULL, (double)flv->keyframes.keyframetimestamps[i] / 1000.0);

		writeBufferFLVScriptDataVariableArrayEnd(&b);
	}

	writeBufferFLVScriptDataVariableArrayEnd(&b);

	if(pass == 0) {
		pass = 1;
		goto onmetadatapass;
	}

	// Write the onMetaData tag
	bufferReset(&flv->onmetadata);

	writeBufferFLVScriptDataTag(&flv->onmetadata, 0, b.used);
	bufferAppendBuffer(&flv->onmetadata, &b);
	writeBufferFLVPreviousTagSize(&flv->onmetadata, flv->onmetadata.used);

	bufferFree(&b);

	return YAMDI_OK;
}

int createFLVEventOnLastSecond(FLV_t *flv) {
	buffer_t b;

	bufferInit(&b);

	// ScriptDataObject
	writeBufferFLVScriptDataObject(&b);

	writeBufferFLVScriptDataECMAArray(&b, "onLastSecond", 0);
	writeBufferFLVScriptDataVariableArrayEnd(&b);

	// Write the onLastSecond tag
	bufferReset(&flv->onlastsecond);

	writeBufferFLVScriptDataTag(&flv->onlastsecond, flv->lastsecond, b.used);
	bufferAppendBuffer(&flv->onlastsecond, &b);
	writeBufferFLVPreviousTagSize(&flv->onlastsecond, flv->onlastsecond.used);

	bufferFree(&b);

	return YAMDI_OK;
}

int createFLVEventOnLastKeyframe(FLV_t *flv) {
	buffer_t b;

	bufferInit(&b);

	// ScriptDataObject
	writeBufferFLVScriptDataObject(&b);

	writeBufferFLVScriptDataECMAArray(&b, "onLastKeyframe", 0);
	writeBufferFLVScriptDataVariableArrayEnd(&b);

	// Write the onLastKeyframe tag
	bufferReset(&flv->onlastkeyframe);

	writeBufferFLVScriptDataTag(&flv->onlastkeyframe, flv->keyframes.lastkeyframetimestamp - 1, b.used);
	bufferAppendBuffer(&flv->onlastkeyframe, &b);
	writeBufferFLVPreviousTagSize(&flv->onlastkeyframe, flv->onlastkeyframe.used);

	bufferFree(&b);

	return YAMDI_OK;
}

int writeBufferFLVScriptDataTag(buffer_t *buffer, int timestamp, size_t datasize) {
	unsigned char bytes[FLV_SIZE_TAGHEADER];

	bytes[ 0] = FLV_TAG_SCRIPTDATA;

	// DataSize
	bytes[ 1] = ((datasize >> 16) & 0xff);
	bytes[ 2] = ((datasize >>  8) & 0xff);
	bytes[ 3] = ((datasize >>  0) & 0xff);

	// Timestamp
	bytes[ 4] = ((timestamp >> 16) & 0xff);
	bytes[ 5] = ((timestamp >>  8) & 0xff);
	bytes[ 6] = ((timestamp >>  0) & 0xff);

	// TimestampExtended
	bytes[ 7] = ((timestamp >> 24) & 0xff);

	// StreamID
	bytes[ 8] = 0;
	bytes[ 9] = 0;
	bytes[10] = 0;

	bufferAppendBytes(buffer, bytes, FLV_SIZE_TAGHEADER);

	return YAMDI_OK;
}

int writeFLVDataTag(FILE *fp, int type, int timestamp, size_t datasize) {
	unsigned char bytes[FLV_SIZE_TAGHEADER];

	bytes[ 0] = type;

	// DataSize
	bytes[ 1] = ((datasize >> 16) & 0xff);
	bytes[ 2] = ((datasize >>  8) & 0xff);
	bytes[ 3] = ((datasize >>  0) & 0xff);

	// Timestamp
	bytes[ 4] = ((timestamp >> 16) & 0xff);
	bytes[ 5] = ((timestamp >>  8) & 0xff);
	bytes[ 6] = ((timestamp >>  0) & 0xff);

	// TimestampExtended
	bytes[ 7] = ((timestamp >> 24) & 0xff);

	// StreamID
	bytes[ 8] = 0;
	bytes[ 9] = 0;
	bytes[10] = 0;

	fwrite(bytes, FLV_SIZE_TAGHEADER, 1, fp);

	return YAMDI_OK;
}

int writeBufferFLVPreviousTagSize(buffer_t *buffer, size_t tagsize) {
	unsigned char bytes[4];

	bytes[0] = ((tagsize >> 24) & 0xff);
	bytes[1] = ((tagsize >> 16) & 0xff);
	bytes[2] = ((tagsize >>  8) & 0xff);
	bytes[3] = ((tagsize >>  0) & 0xff);

	bufferAppendBytes(buffer, bytes, 4);

	return YAMDI_OK;
}

int writeFLVPreviousTagSize(FILE *fp, size_t tagsize) {
	unsigned char bytes[4];

	bytes[0] = ((tagsize >> 24) & 0xff);
	bytes[1] = ((tagsize >> 16) & 0xff);
	bytes[2] = ((tagsize >>  8) & 0xff);
	bytes[3] = ((tagsize >>  0) & 0xff);

	fwrite(bytes, 4, 1, fp);

	return YAMDI_OK;
}

int analyzeFLVH263VideoPacket(FLV_t *flv, FLVTag_t *flvtag, FILE *fp) {
	int startcode, picturesize;
	unsigned char *buffer, data[10];

	readFLVTagData(data, sizeof(data), flvtag, fp);
	// Skip the VIDEODATA header
	buffer = &data[1];

	// 8bit  |pppppppp|pppppppp|pvvvvvrr|rrrrrrss|swwwwwww|whhhhhhh|h
	// 16bit |pppppppp|pppppppp|pvvvvvrr|rrrrrrss|swwwwwww|wwwwwwww|whhhhhhh|hhhhhhhh|h

	startcode = FLV_UI24(buffer) >> 7;
	if(startcode != 1)
		return YAMDI_ERROR;

	picturesize = ((buffer[3] & 0x3) << 1) + ((buffer[4] >> 7) & 0x1);

	switch(picturesize) {
		case 0: // Custom 8bit
			flv->video.width = ((buffer[4] & 0x7f) << 1) + ((buffer[5] >> 7) & 0x1);
			flv->video.height = ((buffer[5] & 0x7f) << 1) + ((buffer[6] >> 7) & 0x1);
			break;
		case 1: // Custom 16bit
			flv->video.width = ((buffer[4] & 0x7f) << 9) + (buffer[5] << 1) + ((buffer[6] >> 7) & 0x1);
			flv->video.height = ((buffer[6] & 0x7f) << 9) + (buffer[7] << 1) + ((buffer[8] >> 7) & 0x1);
			break;
		case 2: // CIF
			flv->video.width = 352.0;
			flv->video.height = 288.0;
			break;
		case 3: // QCIF
			flv->video.width = 176.0;
			flv->video.height = 144.0;
			break;
		case 4: // SQCIF
			flv->video.width = 128.0;
			flv->video.height = 96.0;
			break;
		case 5:
			flv->video.width = 320.0;
			flv->video.height = 240.0;
			break;
		case 6:
			flv->video.width = 160.0;
			flv->video.height = 120.0;
			break;
		default:
			break;
	}

	return YAMDI_OK;
}

int analyzeFLVScreenVideoPacket(FLV_t *flv, FLVTag_t *flvtag, FILE *fp) {
	unsigned char *buffer, data[5];

	// |1111wwww|wwwwwwww|2222hhhh|hhhhhhhh|

	readFLVTagData(data, sizeof(data), flvtag, fp);
	// Skip the VIDEODATA header
	buffer = &data[1];

	flv->video.width = ((buffer[0] & 0xf) << 8) + buffer[1];
	flv->video.height = ((buffer[2] & 0xf) << 8) + buffer[3];

	return YAMDI_OK;
}

int analyzeFLVVP6VideoPacket(FLV_t *flv, FLVTag_t *flvtag, FILE *fp) {
	int offset = 3;	// default buffer offset for dim_y
	unsigned char *buffer, data[10];

	readFLVTagData(data, sizeof(data), flvtag, fp);

#if DEBUG
	fprintf(stderr, "\n");

	fprintf(stderr, "[VP6] ");
	int i = 0;
	for(i = 0; i < sizeof(data); i++)
		fprintf(stderr, "%d=%d ", i, data[i]);
	fprintf(stderr, "\n");
#endif

	// Skip the VIDEODATA header
	buffer = &data[1];

	// VP6FLVVIDEOPACKET header (as described in the SWF Specs v10, page 249)
	int hadjust = ((buffer[0] >> 4) & 0x0f);
	int vadjust = (buffer[0] & 0x0f);

	// Raw vp6 video data (http://wiki.multimedia.cx/index.php?title=On2_VP6)
	int frame_mode = ((buffer[1] >> 7) & 0x01);

	if(frame_mode != 0)	// We need an iframe
		return YAMDI_ERROR;

	int marker = (buffer[1] & 0x01);	// Without checking this value, we support all VP6 variants

	int version2 = ((buffer[2] >> 1) & 0x03);

#if DEBUG
	int version = ((buffer[2] >> 3) & 0x1f);
	int interlace = (buffer[2] & 0x01);

	fprintf(stderr, "[VP6] marker = %d\n", marker);
	fprintf(stderr, "[VP6] version = %d\n", version);
	fprintf(stderr, "[VP6] version2 = %d\n", version2);
	fprintf(stderr, "[VP6] interlace = %d\n", interlace);
#endif

	// In these cases there are 2 more bytes that we have to skip
	if(marker == 1 || version2 == 0)
		offset += 2;

#if DEBUG
	fprintf(stderr, "[VP6] offset = %d\n", offset);
	fprintf(stderr, "[VP6] dim_y = %d\n", buffer[offset]);
	fprintf(stderr, "[VP6] dim_x = %d\n", buffer[offset + 1]);
	fprintf(stderr, "[VP6] render_y = %d\n", buffer[offset + 2]);
	fprintf(stderr, "[VP6] render_x = %d\n", buffer[offset + 3]);
#endif

	// Now offset points to the resolution values: [dim_y, dim_x, render_y, render_x]
	// Values represent macroblocks
	flv->video.height = (buffer[offset + 0] * 16) - vadjust;
	flv->video.width = (buffer[offset + 1] * 16) - hadjust;

	return YAMDI_OK;
}

int analyzeFLVVP6AlphaVideoPacket(FLV_t *flv, FLVTag_t *flvtag, FILE *fp) {
	unsigned char *buffer, data[9];

	readFLVTagData(data, sizeof(data), flvtag, fp);
	// Skip the VIDEODATA header
	buffer = &data[1];

	flv->video.width = buffer[7] * 16 - (buffer[0] >> 4);
	flv->video.height = buffer[6] * 16 - (buffer[0] & 0x0f);

	return YAMDI_OK;
}

int analyzeFLVH264VideoPacket(FLV_t *flv, FLVTag_t *flvtag, FILE *fp) {
	int avcpackettype;
	int i, length, offset, nSPS;
	unsigned char *avcc;
	unsigned char *buffer, data[flvtag->datasize];
	h264data_t h264data;

	readFLVTagData(data, sizeof(data), flvtag, fp);
	// Skip the VIDEODATA header
	buffer = &data[1];

	avcpackettype = buffer[0];

#ifdef DEBUG
	fprintf(stderr, "[FLV] AVCPacketType = %d\n", avcpackettype);
#endif

	if(avcpackettype != 0)
		return YAMDI_ERROR;

	// AVCDecoderConfigurationRecord (14496-15, 5.2.4.1.1)
	avcc = (unsigned char *)&buffer[4];

	nSPS = avcc[5] & 0x1f;

#ifdef DEBUG
	fprintf(stderr, "[AVC/H.264] AVCDecoderConfigurationRecord\n");
	fprintf(stderr, "[AVC/H.264] configurationVersion = %d\n", avcc[0]);
	fprintf(stderr, "[AVC/H.264] AVCProfileIndication = %d\n", avcc[1]);
	fprintf(stderr, "[AVC/H.264] profile_compatibility = %d\n", avcc[2]);
	fprintf(stderr, "[AVC/H.264] AVCLevelIndication = %d\n", avcc[3]);
	fprintf(stderr, "[AVC/H.264] lengthSizeMinusOne = %d\n", avcc[4] & 0x3);
	fprintf(stderr, "[AVC/H.264] numOfSequenceParameterSets = %d\n", nSPS);
#endif

	offset = 6;
	for(i = 0; i < nSPS; i++) {
		length = (avcc[offset] << 8) + avcc[offset + 1];
#ifdef DEBUG
		fprintf(stderr, "[AVC/H.264]\tsequenceParameterSetLength = %d bit\n", 8 * length);
#endif
		memset(&h264data, 0, sizeof(h264data_t));
		if(readH264NALUnit(&h264data, &avcc[offset + 2], length) == YAMDI_OK)
			break;

		offset += (2 + length);
	}

	// There would be some Picture Parameter Sets, but we don't need them. Bail out.
/*
	int nPPS = avcc[offset++];
	fprintf(stderr, "numOfPictureParameterSets = %d\n", nPPS);

	for(i = 0; i < nPPS; i++) {
		length = (avcc[offset] << 8) + avcc[offset + 1];
#ifdef DEBUG
		fprintf(stderr, "[AVC/H.264]\tpictureParameterSetLength = %d bit\n", 8 * length);
#endif
		readH264NALUnit(&avcc[offset + 2], length);

		offset += (2 + length);
	}
*/

	if(h264data.valid == 0)
		return YAMDI_ERROR;

	flv->video.width = h264data.width;
	flv->video.height = h264data.height;

	return YAMDI_OK;
}

int writeBufferFLVScriptDataObject(buffer_t *buffer) {
	unsigned char type = 2;

	bufferAppendBytes(buffer, &type, 1);

	return YAMDI_OK;
}

int writeBufferFLVScriptDataECMAArray(buffer_t *buffer, const char *name, size_t len) {
	unsigned char type, bytes[4];

	writeBufferFLVScriptDataString(buffer, name);

	type = 8;	// ECMAArray
	bufferAppendBytes(buffer, &type, 1);

	bytes[0] = ((len >> 24) & 0xff);
	bytes[1] = ((len >> 16) & 0xff);
	bytes[2] = ((len >>  8) & 0xff);
	bytes[3] = ((len >>  0) & 0xff);

	bufferAppendBytes(buffer, bytes, 4);

	return YAMDI_OK;
}

int writeBufferFLVScriptDataValueArray(buffer_t *buffer, const char *name, size_t len) {
	unsigned char type, bytes[4];
	
	writeBufferFLVScriptDataString(buffer, name);

	type = 10;	// Value Array
	bufferAppendBytes(buffer, &type, 1);

	bytes[0] = ((len >> 24) & 0xff);
	bytes[1] = ((len >> 16) & 0xff);
	bytes[2] = ((len >>  8) & 0xff);
	bytes[3] = ((len >>  0) & 0xff);

	bufferAppendBytes(buffer, bytes, 4);

	return YAMDI_OK;
}

int writeBufferFLVScriptDataVariableArray(buffer_t *buffer, const char *name) {
	unsigned char type;

	writeBufferFLVScriptDataString(buffer, name);

	type = 3;	// Variable Array
	bufferAppendBytes(buffer, &type, 1);

	return YAMDI_OK;
}

int writeBufferFLVScriptDataVariableArrayEnd(buffer_t *buffer) {
	unsigned char bytes[3];

	bytes[0] = 0;
	bytes[1] = 0;
	bytes[2] = 9;

	bufferAppendBytes(buffer, bytes, 3);

	return YAMDI_OK;
}

int writeBufferFLVScriptDataValueString(buffer_t *buffer, const char *name, const char *value) {
	unsigned char type;

	if(name != NULL)
		writeBufferFLVScriptDataString(buffer, name);

	type = 2;	// DataString
	bufferAppendBytes(buffer, &type, 1);

	writeBufferFLVScriptDataString(buffer, value);

	return YAMDI_OK;
}

int writeBufferFLVScriptDataValueBool(buffer_t *buffer, const char *name, int value) {
	unsigned char type;

	if(name != NULL)
		writeBufferFLVScriptDataString(buffer, name);

	type = 1;	// Bool
	bufferAppendBytes(buffer, &type, 1);

	writeBufferFLVBool(buffer, value);

	return YAMDI_OK;
}

int writeBufferFLVScriptDataValueDouble(buffer_t *buffer, const char *name, double value) {
	unsigned char type;

	if(name != NULL)
		writeBufferFLVScriptDataString(buffer, name);

	type = 0;	// Double
	bufferAppendBytes(buffer, &type, 1);

	writeBufferFLVDouble(buffer, value);

	return YAMDI_OK;
}

int writeBufferFLVScriptDataString(buffer_t *buffer, const char *s) {
	size_t len;
	unsigned char bytes[2];

	len = strlen(s);

	// critical, if only DataString is expected?
	if(len > 0xffff)
		writeBufferFLVScriptDataLongString(buffer, s);
	else {
		bytes[0] = ((len >> 8) & 0xff);
		bytes[1] = ((len >> 0) & 0xff);

		bufferAppendBytes(buffer, bytes, 2);
		bufferAppendString(buffer, (unsigned char*)s);
	}

	return YAMDI_OK;
}

int writeBufferFLVScriptDataLongString(buffer_t *buffer, const char *s) {
	size_t len;
	unsigned char bytes[4];

	len = strlen(s);

	if(len > 0xffffffff)
		len = 0xffffffff;

	bytes[0] = ((len >> 24) & 0xff);
	bytes[1] = ((len >> 16) & 0xff);
	bytes[2] = ((len >>  8) & 0xff);
	bytes[3] = ((len >>  0) & 0xff);

	bufferAppendBytes(buffer, bytes, 4);
	bufferAppendString(buffer, (unsigned char *)s);

	return YAMDI_OK;
}

int writeBufferFLVBool(buffer_t *buffer, int value) {
	unsigned char b;

	b = (value & 1);

	bufferAppendBytes(buffer, &b, 1);

	return YAMDI_OK;
}

int writeBufferFLVDouble(buffer_t *buffer, double value) {
	union {
		unsigned char dc[8];
		double dd;
	} d;
	unsigned char b[8];

	d.dd = value;

	if(isBigEndian()) {
		b[0] = d.dc[0];
		b[1] = d.dc[1];
		b[2] = d.dc[2];
		b[3] = d.dc[3];
		b[4] = d.dc[4];
		b[5] = d.dc[5];
		b[6] = d.dc[6];
		b[7] = d.dc[7];
	}
	else {
		b[0] = d.dc[7];
		b[1] = d.dc[6];
		b[2] = d.dc[5];
		b[3] = d.dc[4];
		b[4] = d.dc[3];
		b[5] = d.dc[2];
		b[6] = d.dc[1];
		b[7] = d.dc[0];
	}

	bufferAppendBytes(buffer, b, 8);

	return YAMDI_OK;
}

int readFLVTag(FLVTag_t *flvtag, off_t offset, FILE *fp) {
	int rv;
	unsigned char buffer[FLV_SIZE_TAGHEADER];

	memset(flvtag, 0, sizeof(FLVTag_t));

	rv = fseeko(fp, offset, SEEK_SET);
	if(rv != 0) {
#ifdef DEBUG
		fprintf(stderr, "[FLV] %s\n", strerror(errno));
#endif
		return YAMDI_READ_ERROR;
	}

	flvtag->offset = offset;

	// Read the header
	if(readBytes(buffer, FLV_SIZE_TAGHEADER, fp) != YAMDI_OK)
		return YAMDI_READ_ERROR;

	flvtag->tagtype = FLV_UI8(buffer);

	// Assuming only known tags. Otherwise we only process the
	// input file up to this point. It is not possible to detect
	// where the next valid tag could be.
	switch(flvtag->tagtype) {
		case FLV_TAG_VIDEO:
		case FLV_TAG_AUDIO:
		case FLV_TAG_SCRIPTDATA:
			break;
		default:
			return YAMDI_INVALID_TAGTYPE;
	}

	flvtag->datasize = (size_t)FLV_UI24(&buffer[1]);
	flvtag->timestamp = FLV_TIMESTAMP(&buffer[4]);

	// Skip the data
	readBytes(NULL, flvtag->datasize, fp);

	// Read the previous tag size
	readBytes(buffer, FLV_SIZE_PREVIOUSTAGSIZE, fp);

	// Check the previous tag size
	// This is too picky. We don't need it.
/*
	if(FLV_UI32(buffer) != (FLV_SIZE_TAGHEADER + flvtag->datasize))
		return YAMDI_INVALID_PREVIOUSTAGSIZE;
*/

	flvtag->tagsize = FLV_SIZE_TAGHEADER + flvtag->datasize;

	return YAMDI_OK;
}

int readFLVTagData(unsigned char *ptr, size_t size, FLVTag_t *flvtag, FILE *stream) {
	// check for size <= flvtag->datasize?

	fseeko(stream, flvtag->offset + FLV_SIZE_TAGHEADER, SEEK_SET);

	return readBytes(ptr, size, stream);
}

int readBytes(unsigned char *ptr, size_t size, FILE *stream) {
	size_t bytesread;

	if(ptr == NULL) {
		fseeko(stream, (off_t)size, SEEK_CUR);
		return YAMDI_OK;
	}

	bytesread = fread(ptr, 1, size, stream);
	if(bytesread < size)
		return YAMDI_READ_ERROR;

	return YAMDI_OK;
}

int readH264NALUnit(h264data_t * h264data, unsigned char *nalu, int length) {
	int i, numBytesInRBSP;
	int nal_unit_type;
	bitstream_t bitstream;

	// See 14496-10, 7.3.1
#ifdef DEBUG
	fprintf(stderr, "[AVC/H.264]\tNALU Header: %02x\n", nalu[0]);
	fprintf(stderr, "[AVC/H.264]\t\tforbidden_zero_bit = %d\n", (nalu[0] >> 7) & 0x1);
	fprintf(stderr, "[AVC/H.264]\t\tnal_ref_idc = %d\n", (nalu[0] >> 5) & 0x3);
	fprintf(stderr, "[AVC/H.264]\t\tnal_unit_type = %d\n", nalu[0] & 0x1f);
	fprintf(stderr, "[AVC/H.264]\tRBSP: ");
	for(i = 1; i < length; i++)
		fprintf(stderr, "%02x ", nalu[i]);
	fprintf(stderr, "\n");
#endif

	nal_unit_type = nalu[0] & 0x1f;

	// We are only interested in NALUnits of type 7 (sequence parameter set, SPS)
	if(nal_unit_type != 7)
		return YAMDI_H264_USELESS_NALU;

	bitstream.bytes = (unsigned char *)calloc(1, length - 1);

	numBytesInRBSP = 0;
	for(i = 1; i < length; i++) {
		if(i + 2 < length && nalu[i] == 0x00 && nalu[i + 1] == 0x00 && nalu[i + 2] == 0x03) {
			bitstream.bytes[numBytesInRBSP++] = nalu[i];
			bitstream.bytes[numBytesInRBSP++] = nalu[i + 1];

			i += 2;
		}
		else
			bitstream.bytes[numBytesInRBSP++] = nalu[i];
	}

	bitstream.length = numBytesInRBSP;
	bitstream.byte = 0;
	bitstream.bit = 0;

#ifdef DEBUG
	fprintf(stderr, "[AVC/H.264]\tSODB: ");
	for(i = 0; i < bitstream.length; i++)
		fprintf(stderr, "%02x ", bitstream.bytes[i]);
	fprintf(stderr, "\n");
#endif

	readH264SPS(h264data, &bitstream);

	free(bitstream.bytes);

	return YAMDI_OK;
}

void readH264SPS(h264data_t *h264data, bitstream_t *bitstream) {
	int i, j;
	unsigned int profile_idc;
	unsigned int chroma_format_idc = 1, separate_color_plane_flag = 0;

	unsigned int pic_width_in_mbs_minus1, pic_height_in_map_units_minus1;
	unsigned int frame_mbs_only_flag;
	unsigned int frame_cropping_flag;
	unsigned int frame_crop_left_offset = 0, frame_crop_right_offset = 0, frame_crop_top_offset = 0, frame_crop_bottom_offset = 0;

	unsigned int chromaArrayType;
/*
	We need these values from SPS

	chroma_format_idc
	separate_color_plane_flag
	pic_width_in_mbs_minus1
	pic_height_in_map_units_minus1
	frame_mbs_only_flag
	frame_cropping_flag
	frame_crop_left_offset
	frame_crop_right_offset
	frame_crop_top_offset
	frame_crop_bottom_offset
*/

	profile_idc = readCodedU(bitstream, 8, "profile_idc");
	readCodedU(bitstream, 1, "constraint_set0_flag");
	readCodedU(bitstream, 1, "constraint_set1_flag");
	readCodedU(bitstream, 1, "constraint_set2_flag");
	readCodedU(bitstream, 1, "constraint_set3_flag");
	readCodedU(bitstream, 4, "reserved_zero_4bits");
	readCodedU(bitstream, 8, "level_idc");
	readCodedUE(bitstream, "seq_parameter_set_id");

	if(
		profile_idc == 100 ||
		profile_idc == 110 ||
		profile_idc == 122 ||
		profile_idc == 244 ||
		profile_idc == 44 ||
		profile_idc == 83 ||
		profile_idc == 86
	) {
		chroma_format_idc = readCodedUE(bitstream, "chroma_format_idc");

		if(chroma_format_idc == 3)
			separate_color_plane_flag = readCodedU(bitstream, 1, "separate_color_plane_flag");

		readCodedUE(bitstream, "bit_depth_luma_minus8");
		readCodedUE(bitstream, "bit_depth_chroma_minus8");
		readCodedU(bitstream, 1, "qpprime_y_zero_transform_bypass_flag");

		unsigned int seq_scaling_matrix_present_flag = readCodedU(bitstream, 1, "seq_scaling_matrix_present_flag");
		if(seq_scaling_matrix_present_flag == 1) {
			int sizeOfScalingList, delta_scale, lastScale, nextScale;

			int seq_scaling_matrix_count = (chroma_format_idc != 3) ? 8 : 12;
			unsigned int seq_scaling_list_present_flag;
			for(i = 0; i < seq_scaling_matrix_count; i++) {
				seq_scaling_list_present_flag = readCodedU(bitstream, 1, "seq_scaling_list_present_flag");

				if(seq_scaling_list_present_flag == 1) {
					sizeOfScalingList = (i < 6) ? 16 : 64;
					lastScale = nextScale = 8;
					for(j = 0; j < sizeOfScalingList; j++) {
						if(nextScale != 0) {
							delta_scale = readCodedSE(bitstream, "delta_scale");

							nextScale = (lastScale + delta_scale + 256) % 256;
						}

						lastScale = (nextScale == 0) ? lastScale : nextScale;
					}
				}
			}
		}
	}

	readCodedUE(bitstream, "log2_max_frame_num_minus4");
	unsigned int pic_order_cnt_type = readCodedUE(bitstream, "pic_order_cnt_type");

	if(pic_order_cnt_type == 1) {
		readCodedU(bitstream, 1, "delta_pic_order_always_zero_flag");
		readCodedSE(bitstream, "offset_for_non_ref_pic");
		readCodedSE(bitstream, "offset_for_top_to_bottom_field");

		unsigned int num_ref_frames_in_pic_order_cnt_cycle = readCodedUE(bitstream, "num_ref_frames_in_pic_order_cnt_cycle");
		for(i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++)
			readCodedSE(bitstream, "offset_for_ref_frame");
	}
	else if(pic_order_cnt_type == 0)
		readCodedUE(bitstream, "log2_max_pic_order_cnt_lsb_minus4");

	readCodedUE(bitstream, "max_num_ref_frames");
	readCodedU(bitstream, 1, "gaps_in_frame_num_value_allowed_flag");

	pic_width_in_mbs_minus1 = readCodedUE(bitstream, "pic_width_in_mbs_minus1");
	pic_height_in_map_units_minus1 = readCodedUE(bitstream, "pic_height_in_map_units_minus1");

	frame_mbs_only_flag = readCodedU(bitstream, 1, "frame_mbs_only_flag");
	if(frame_mbs_only_flag == 0)
		readCodedU(bitstream, 1, "mb_adaptive_frame_field_flag");

	readCodedU(bitstream, 1, "direct_8x8_inference_flag");

	frame_cropping_flag = readCodedU(bitstream, 1, "frame_cropping_flag");
	if(frame_cropping_flag == 1) {
		frame_crop_left_offset = readCodedUE(bitstream, "frame_crop_left_offset");
		frame_crop_right_offset = readCodedUE(bitstream, "frame_crop_right_offset");
		frame_crop_top_offset = readCodedUE(bitstream, "frame_crop_top_offset");
		frame_crop_bottom_offset = readCodedUE(bitstream, "frame_crop_bottom_offset");
	}

	readCodedU(bitstream, 1, "vui_parameters_present_flag");

	// and so on ... VUI is not interesting for us. We have everything we need.

	// Now we have enough information to compute the width and height of this video stream

	unsigned int picWidthInMbs = (pic_width_in_mbs_minus1 + 1);
	unsigned int picHeightInMapUnits = (pic_height_in_map_units_minus1 + 1);
	unsigned int frameHeightInMbs = (2 - frame_mbs_only_flag) * picHeightInMapUnits;

	unsigned int width = picWidthInMbs * 16;
	unsigned int height = frameHeightInMbs * 16;

#ifdef DEBUG
	fprintf(stderr, "[AVC/H.264] width = %u (pre crop)\n", width);
	fprintf(stderr, "[AVC/H.264] height = %u (pre crop)\n", height);
#endif

	// Cropping

	int cropLeft, cropRight;
	int cropTop, cropBottom;

	if(frame_cropping_flag == 1) {
		// See 14496-10, Table 6-1
		int subWidthC[4] = {1, 2, 2, 1};
		int subHeightC[4] = {1, 2, 1, 1};

		unsigned int cropUnitX, cropUnitY;

		if(separate_color_plane_flag == 0)
			chromaArrayType = chroma_format_idc;
		else
			chromaArrayType = 0;

		if(chromaArrayType == 0) {
			cropUnitX = 1;
			cropUnitY = 2 - frame_mbs_only_flag;
		}
		else {
			cropUnitX = subWidthC[chroma_format_idc];
			cropUnitY = subHeightC[chroma_format_idc] * (2 - frame_mbs_only_flag);
		}

		cropLeft = cropUnitX * frame_crop_left_offset;
		cropRight = cropUnitX * frame_crop_right_offset;
		cropTop = cropUnitY * frame_crop_top_offset;
		cropBottom = cropUnitY * frame_crop_bottom_offset;
	}
	else {
		cropLeft = 0;
		cropRight = 0;
		cropTop = 0;
		cropBottom = 0;
	}

	width = width - cropLeft - cropRight;
	height = height - cropTop - cropBottom;

#ifdef DEBUG
	fprintf(stderr, "[AVC/H.264] width = %u\n", width);
	fprintf(stderr, "[AVC/H.264] height = %u\n", height);
#endif

	h264data->valid = 1;
	h264data->width = width;
	h264data->height = height;

	return;
}

unsigned int readCodedU(bitstream_t *bitstream, int nbits, const char *name) {
	// unsigned integer with n bits
	unsigned int bits = (unsigned int)readBits(bitstream, nbits);

#ifdef DEBUG
	if(name != NULL)
		fprintf(stderr, "[AVC/H.264]\t\t%s = %u\n", name, bits);
#endif

	return bits;
}

unsigned int readCodedUE(bitstream_t *bitstream, const char *name) {
	// unsigned integer Exp-Golomb coded (see 14496-10, 9.1)
	int leadingZeroBits = -1;
	int bit;
	unsigned int codeNum = 0;

	for(bit = 0; bit == 0; leadingZeroBits++) {
		bit = readBit(bitstream);
		if (bitstream->byte == bitstream->length) break;
	}

	codeNum = ((1 << leadingZeroBits) - 1 + (unsigned int)readBits(bitstream, leadingZeroBits));

#ifdef DEBUG
	if(name != NULL)
		fprintf(stderr, "[AVC/H.264]\t\t%s = %u\n", name, codeNum);
#endif

	return codeNum;
}

int readCodedSE(bitstream_t *bitstream, const char *name) {
	// signed integer Exp-Golomb coded (see 14496-10, 9.1 and 9.1.1)
	unsigned int codeNum;
	int codeNumSigned, sign;

	codeNum = readCodedUE(bitstream, NULL);

	sign = (codeNum % 2) + 1;
	codeNumSigned = codeNum >> 1;
	if(sign == 0)
		codeNumSigned++;
	else
		codeNumSigned *= -1;

#ifdef DEBUG
	if(name != NULL)
		fprintf(stderr, "[AVC/H.264]\t\t%s = %d\n", name, codeNumSigned);
#endif

	return codeNumSigned;
}

int readBits(bitstream_t *bitstream, int nbits) {
	int i, rv = 0;

	for(i = 0; i < nbits; i++) {
		rv = (rv << 1);
		rv += readBit(bitstream);
	}

	return rv;
}

int readBit(bitstream_t *bitstream) {
	int bit;

	if(bitstream->byte == bitstream->length)
		return 0;

	bit = (bitstream->bytes[bitstream->byte] >> (7 - bitstream->bit)) & 0x01;

	bitstream->bit++;
	if(bitstream->bit == 8) {
		bitstream->byte++;
		bitstream->bit = 0;
	}

	return bit;
}

int bufferInit(buffer_t *buffer) {
	if(buffer == NULL)
		return YAMDI_ERROR;

	buffer->data = NULL;
	buffer->size = 0;
	buffer->used = 0;

	return YAMDI_OK;
}

int bufferFree(buffer_t *buffer) {
	if(buffer == NULL)
		return YAMDI_ERROR;

	if(buffer->data != NULL) {
		free(buffer->data);
		buffer->data = NULL;
	}

	return YAMDI_OK;
}

int bufferReset(buffer_t *buffer) {
	if(buffer == NULL)
		return YAMDI_ERROR;

	buffer->used = 0;

	return YAMDI_OK;
}

int bufferAppendString(buffer_t *dst, const unsigned char *string) {
	if(string == NULL)
		return YAMDI_OK;

	return bufferAppendBytes(dst, string, strlen((char *)string));
}

int bufferAppendBuffer(buffer_t *dst, buffer_t *src) {
	if(src == NULL)
		return YAMDI_OK;

	return bufferAppendBytes(dst, src->data, src->used);
}

int bufferAppendBytes(buffer_t *dst, const unsigned char *bytes, size_t nbytes) {
	size_t size;
	unsigned char *data;

	if(dst == NULL)
		return YAMDI_ERROR;

	if(bytes == NULL)
		return YAMDI_OK;

	if(nbytes == 0)
		return YAMDI_OK;

	// Check if we have to increase the buffer size
	if(dst->size < dst->used + nbytes) {
		// Pre-allocating some memory. Round up to the next 1024 bound
		size = ((dst->used + nbytes) / 1024 + 1) * 1024;

		data = (unsigned char *)realloc(dst->data, size);
		if(data == NULL)
			return YAMDI_ERROR;

		dst->data = data;
		dst->size = size;
	}

	// Copy the stuff into the buffer
	memcpy(&dst->data[dst->used], bytes, nbytes);

	dst->used += nbytes;

	return YAMDI_OK;
}

int isBigEndian(void) {
	long one = 1;
	return !(*((char *)(&one)));
}

void writeXMLMetadata(FILE *fp, const char *infile, const char *outfile, FLV_t *flv) {
	size_t i;

	fprintf(fp, "<?xml version='1.0' encoding='UTF-8'?>\n");
	fprintf(fp, "<fileset>\n");

	if(outfile != NULL)
		fprintf(fp, "<flv name=\"%s\">\n", outfile);
	else
		fprintf(fp, "<flv name=\"%s\">\n", infile);

	fprintf(fp, "<hasKeyframes>%s</hasKeyframes>\n", (flv->haskeyframes != 0) ? "true" : "false");
	fprintf(fp, "<hasVideo>%s</hasVideo>\n", (flv->hasvideo != 0) ? "true" : "false");
	fprintf(fp, "<hasAudio>%s</hasAudio>\n", (flv->hasaudio != 0) ? "true" : "false");
	fprintf(fp, "<hasMetadata>true</hasMetadata>\n");
	fprintf(fp, "<hasCuePoints>%s</hasCuePoints>\n", (flv->hascuepoints != 0) ? "true" : "false");
	fprintf(fp, "<canSeekToEnd>%s</canSeekToEnd>\n", (flv->canseektoend != 0) ? "true" : "false");

	fprintf(fp, "<audiocodecid>%d</audiocodecid>\n", flv->audio.codecid);      
	fprintf(fp, "<audiosamplerate>%d</audiosamplerate>\n", flv->audio.samplerate);
	fprintf(fp, "<audiodatarate>%d</audiodatarate>\n", (int)flv->audio.datarate);
	fprintf(fp, "<audiosamplesize>%d</audiosamplesize>\n", flv->audio.samplesize);
	fprintf(fp, "<audiodelay>%.2f</audiodelay>\n", (double)flv->audio.delay);
	fprintf(fp, "<stereo>%s</stereo>\n", (flv->audio.stereo != 0) ? "true" : "false");

	fprintf(fp, "<videocodecid>%d</videocodecid>\n", flv->video.codecid);
	fprintf(fp, "<framerate>%.2f</framerate>\n", flv->video.framerate);
	fprintf(fp, "<videodatarate>%d</videodatarate>\n", (int)flv->video.datarate);
	fprintf(fp, "<height>%d</height>\n", (int)flv->video.height);
	fprintf(fp, "<width>%d</width>\n", (int)flv->video.width);

	fprintf(fp, "<datasize>%" PRIu64 "</datasize>\n", flv->datasize);
	fprintf(fp, "<audiosize>%" PRIu64 "</audiosize>\n", flv->audio.size);
	fprintf(fp, "<videosize>%" PRIu64 "</videosize>\n", flv->video.size);
	fprintf(fp, "<filesize>%" PRIu64 "</filesize>\n", flv->filesize);

	fprintf(fp, "<lasttimestamp>%.2f</lasttimestamp>\n", (double)flv->lasttimestamp / 1000.0);
	fprintf(fp, "<lastvideoframetimestamp>%.2f</lastvideoframetimestamp>\n", (double)flv->video.lasttimestamp / 1000.0);
	fprintf(fp, "<lastkeyframetimestamp>%.2f</lastkeyframetimestamp>\n", (double)flv->keyframes.lastkeyframetimestamp / 1000.0);
	fprintf(fp, "<lastkeyframelocation>%" PRIu64 "</lastkeyframelocation>\n", (uint64_t)flv->keyframes.lastkeyframelocation);

	if(flv->options.xmlomitkeyframes == 0) {
		fprintf(fp, "<keyframes>\n");
		fprintf(fp, "<times>\n");

		for(i = 0; i < flv->keyframes.nkeyframes; i++)
			fprintf(fp, "<value id=\"%" PRIu64 "\">%.2f</value>\n", (uint64_t)i, (double)flv->keyframes.keyframetimestamps[i] / 1000.0);

		fprintf(fp, "</times>\n");
		fprintf(fp, "<filepositions>\n");

		for(i = 0; i < flv->keyframes.nkeyframes; i++)
			fprintf(fp, "<value id=\"%" PRIu64 "\">%" PRIu64 "</value>\n", (uint64_t)i, (uint64_t)flv->keyframes.keyframelocations[i]);

		fprintf(fp, "</filepositions>\n");
		fprintf(fp, "</keyframes>\n");
	}

	fprintf(fp, "<duration>%.2f</duration>\n", (double)flv->lasttimestamp / 1000.0);
	fprintf(fp, "</flv>\n");
	fprintf(fp, "</fileset>\n");

	return;
}

void printUsage(void) {
	fprintf(stderr, "NAME\n");
	fprintf(stderr, "\tyamdi -- Yet Another Metadata Injector for FLV\n");
	fprintf(stderr, "\tVersion: " YAMDI_VERSION "\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "SYNOPSIS\n");
	fprintf(stderr, "\tyamdi -i input file [-x xml file | -o output file [-x xml file]]\n");
	fprintf(stderr, "\t      [-t temporary file] [-c creator] [-a interval] [-skMXw] [-h]\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "DESCRIPTION\n");
	fprintf(stderr, "\tyamdi is a metadata injector for FLV files.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "\tOptions:\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "\t-i\tThe source FLV file. If the file name is '-' the input\n");
	fprintf(stderr, "\t\tfile will be read from stdin. Use the -t option to specify\n");
	fprintf(stderr, "\t\ta temporary file.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "\t-o\tThe resulting FLV file with the metatags. If the file\n");
	fprintf(stderr, "\t\tname is '-' the output will be written to stdout.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "\t-x\tAn XML file with the resulting metadata information. If the\n");
	fprintf(stderr, "\t\toutput file is ommited, only metadata will be generated.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "\t-t\tA temporary file to store the source FLV file in if the\n");
	fprintf(stderr, "\t\tinput file is read from stdin.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "\t-c\tA string that will be written into the creator tag.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "\t-s, -l\tAdd the onLastSecond event.\n");
	fprintf(stderr, "\t\tThe -l option is deprecated and will be removed in a future\n");
	fprintf(stderr, "\t\tversion.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "\t-k\tAdd the onLastKeyframe event.\n");
	fprintf(stderr, "\n");
/*
	fprintf(stderr, "\t-m\tLeave the existing metadata intact.\n");
	fprintf(stderr, "\t\tMetadata that yamdi does not add is left untouched, e.g. onCuepoint.\n");
	fprintf(stderr, "\n");
*/
	fprintf(stderr, "\t-M\tStrip all metadata from the FLV. The -s and -k options will\n");
	fprintf(stderr, "\t\tbe ignored.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "\t-X\tOmit the keyframes tag in the XML output.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "\t-a\tTime in milliseconds between keyframes if there is only audio.\n");
	fprintf(stderr, "\t\tThis option will be ignored if there is a video stream. No\n");
	fprintf(stderr, "\t\tkeyframes will be added if this option is omitted.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "\t-w\tReplace the input file with the output file. -i and -o are\n");
	fprintf(stderr, "\t\trequired to be different files otherwise this option will be\n");
	fprintf(stderr, "\t\tignored.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "\t-h\tThis description.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "COPYRIGHT\n");
	fprintf(stderr, "\t(c) 2007+ Ingo Oppermann\n");
	fprintf(stderr, "\n");
	return;
}
