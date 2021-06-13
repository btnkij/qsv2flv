#pragma once


#include <cstdio>
#include <cstdint>
#include <cstring>
#include <QFile>
#include "qsvreader.h"


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


QsvUnpacker::QsvUnpacker(const QString& file_path) {
    infile = nullptr;
    errcode = 0;
    version = 0;
    nb_indices = 0;
    qindices = nullptr;

    infile = new QFile(file_path);
    if(!infile->open(QIODevice::ReadOnly)) {
        errcode = -1;
        msg = "无法打开文件";
        return;
    }

    QsvHeader qheader;
    if(infile->read((char*)&qheader, sizeof(QsvHeader)) != sizeof(QsvHeader)) {
        errcode = -2;
        msg = "文件不完整";
        return;
    }
    if(memcmp(qheader.signature, QSV_SIGNATURE, sizeof(QSV_SIGNATURE) - 1) != 0) {
        errcode = -3;
        msg = "无法识别的格式";
        return;
    }
    version = qheader.version;
    if(version != 1 && version != 2) {
        errcode = -4;
        msg = "无法识别的格式";
    }
    nb_indices = qheader.nb_indices;
    if(nb_indices < 1 || nb_indices > 0xFFFF) {
        errcode = -3;
        msg = "无法识别的格式";
        return;
    }

    int _index_flag_size = (nb_indices + 7) >> 3;
    if(infile->skip(_index_flag_size) != _index_flag_size) {
        errcode = -2;
        msg = "文件不完整";
        return;
    }
    qindices = new QsvIndex[nb_indices];
    if(infile->read((char*)qindices, nb_indices * sizeof(QsvIndex)) != nb_indices * sizeof(QsvIndex)) {
        errcode = -2;
        msg = "文件不完整";
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
            msg = "无法识别的格式";
            return;
        }
    }
}

QsvUnpacker::~QsvUnpacker() {
    if(infile) {
        infile->close();
        delete infile;
    }
    if(qindices) {
        delete[] qindices;
    }
}

int QsvUnpacker::get_errcode() const {
    return errcode;
}

const QString& QsvUnpacker::get_msg() const {
    return msg;
}

int QsvUnpacker::get_nb_indices() const {
    return nb_indices;
}

void QsvUnpacker::init_read() {
    read_index = 0;
    read_offset = 0;
    total_size = 0;
    for(int i = 0; i < nb_indices; ++i) {
        total_size += qindices[i].segment_size;
    }
    processed_size = 0;
    infile->seek(qindices[read_index].segment_offset);
}

int QsvUnpacker::read_bytes(BYTE* buffer, int query_size) {
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
        processed_size += read_size;
    }
    return total_read_size == 0 ? -1 : total_read_size;
}

float QsvUnpacker::get_progress() const {
    return (float)processed_size / total_size;
}
