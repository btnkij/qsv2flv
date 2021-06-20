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

void MainWindow::dragEnterEvent(QDragEnterEvent *e) {
    if(e->mimeData()->hasUrls()) {
        e->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *e) {
    QList<QUrl> urls = e->mimeData()->urls();
    for(QUrl& url : urls) {
        QString filePath = url.toLocalFile();
        if(filePath.endsWith(".qsv")) {
            dataModel->appendInputFile(filePath);
        }
    }
}

void MainWindow::on_btnSelectOutputPath_clicked()
{
    QString outputPath = QFileDialog::getExistingDirectory(this, "选择输出路径");
    dataModel->setOutputDir(outputPath);
}

void MainWindow::on_btnAppendFiles_clicked()
{
    if(dataModel->getGlobalStatus() != 0) {
        QMessageBox::information(this, "消息", "转码进行中，无法添加文件");
        return;
    }
    QStringList filenames = QFileDialog::getOpenFileNames(this, "选择待转码的文件", "", "QIYI视频(*.qsv)");
    for(QString& filename : filenames) {
        dataModel->appendInputFile(filename);
    }
}

void MainWindow::conversionDestroyed() {
    converterThread = nullptr;
    dataModel->setGlobalStatus(0);
}

void MainWindow::on_btnConvert_clicked()
{
    if(dataModel->getGlobalStatus() != 0) {
        QMessageBox::information(this, "消息", "转码进行中，无法开始新任务");
        return;
    }
    dataModel->setGlobalStatus(1);
    converterThread = new ConverterThread(nullptr);
    connect(converterThread, &ConverterThread::fileStatusChanged, dataModel, &DataModel::fileStatusChanged);
    connect(converterThread, &QThread::finished, converterThread, &QObject::deleteLater);
    connect(converterThread, &QObject::destroyed, this, &MainWindow::conversionDestroyed);
    converterThread->start();
}

void MainWindow::on_btnRemoveFile_clicked()
{
    if(dataModel->getGlobalStatus() != 0) {
        QMessageBox::information(this, "消息", "转码进行中，无法移除文件");
        return;
    }
    QModelIndexList selectedRows = ui->tbvInputList->selectionModel()->selectedRows();
    for(QModelIndex index : selectedRows) {
        dataModel->removeInputFile(index.row());
    }
}


void MainWindow::on_btnClearList_clicked()
{
    if(dataModel->getGlobalStatus() != 0) {
        QMessageBox::information(this, "消息", "转码进行中，无法清空列表");
        return;
    }
    dataModel->clearInputFile();
}

void MainWindow::on_cbxTargetFormat_currentIndexChanged(int index)
{
    dataModel->setTargetFormat(ui->cbxTargetFormat->currentText());
}

