#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include "converter.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    virtual void dragEnterEvent(QDragEnterEvent *e);
    virtual void dropEvent(QDropEvent *e);

private slots:
    void on_btnSelectOutputPath_clicked();

    void on_btnAppendFiles_clicked();

    void conversionDestroyed();

    void on_btnConvert_clicked();

    void on_btnRemoveFile_clicked();

    void on_btnClearList_clicked();

    void on_cbxTargetFormat_currentIndexChanged(int index);

private:
    Ui::MainWindow *ui;
    ConverterThread *converterThread;
};
#endif // MAINWINDOW_H
