/**
*
* description: an example for parsing qsv
* author: btnkij
*
* the qsv format is extracted from GeePlayer 5.2.61.5220 (Jan 21, 2020)
* algorithm may fail in the future
*
**/


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

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

#pragma pack()

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

void print_bytes(BYTE* buffer, DWORD size) {
	for(int i = 0; i < size; ++i) {
		printf("%02X ", buffer[i]);
		if((i + 1) % 0x20 == 0)printf("\n");
	}
	if(size % 0x10 != 0)printf("\n");
}

void print_chars(BYTE* buffer, DWORD size) {
	for(int i = 0; i < size; ++i) {
		putchar(buffer[i]);
	}
	printf("\n");
}

int main(int argc, char** argv) {
	// input the qsv file name as the first parameter
	// or assign the following variable directly
	char* filename = argv[1];
	FILE* fp = fopen(filename, "rb");

	QsvHeader qheader;
	fread(&qheader, sizeof(qheader), 1, fp);
	printf("\n# header\n");
	printf("signature: ");
	print_chars(qheader.signature, sizeof(qheader.signature));
	printf("version: 0x%X\n", qheader.version);
	printf("vid: ");
	for(int i = 0; i < sizeof(qheader.vid); ++i) {
		printf("%02X", qheader.vid[i]);
	}
	printf("\n");
	printf("xml_offset: 0x%llX\n", qheader.xml_offset);
	printf("xml_size: 0x%X\n", qheader.xml_size);
	printf("nb_indices: 0x%X\n", qheader.nb_indices);

	printf("\n# indices\n");
	DWORD _unknown_flag_size = (qheader.nb_indices + 7) >> 3;
	BYTE* _unknown_flag = (BYTE*)malloc(_unknown_flag_size);
	fread(_unknown_flag, _unknown_flag_size, 1, fp);
	QsvIndex* qindices = (QsvIndex*)malloc(qheader.nb_indices * sizeof(QsvIndex));
	fread(qindices, sizeof(QsvIndex), qheader.nb_indices, fp);
	for(int i = 0; i < qheader.nb_indices; ++i) {
		QsvIndex* qindex = qindices + i;
		if(qheader.version == 0x2) {
			decrypt_2((BYTE*)qindex, sizeof(QsvIndex));
		}
		printf("flag: %X", (_unknown_flag[i >> 3] >> (i & 0x7)) & 1);
		printf(", offset: 0x%X", qindex->segment_offset);
		printf(", size: 0x%X\n", qindex->segment_size);
	}
	free(_unknown_flag);
	_unknown_flag = NULL;

	printf("\n# xml\n");
	BYTE* xml = (BYTE*)malloc(qheader.xml_size);
	fseek(fp, qheader.xml_offset, SEEK_SET);
	fread(xml, qheader.xml_size, 1, fp);
	decrypt_1(xml, qheader.xml_size);
	print_chars(xml, qheader.xml_size);
	free(xml);
	xml = NULL;

	printf("\n# segments\n");
	BYTE* buffer = (BYTE*)malloc(QSV_ENCRYPTED_SIZE);
	for(int i = 0; i < qheader.nb_indices; ++i) {
		printf("### segment %d\n", i);
		QsvIndex* qindex = qindices + i;
		fseek(fp, qindex->segment_offset, SEEK_SET);
		fread(buffer, QSV_ENCRYPTED_SIZE, 1, fp);
		if(qheader.version == 0x1) {
			decrypt_1(buffer, QSV_ENCRYPTED_SIZE);
		} else if(qheader.version == 0x2) {
			decrypt_2(buffer, QSV_ENCRYPTED_SIZE);
		}
		print_bytes(buffer, QSV_ENCRYPTED_SIZE);
		printf("\n");
	}
	free(qindices);
	qindices = NULL;
	free(buffer);
	buffer = NULL;

	fclose(fp);

	return 0;
}