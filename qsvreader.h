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


void decrypt_1(BYTE* buffer, DWORD size) {
    static const BYTE dict[] = {0x62, 0x67, 0x70, 0x79};
    for(DWORD i = 0; i < size; ++i) {
        DWORD j = ~i & 0x3;
        buffer[i] ^= dict[j];
    }
}

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


//class QsvSegmentReader {
//private:
//    // input stream pointing to the beginning of the segment
//    QFile* infile;
//    // qsv version number
//    int version;
//    // size of segment
//    DWORD size;
//    // the number of bytes consumed
//    DWORD offset;
//public:
//    QsvSegmentReader(QFile* infile, int version, DWORD size) {
//        this->infile = infile;
//        this->version = version;
//        this->size = size;
//        this->offset = 0;
//    }

//    int read_bytes(BYTE* buffer, int len) {
//        qDebug("offset %d, len %d", offset, len);
//        len = len > size - offset ? size - offset : len;
//        if(len == 0)return -1;
//        int ret = infile->read((char*)buffer, len);
//        if(ret != -1) {
//            if(offset == 0 && ret >= QSV_SIZE_ENCRYPTED) {
//                if(version == 1) {
//                    decrypt_1((BYTE*)buffer, QSV_SIZE_ENCRYPTED);
//                } else if(version == 2) {
//                    decrypt_2((BYTE*)buffer, QSV_SIZE_ENCRYPTED);
//                }
//            }
//            offset += ret;
//        }
//        return ret;
//    }
//};


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
public:
    QsvUnpacker(QString& file_path) {
        infile = nullptr;
        errcode = 0;
        version = 0;
        nb_indices = 0;
        qindices = nullptr;

        infile = new QFile(file_path);
        if(!infile->open(QIODevice::ReadOnly)) {
            errcode = -1;
            msg = "unable to open file";
            return;
        }

        QsvHeader qheader;
        if(infile->read((char*)&qheader, sizeof(QsvHeader)) != sizeof(QsvHeader)) {
            errcode = -2;
            msg = "unexpected end of file";
            return;
        }
        if(memcmp(qheader.signature, QSV_SIGNATURE, sizeof(QSV_SIGNATURE) - 1) != 0) {
            errcode = -3;
            msg = "malformed qsv format";
            return;
        }
        version = qheader.version;
        if(version != 1 && version != 2) {
            errcode = -4;
            msg = "unsupported version";
        }
        nb_indices = qheader.nb_indices;
        if(nb_indices < 1 || nb_indices > 0xFFFF) {
            errcode = -3;
            msg = "malformed qsv format";
            return;
        }

        int _index_flag_size = (nb_indices + 7) >> 3;
        if(infile->skip(_index_flag_size) != _index_flag_size) {
            errcode = -2;
            msg = "unexpected end of file";
            return;
        }
        qindices = new QsvIndex[nb_indices];
        if(infile->read((char*)qindices, nb_indices * sizeof(QsvIndex)) != nb_indices * sizeof(QsvIndex)) {
            errcode = -2;
            msg = "unexpected end of file";
            return;
        }
        for(int i = 0; i < nb_indices; ++i) {
            QsvIndex* index = qindices + i;
            if(version == 2) {
                decrypt_2((BYTE*)(qindices + i), sizeof(QsvIndex));
            }
            if(index->segment_offset + index->segment_size > (QWORD)infile->size()
                    || index->segment_size < QSV_SIZE_ENCRYPTED) {
                errcode = -3;
                msg = "malformed qsv format";
                return;
            }
        }
    }

    ~QsvUnpacker() {
        if(infile) {
            infile->close();
            delete infile;
        }
        if(qindices) {
            delete[] qindices;
        }
    }

    int get_errcode() const {
        return errcode;
    }

    const QString& get_msg() const {
        return msg;
    }

    int get_nb_indices() const {
        return nb_indices;
    }

//    QsvSegmentReader get_segment_reader(int i) {
//        infile->seek(qindices[i].segment_offset);
//        return QsvSegmentReader(infile, version, qindices[i].segment_size);
//    }

    void init_read() {
        read_index = 0;
        read_offset = 0;
        infile->seek(qindices[read_index].segment_offset);
    }

    int read_bytes(BYTE* buffer, int query_size) {
//        qDebug("index %d, offset %d, query_size %d", read_index, read_offset, query_size);
        int total_read_size = 0;
        while(total_read_size < query_size && read_index < nb_indices) {
            int read_size = query_size - total_read_size;
            if((DWORD)(read_offset + read_size) > qindices[read_index].segment_size) {
                read_size = qindices[read_index].segment_size - read_offset;
            }
            if(read_size == 0) {
                ++read_index;
                read_offset = 0;
                if(read_index < nb_indices) {
                    infile->seek(qindices[read_index].segment_offset);
                }
                continue;
            }
            if(read_offset == 0 && read_size < QSV_SIZE_ENCRYPTED) {
                break; // no enough bytes to decryte
            }
            read_size = infile->read((char*)buffer + total_read_size, read_size);
            if(read_size <= 0) {
                break; // error
            }
            if(read_offset == 0) {
                if(version == 1) {
                    decrypt_1(buffer + total_read_size, QSV_SIZE_ENCRYPTED);
                } else if(version == 2) {
                    decrypt_2(buffer + total_read_size, QSV_SIZE_ENCRYPTED);
                }
            }
            total_read_size += read_size;
            read_offset += read_size;
        }
        return total_read_size == 0 ? -1 : total_read_size;
    }
};
