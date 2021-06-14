#pragma once

#include <QString>
#include <QFileInfo>


class InputFileModel {
public:
    enum FileStatus {
        STATUS_NONE,
        STATUS_WAITING,
        STATUS_PROCESSING,
        STATUS_SUCCEEDED,
        STATUS_FAILED
    };
private:
    QFileInfo info;
    FileStatus statusCode;
    QString statusMsg;
    float progress;
public:
    InputFileModel(const QString& path);

    QString getFilePath() const;

    QString getFileBaseName() const;

    QString getFileName() const;

    FileStatus getStatusCode() const;

    void setStatusCode(FileStatus value);

    QString getStatusMsg() const;

    void setStatusMsg(const QString& value);

    void setProgress(float value);
};
