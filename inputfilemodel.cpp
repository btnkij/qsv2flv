#include "inputfilemodel.h"
#include <QFileInfo>

InputFileModel::InputFileModel(const QString& path)
    :info(path) {
    statusCode = STATUS_WAITING;
    progress = 0;
}

QString InputFileModel::getFilePath() const {
    return info.absoluteFilePath();
}

QString InputFileModel::getFileBaseName() const {
    return info.baseName();
}

QString InputFileModel::getFileName() const {
    return info.fileName();
}

InputFileModel::FileStatus InputFileModel::getStatusCode() const {
    return statusCode;
}

void InputFileModel::setStatusCode(InputFileModel::FileStatus value) {
    statusCode = value;
}

QString InputFileModel::getStatusMsg() const {
    switch(statusCode) {
    case STATUS_WAITING:
        return "等待";
    case STATUS_PROCESSING:
        return QString("正在转码%1%").arg(100 * progress, 0, 'g', 3);
    case STATUS_SUCCEEDED:
        return "完成";
    case STATUS_FAILED:
        return QString("错误: %1").arg(statusMsg);
    default:
        return "未知错误";
    }
}

void InputFileModel::setStatusMsg(const QString& value) {
    statusMsg = value;
}

void InputFileModel::setProgress(float value) {
    progress = value;
}
