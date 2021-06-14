#include <QString>
#include <QTableView>
#include <QLineEdit>
#include <QStandardItemModel>
#include <QFileInfo>
#include <QDir>
#include <QTableView>
#include "inputfilemodel.h"
#include "datamodel.h"


DataModel::DataModel(QTableView* tbvInputList, QLineEdit* txtOutputDir) {
    this->tbvInputList = tbvInputList;
    this->txtOutputDir = txtOutputDir;
    inputFileModel = new QStandardItemModel();
    inputFileModel->setHorizontalHeaderLabels({"文件名", "状态"});
    tbvInputList->setModel(inputFileModel);
    tbvInputList->setColumnWidth(0, tbvInputList->width() - 100);
    tbvInputList->setColumnWidth(1, 99);
    globalStatus = 0;
    targetFormat = ".mp4";
}

void DataModel::appendInputFile(const QString& filePath) {
    files.append(InputFileModel(filePath));
    const InputFileModel& file = files.back();
    inputFileModel->appendRow({new QStandardItem(file.getFileName()), new QStandardItem(file.getStatusMsg())});
}

void DataModel::removeInputFile(int rowIndex) {
    files.removeAt(rowIndex);
    inputFileModel->removeRow(rowIndex);
}

void DataModel::clearInputFile() {
    files.clear();
    inputFileModel->removeRows(0, inputFileModel->rowCount());
}

const QDir& DataModel::getOutputDir() const {
    return outputDir;
}

void DataModel::setOutputDir(const QString& value) {
    outputDir.setPath(value);
    txtOutputDir->setText(value);
}

int DataModel::getGlobalStatus() const {
    return globalStatus;
}

void DataModel::setGlobalStatus(int value) {
    globalStatus = value;
}

void DataModel::fileStatusChanged(int rowIndex) {
    QModelIndex index = inputFileModel->index(rowIndex, 1);
    QString statusMsg = files[rowIndex].getStatusMsg();
    inputFileModel->setData(index, statusMsg);
}

QVector<InputFileModel>* DataModel::getInputFiles() {
    return &files;
}

const QString& DataModel::getTargetFormat() const {
    return targetFormat;
}

void DataModel::setTargetFormat(const QString& value) {
    targetFormat = value;
}
