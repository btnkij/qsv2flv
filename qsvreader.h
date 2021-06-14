#pragma once


#include <cstdio>
#include <cstdint>
#include <cstring>
#include <QFile>


typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef uint64_t QWORD;


#define QSV_SIZE_ENCRYPTED 0x400
const char QSV_SIGNATURE[] = "QIYI VIDEO";


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


void decrypt_1(BYTE* buffer, DWORD size);

void decrypt_2(BYTE* buffer, DWORD size);


class QsvUnpacker {
private:
    QFile* infile;
    int errcode;
    QString msg;
    int version;
    int nb_indices;
    QsvIndex* qindices;

    int read_index;
    int read_offset;
    int total_size;
    int processed_size;
public:
    QsvUnpacker(const QString& file_path);

    ~QsvUnpacker();

    int get_errcode() const;

    const QString& get_msg() const;

    int get_nb_indices() const;

    void seek_to_segment(int idx);

    int read_bytes(BYTE* buffer, int query_size);

    void init_progress();

    float get_progress() const;
};
