#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QDebug>
#include <QMessageBox>
#include <QSerialPort>
#include <QTimer>
#include <QTime>
#include "UserUartLink.h"


namespace Ui {
class MainWindow;
}

//const char[10][]
//{


//}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_comboBox_currentIndexChanged(int index);

    void slotDataRead();

    void slotResend();

    void on_rdButton_clicked();

    void on_wrButton_clicked();

private:
    Ui::MainWindow *ui;
    QSerialPort serialPort;
    QTimer timerResend;          //查询设备ID
    QTimer timer2;          //查询密钥
    QTimer timerWritter;    //
    QTimer timerFlush;      //
    QTimer timerWriteKey;
    QTimer timerOnLine;

    unsigned char receivedFrame[80];
    unsigned char writeFrame[80];
    QByteArray deviceid;
    QByteArray writtingDeviceid;
    QByteArray writtingKey;
    QByteArray key;
    bool isNeedFlush;

    bool isOnLine;


    unsigned char sentcmd;      //已发送命令
    unsigned char sentcnt;      //已发送次数
    bool isInWrittingState;

    void StartResend();
    void EndResend();
    void QueryId();
    void QueryKey();
    void QueryInfo();
    void WrDeviceID();
    void WriteKey();
    QString GetDevicetype(QString s);
    void OffLine();

private slots:
    void slotKeyOver();

signals:
    void signalKeyOver();

};

#endif // MAINWINDOW_H
