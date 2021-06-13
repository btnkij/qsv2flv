#pragma once

#include <QDebug>
#include <QString>
#include <QThread>
#include "qsvreader.h"
#include "datamodel.h"

int readFromSegment(void *opaque, uint8_t *buf, int buf_size);

class ConverterThread : public QThread {
    Q_OBJECT
private:
    int curRowIndex;
    InputFileModel* curFile;
    void convertAllFiles();
    void convertSingleFile();
public:
    ConverterThread(QObject* parent = nullptr);
protected:
    void run();
signals:
    void fileStatusChanged(int rowIndex);
};
