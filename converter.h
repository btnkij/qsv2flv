#pragma once

#include <QDebug>
#include <QString>
#include <QThread>
#include "qsvreader.h"
#include "datamodel.h"

extern "C" {
   #include "libavcodec/avcodec.h"
   #include "libavformat/avformat.h"
   #include "libswscale/swscale.h"
   #include "libavdevice/avdevice.h"
}


int readFromSegment(void *opaque, uint8_t *buf, int buf_size);

class ConverterThread : public QThread {
    Q_OBJECT
private:
    int curRowIndex;
    InputFileModel* curFile;
    void convertAllFiles();
    void convertSingleFile();
    AVFormatContext* createInputContext(QsvUnpacker* unpacker);
    AVFormatContext* createOutputContext(const char* outputPath, AVFormatContext* inCtx);
    int copyStreams(AVFormatContext* outCtx, AVFormatContext* inCtx, QsvUnpacker* unpacker);
public:
    ConverterThread(QObject* parent = nullptr);
protected:
    void run();
signals:
    void fileStatusChanged(int rowIndex);
};
