#pragma once

#include <QObject>
#include <QString>
#include <QTableView>
#include <QLineEdit>
#include <QStandardItemModel>
#include "inputfilemodel.h"


class DataModel : public QObject {
    Q_OBJECT
private:
    QTableView* tbvInputList;
    QLineEdit* txtOutputDir;
    QString outputDir;
    QVector<InputFileModel> files;
    QStandardItemModel* inputFileModel;
    int globalStatus;
    QString targetFormat;
public:
    DataModel(QTableView* tbvInputList, QLineEdit* txtOutputDir);

    void appendInputFile(const QString& filePath);

    void removeInputFile(int rowIndex);

    void clearInputFile();

    const QString& getOutputDir() const;

    void setOutputDir(const QString& value);

    int getGlobalStatus() const;

    void setGlobalStatus(int value);

    QVector<InputFileModel>* getInputFiles();

    QString getOutputPath(const QString& inputPath) const;
public slots:
    void fileStatusChanged(int rowIndex);
};

extern DataModel* dataModel;
