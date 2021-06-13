#pragma once

#include <QString>


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
    QString inputPath;
    FileStatus statusCode;
    QString statusMsg;
    float progress;
public:
    InputFileModel(const QString& path);

    const QString& getInputPath() const;

    FileStatus getStatusCode() const;

    void setStatusCode(FileStatus value);

    QString getStatusMsg() const;

    void setStatusMsg(const QString& value);

    void setProgress(float value);
};
