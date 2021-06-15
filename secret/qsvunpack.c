/**
*
* description: extract and merge media segments
* author: btnkij
*
* the qsv format is extracted from GeePlayer 5.2.61.5220 (Jan 21, 2020)
* algorithm may fail in the future
*
**/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef uint64_t QWORD;

#define QSV_SIZE_ENCRYPTED 0x400

#define TS_SIZE_PACKET 0xBC
#define FLV_SIZE_HEADER 0x9
#define FLV_SIZE_PREVIOUSTAGSIZE 0x4
#define FLV_UI24(x) (DWORD)(((*(x)) << 16) + ((*(x + 1)) << 8) + (*(x + 2)))

#pragma pack(1)

typedef struct {
	BYTE signature[0xA];
	DWORD version;
	BYTE vid[0x10];
	DWORD _unknown1;
	BYTE _unknown2[0x20];
	DWORD _unknown3;
	DWORD _unknown4;
	QWORD xml_offset;
	DWORD xml_size;
	DWORD nb_indices;
} QsvHeader;

typedef struct {
	BYTE _codetable[0x10];
	QWORD segment_offset;
	DWORD segment_size;
} QsvIndex;

#pragma pack()

// decryption algorithm for some segments in qsv version 0x1
void decrypt_1(BYTE* buffer, DWORD size) {
	static BYTE dict[] = {0x62, 0x67, 0x70, 0x79};
	for(int i = 0; i < size; ++i) {
		DWORD j = ~i & 0x3;
		buffer[i] ^= dict[j];
	}
}

// decryption algorithm for some segments in qsv version 0x2
void decrypt_2(BYTE* buffer, DWORD size) {
	DWORD x = 0x62677079;
	for(DWORD i = size - 1; i != 0; --i) {
		x = (x << 1) | (x >> 31);
		x ^= buffer[i];
	}
	for(DWORD i = 1; i < size; ++i) {
		x ^= buffer[i] & 0xFF;
		x = (x >> 1) | (x << 31);
		DWORD j = x % i;
		BYTE tmp = buffer[j];
		buffer[j] = tmp ^ (BYTE)~buffer[i];
		buffer[i] = tmp;
	}
}

#pragma pack(1)

typedef struct {
  BYTE tag_type;
  BYTE tag_data_size[3];
  BYTE timestamp[3];
  BYTE timestamp_extended;
  BYTE stream_id[3];
} FlvTagHeader;

#pragma pack()

void write_flv_segment(FILE* fp, BYTE* buffer, DWORD size, DWORD is_first) {
	if(is_first) {
		// write flv header and the first previous tag size
		fwrite(buffer, FLV_SIZE_HEADER + FLV_SIZE_PREVIOUSTAGSIZE, 1, fp);
	}
	DWORD offset = FLV_SIZE_HEADER + FLV_SIZE_PREVIOUSTAGSIZE;
	// if is_first then skip the script tag
	// else skip script tag and the following 2 tags whose timestamps are always 0
	// it's tedious to merge flv metadata. so just ignore it.
	int skip_count = is_first ? 1 : 3;
	for(int i = 0; i < skip_count; ++i) {
		FlvTagHeader* tag = (FlvTagHeader*)(buffer + offset);
		offset += sizeof(FlvTagHeader) + FLV_UI24(tag->tag_data_size) + FLV_SIZE_PREVIOUSTAGSIZE;
	}
	// write the remaining tags;
	fwrite(buffer + offset, size - offset, 1, fp);
}

void write_ts_segment(FILE* fp, BYTE* buffer, DWORD size, DWORD is_first) {
	if(is_first) {
		// write all packets, including SDT, PAT and PMT
		fwrite(buffer, size, 1, fp);
	} else {
		// skip PAT and PMT packets
		DWORD offset = TS_SIZE_PACKET * 2;
		// writing the remain packets
		fwrite(buffer + offset, size - offset, 1, fp);
	}
}

DWORD get_media_type(BYTE* buffer) {
	if(*(DWORD*)buffer == 0x01564C46) // if begin with 'FLV\x01'
		return 1;
	if(*buffer == '\x47')
		return 2;
	return 0;
}

void extract_media(char* input_file, char* output_file) {
	FILE* fin = fopen(input_file, "rb");
	FILE* fout = fopen(output_file, "wb");

	QsvHeader qheader;
	fread(&qheader, sizeof(qheader), 1, fin);

	QsvIndex* qindices = (QsvIndex*)malloc(qheader.nb_indices * sizeof(QsvIndex));
	DWORD _unknown_flag_size = (qheader.nb_indices + 7) >> 3;
	fseek(fin, _unknown_flag_size, SEEK_CUR);
	fread(qindices, sizeof(QsvIndex), qheader.nb_indices, fin);
	if(qheader.version == 0x2) {
		for(int i = 0; i < qheader.nb_indices; ++i) {
			decrypt_2((BYTE*)(qindices + i), sizeof(QsvIndex));
		}
	}

	DWORD max_segment_size = 0;
	for(int i = 0; i < qheader.nb_indices; ++i) {
		DWORD segment_size = qindices[i].segment_size;
		max_segment_size = max_segment_size < segment_size ? segment_size : max_segment_size;
	}
	BYTE* segment = (BYTE*)malloc(max_segment_size);

	DWORD media_type = -1;
	for(int i = 0; i < qheader.nb_indices; ++i) {
		QsvIndex* index = qindices + i;
		fseek(fin, index->segment_offset, SEEK_SET);
		fread(segment, index->segment_size, 1, fin);
		if(qheader.version == 0x1) {
			decrypt_1(segment, QSV_SIZE_ENCRYPTED);
		} else if(qheader.version == 0x2) {
			decrypt_2(segment, QSV_SIZE_ENCRYPTED);
		}
		if(media_type == -1) {
			media_type = get_media_type(segment);
		}
		switch(media_type) {
		case 1:
			write_flv_segment(fout, segment, index->segment_size, i == 0);
			break;
		case 2:
			write_ts_segment(fout, segment, index->segment_size, i == 0);
			break;
		}
	}

	free(qindices);
	qindices = NULL;
	free(segment);
	segment = NULL;
	fclose(fin);
	fclose(fout);
}

void help() {
	printf("Usage: qsvunpack.exe input_file_name output_file_name\n");
}

int main(int argc, char** argv) {
	if(argc != 3) {
		help();
		return -1;
	}
	char* input_file = argv[1];
	char* output_file = argv[2];
	extract_media(input_file, output_file);
	return 0;
}