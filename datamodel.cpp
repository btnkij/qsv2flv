#include <QString>
#include <QTableView>
#include <QLineEdit>
#include <QStandardItemModel>
#include <QFileInfo>
#include <QDir>
#include "inputfilemodel.h"
#include "datamodel.h"


DataModel::DataModel(QTableView* tbvInputList, QLineEdit* txtOutputDir) {
    this->tbvInputList = tbvInputList;
    this->txtOutputDir = txtOutputDir;
    inputFileModel = new QStandardItemModel();
    inputFileModel->setHorizontalHeaderLabels({"文件名", "状态"});
    tbvInputList->setModel(inputFileModel);
    globalStatus = 0;
    targetFormat = ".mp4";
}

void DataModel::appendInputFile(const QString& filePath) {
    files.append(InputFileModel(filePath));
    const InputFileModel& file = files.back();
    inputFileModel->appendRow({new QStandardItem(file.getInputPath()), new QStandardItem(file.getStatusMsg())});
}

void DataModel::removeInputFile(int rowIndex) {
    files.removeAt(rowIndex);
    inputFileModel->removeRow(rowIndex);
}

void DataModel::clearInputFile() {
    files.clear();
    inputFileModel->clear();
    inputFileModel->setHorizontalHeaderLabels({"文件名", "状态"});
}

const QString& DataModel::getOutputDir() const {
    return outputDir;
}

void DataModel::setOutputDir(const QString& value) {
    outputDir = value;
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

QString DataModel::getOutputPath(const QString& inputPath) const {
    QFileInfo fileInfo(inputPath);
    QString baseName = fileInfo.baseName();
    QDir outDir(outputDir);
    return outDir.absoluteFilePath(baseName + targetFormat);
}
