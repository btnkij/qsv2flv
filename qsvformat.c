#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef uint64_t QWORD;

#pragma pack(1)
typedef struct {
	BYTE _codetable[0x10];
	QWORD flv_offset;
	DWORD flv_size;
} QSV_INDEX;

#pragma pack(1)
typedef struct {
	BYTE magic[0xA];
	DWORD version;
	BYTE vid[0x10];
	DWORD _unknown1;
	BYTE _unknown2[0x20];
	DWORD _unknown3;
	DWORD _unknown4;
	QWORD xml_offset;
	DWORD xml_size;
	DWORD nb_indices;
	BYTE* _unknown5;
	QSV_INDEX* indices;
} QSV_HEADER;

void decrypt_1(BYTE* buffer, DWORD size) {
	static BYTE dict[] = {0x62, 0x67, 0x70, 0x79};
	for(int eax = 0; eax < size; ++eax) {
		DWORD edx = ~eax & 0x3;
		buffer[eax] ^= dict[edx];
	}
}

void decrypt_2(BYTE* buffer, DWORD size) {
	DWORD ecx = 0x62677079U;
	for(DWORD eax = size - 1; eax != 0; --eax) {
		ecx = (ecx << 1) | (ecx >> 31);
		ecx ^= buffer[eax];
	}
	for(DWORD ebx = 1; ebx < size; ++ebx) {
		DWORD eax = buffer[ebx] & 0xFF;
		ecx ^= eax;
		ecx = (ecx >> 1) | (ecx << 31);
		DWORD k = ecx % ebx;
		BYTE al = buffer[k];
		buffer[k] = al ^ (BYTE)~buffer[ebx];
		buffer[ebx] = al;
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
	char* filename = argv[1];
	FILE* fp = fopen(filename, "rb");

	QSV_HEADER qheader;

	fread(&qheader, 1, sizeof(qheader) - sizeof(qheader._unknown5) - sizeof(qheader.indices), fp);
	printf("\n### header\n");
	printf("magic: ");
	print_chars(qheader.magic, sizeof(qheader.magic));
	printf("version: 0x%X\n", qheader.version);
	printf("vid: ");
	for(int i = 0; i < sizeof(qheader.vid); ++i) {
		printf("%02X", qheader.vid[i]);
	}
	printf("\n");
	printf("xml_offset: 0x%llX\n", qheader.xml_offset);
	printf("xml_size: 0x%X\n", qheader.xml_size);
	printf("nb_indices: 0x%X\n", qheader.nb_indices);

	DWORD _unknown5_size = (qheader.nb_indices + 7) >> 3;
	qheader._unknown5 = (BYTE*)malloc(_unknown5_size);
	fread(qheader._unknown5, 1, _unknown5_size, fp);

	printf("\n### indices\n");
	qheader.indices = (QSV_INDEX*)malloc(qheader.nb_indices * sizeof(QSV_INDEX));
	fread(qheader.indices, 1, qheader.nb_indices * sizeof(QSV_INDEX), fp);
	// print_bytes((BYTE*)qheader.indices, 0x1C * 3);
	for(int i = 0; i < qheader.nb_indices; ++i) {
		QSV_INDEX* qindex = qheader.indices + i;
		decrypt_2((BYTE*)qindex, sizeof(QSV_INDEX));
		printf("flv_offset: 0x%llX, flv_size: 0x%X\n", qindex->flv_offset, qindex->flv_size);
	}
	// print_bytes((BYTE*)qheader.indices, 0x1C * 3);

	printf("\n### xml\n");
	BYTE* xml = (BYTE*)malloc(qheader.xml_size);
	fseek(fp, qheader.xml_offset, SEEK_SET);
	fread(xml, 1, qheader.xml_size, fp);
	decrypt_1(xml, qheader.xml_size);
	print_chars(xml, qheader.xml_size);

	printf("\n### flv\n");
	const DWORD LEN_ENCRYPTED_SEGMENT = 0x400;
	BYTE* flv = (BYTE*)malloc(LEN_ENCRYPTED_SEGMENT);
	for(int i = 0; i < qheader.nb_indices; ++i) {
		printf("# section %d\n", i);
		fseek(fp, qheader.indices[i].flv_offset, SEEK_SET);
		fread(flv, 1, LEN_ENCRYPTED_SEGMENT, fp);
		decrypt_2(flv, LEN_ENCRYPTED_SEGMENT);
		print_chars(flv, LEN_ENCRYPTED_SEGMENT);
		printf("\n");
	}
	free(flv);
	flv = NULL;

	fclose(fp);
	free(qheader._unknown5);
	qheader._unknown5 = NULL;
	free(qheader.indices);
	qheader.indices = NULL;

	return 0;
}