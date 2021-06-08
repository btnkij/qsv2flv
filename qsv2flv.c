/**
*
* description: qsv-to-flv converter
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
#include "yamdi.h"

typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef uint64_t QWORD;

#define QSV_ENCRYPTED_SIZE 0x400

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

#pragma pack(1)
typedef struct {
	BYTE _codetable[0x10];
	QWORD segment_offset;
	DWORD segment_size;
} QsvIndex;

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

void write_flv_segment(FILE* fp, BYTE* buffer, DWORD size, DWORD is_first) {
	if(is_first) {
		// write flv header
		fwrite(buffer, FLV_SIZE_HEADER + FLV_SIZE_PREVIOUSTAGSIZE, 1, fp);
	}
	DWORD offset = FLV_SIZE_HEADER + FLV_SIZE_PREVIOUSTAGSIZE;
	int skip_count = is_first ? 1 : 3;
	for(int i = 0; i < skip_count; ++i) {
		FlvTagHeader* tag = (FlvTagHeader*)(buffer + offset);
		offset += sizeof(FlvTagHeader) + FLV_UI24(tag->tag_data_size) + FLV_SIZE_PREVIOUSTAGSIZE;
	}
	fwrite(buffer + offset, size - offset, 1, fp);
}

void extract_flv(char* input_file, char* output_file) {
	FILE* fin = fopen(input_file, "rb");
	assert(fin && "[ERROR] Unable to open input file.");
	FILE* fout = fopen(output_file, "wb");
	assert(fout && "[ERROR] Unable to open output file.");

	QsvHeader qheader;
	assert(fread(&qheader, sizeof(qheader), 1, fin) == 1 && "[ERROR] Unable to read file.");

	QsvIndex* qindices = (QsvIndex*)malloc(qheader.nb_indices * sizeof(QsvIndex));
	assert(qindices && "[ERROR] Unable to allocate memory.");
	DWORD _index_flag_size = (qheader.nb_indices + 7) >> 3;
	fseek(fin, _index_flag_size, SEEK_CUR);
	assert(fread(qindices, qheader.nb_indices * sizeof(QsvIndex), 1, fin) == 1 && "[ERROR] Unable to read file.");
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
	assert(segment && "[ERROR] Unable to allocate memory.");
	for(int i = 0; i < qheader.nb_indices; ++i) {
		QsvIndex* index = qindices + i;
		fseek(fin, index->segment_offset, SEEK_SET);
		assert(fread(segment, index->segment_size, 1, fin) == 1 && "[ERROR] Unable to read file.");
		if(qheader.version == 0x1) {
			decrypt_1(segment, QSV_ENCRYPTED_SIZE);
		} else if(qheader.version == 0x2) {
			decrypt_2(segment, QSV_ENCRYPTED_SIZE);
		}
		write_flv_segment(fout, segment, index->segment_size, i == 0);
	}

	free(qindices);
	qindices = NULL;
	free(segment);
	segment = NULL;
	fclose(fin);
	fclose(fout);
}

int inject_metadata(char* input_file, char* output_file) {
	char* argv[] = {"", "-i", input_file, "-o", output_file};
	yamdi_main(sizeof(argv) / sizeof(char*), (char**)argv);
}

void help() {
	printf("Usage: qsv2flv.exe input_file_name output_file_name\n");
}

int main(int argc, char** argv) {
	if(argc != 3) {
		help();
		return -1;
	}
	char* input_file = argv[1];
	char* output_file = argv[2];
	char* tmp_suffix = ".tmp";
	char* tmp_file = (char*)malloc(strlen(output_file) + strlen(tmp_suffix) + 1);
	assert(tmp_file && "[ERROR] Unable to allocate memory.");
	strcpy(tmp_file, output_file);
	strcat(tmp_file, tmp_suffix);
	extract_flv(input_file, tmp_file);
	inject_metadata(tmp_file, output_file);
	assert(remove(tmp_file) == 0 && "[ERROR] Cannot delete temporary file.");
	free(tmp_file);
	tmp_file = NULL;
	return 0;
}