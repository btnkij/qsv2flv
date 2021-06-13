#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFile>
#include <QFileDialog>
#include <QThread>
#include <QMessageBox>
#include "datamodel.h"
#include "converter.h"

// global model/view object
DataModel* dataModel;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , converterThread(nullptr)
{
    ui->setupUi(this);
    dataModel = new DataModel(ui->tbvInputList, ui->txtOutputPath);
}

MainWindow::~MainWindow()
{
    delete ui;
    delete dataModel;
}

void MainWindow::on_pushButton_clicked()
{
    if(dataModel->getGlobalStatus() != 0) {
        QMessageBox::information(this, "消息", "转码进行中，无法开始新任务");
        return;
    }
    dataModel->setGlobalStatus(1);
    converterThread = new ConverterThread(nullptr);
    connect(converterThread, &ConverterThread::fileStatusChanged, dataModel, &DataModel::fileStatusChanged);
    connect(converterThread, &QThread::finished, this, &MainWindow::conversionFinished);
    connect(converterThread, &QThread::finished, converterThread, &QObject::deleteLater);
    connect(converterThread, &QObject::destroyed, this, &MainWindow::conversionDestroyed);
    converterThread->start();
}

void MainWindow::on_btnSelectOutputPath_clicked()
{
    QString outputPath = QFileDialog::getExistingDirectory(this, "选择输出路径");
    dataModel->setOutputDir(outputPath);
}

void MainWindow::on_btnAppendFiles_clicked()
{
    QStringList filenames = QFileDialog::getOpenFileNames(this, "选择待转码的文件", "", "QIYI视频(*.qsv)");
    for(QString& filename : filenames) {
        dataModel->appendInputFile(filename);
    }
}

void MainWindow::conversionFinished() {

}

void MainWindow::conversionDestroyed() {
    converterThread = nullptr;
    dataModel->setGlobalStatus(0);
}
