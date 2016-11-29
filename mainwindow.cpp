#include "mainwindow.h"
#include "ui_mainwindow.h"

/***********协议命令*************/
#define CMDCOR      //校正协议命令

#define CmdAck      0x05

#define CmdRdId     0x10
#define CmdRdIdAck  0x11

#ifdef  CMDCOR
#define CmdRdKey    0x14        //new protocol
#else
#define CmdRdKey    0x87        //old protocol
#endif
#define CmdRdKeyAck 0x15

#define CmdRdInfo    0x82
#define CmdRDInfoAck 0x83
#define CmdWrID      0x85

#ifdef  CMDCOR
#define CmdWrKey     0x87       //new protocol'
#else
#define CmdWrKey     0x14       //old protocol
#endif

/***********协议命令*************/
MainWindow::MainWindow(QWidget *parent) :    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setWindowTitle("设备ID读写工具V1.1.0");
    //this->ui->pushButton->setVisible(false);

    QSerialPortInfo::availablePorts();
    this->ui->comboBox->addItem("选择串口");
    foreach (QSerialPortInfo info, QSerialPortInfo::availablePorts()) {
        this->ui->comboBox->addItem(info.portName());
    }
    serialPort.setBaudRate(QSerialPort::Baud9600);
    serialPort.setParity(QSerialPort::NoParity);
    serialPort.setStopBits(QSerialPort::OneStop);
    serialPort.setFlowControl(QSerialPort::NoFlowControl);

    timerResend.setSingleShot(true);
    connect(&serialPort,SIGNAL(readyRead()),this,SLOT(slotDataRead()));
    connect(&timerResend,SIGNAL(timeout()),this,SLOT(slotResend()));
    connect(this,SIGNAL(signalKeyOver()),this,SLOT(slotKeyOver()));
    this->isInWrittingState = false;
    this->sentcnt = 0;

//#define SQUARE(x) ((x)*(x))
//    int a = 5,f=5, g=5, h=5;
//    int b,c,d,e;
//    b = (a++)*(++a);
//    c = (f++)*(f++);
//    d = (++g)*(g++);
//    e = (++h)*(++h);
//    qDebug()<<"b=%d",b;//<<"c=:"+c<<"d=:"+d<<"e+:"+e;
//    qDebug()<<a<<f<<g<<h;
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_comboBox_currentIndexChanged(int index)

{
    qDebug()<<this->ui->comboBox->itemText(index);
    if(serialPort.isOpen())
    {
        serialPort.close();
    }
    if(this->ui->comboBox->itemText(index).contains("COM"))
    {
        this->serialPort.setPortName(ui->comboBox->itemText(index));
        if(serialPort.open(QIODevice::ReadWrite))
        {
        }
        else
        {
            QMessageBox::warning(this,"错误","串口已被使用");
            this->ui->comboBox->setCurrentIndex(0);
        }
    }
    else
    {
        serialPort.close();
        this->ui->comboBox->setCurrentIndex(0);
    }
}

void MainWindow::on_rdButton_clicked()
{
    if(this->serialPort.isOpen())
    {
        QueryId();
        StartResend();
    }
    else
    {
        QMessageBox::warning(this,"错误","请打开串口");
    }
}

void MainWindow::on_wrButton_clicked()
{
    if(QRegExp("[a-fA-F0-9]{56}").exactMatch(this->ui->deviceidlineEdit->text())
            && QRegExp("[a-zA-Z0-9]{16}").exactMatch(this->ui->keylineEdit->text()))
    {
        this->isInWrittingState = true;
        QString str = this->ui->deviceidlineEdit->text();
        writtingDeviceid.clear();
        for( int i=0;i<str.length();i=i+2)
        {
            writtingDeviceid.append(str.mid(i,2).toInt(0,16));
        }
        for(int i=0;i<8;i++)
        {
            writtingDeviceid.append('\0');
        }
        this->writtingKey = this->ui->keylineEdit->text().toLocal8Bit();
        QueryId();
    }
    else
    {
        QMessageBox::warning(this,"错误","设备ID或者秘钥错误");
    }

}

void MainWindow::slotKeyOver()
{
   qDebug()<<__FUNCTION__<<__LINE__;

  if(QMessageBox::question(this,"警告","当前设备ID为："+this->deviceid.toHex()+"\n"+
                        "设备秘钥为:"+QString(this->key)+
                        "确定重新写入?",QMessageBox::Yes|QMessageBox::No) == QMessageBox::Yes)
  {
      qDebug()<<"yes";
      WrDeviceID();
      StartResend();
  }
  else
  {
      qDebug()<<"no";
  }
   this->isInWrittingState = false;
}

void MainWindow::slotDataRead()
{
    QByteArray receivedData = serialPort.readAll();
    bool uartsuccess = UserUartLinkUnpack((unsigned char *)receivedData.data(),receivedData.length());
    if(uartsuccess)
    {
        EndResend();
        int len = getUserUartLinkMsg(this->receivedFrame);
        qDebug()<<__LINE__<<"received frame is:"<<QByteArray((char*)receivedFrame,len);
        switch(this->receivedFrame[0])
        {
            case CmdAck:
            {
                switch (sentcmd)
                {
                    case CmdWrID:
                        WriteKey();
                        break;

                    case CmdWrKey:
                        sentcmd = 0;
                        QMessageBox::about(this,"成功","写入成功");
                        break;

                    default:
                        break;
                }
            }
            break;

            case CmdRdIdAck:
                if(this->isInWrittingState)
                {
                    sentcmd = 0;
                    this->deviceid = QByteArray((char *)receivedFrame+1,len-1);
                    qDebug()<<"deviceid"<<this->deviceid.toHex();
                }
                else
                {
                    this->ui->deviceidlineEdit->setText(QByteArray((char*)receivedFrame+1,28).toHex());

                    this->ui->label_3->setText("设备刷新时间："+QTime::currentTime().toString());
                    QString s = this->ui->deviceidlineEdit->text().mid(14,4);
                    QString devicetype = "设备类型：";

                    devicetype += this->GetDevicetype(s);
                            this->ui->label_4->setText(devicetype);
                }
                QueryKey();
                break;

            case CmdRdKeyAck: // key report frame
                if(this->isInWrittingState)
                {
                    
                    sentcmd = 0;
                    this->key = (QByteArray((char *)receivedFrame+1,16));
                    qDebug()<<"key";
                    emit signalKeyOver();
                }
                else
                {
                    this->ui->keylineEdit->setText(QByteArray((char *)receivedFrame+1,len-1));
                    QueryInfo();
                }
                break;

            case CmdRDInfoAck:
                {
                    sentcmd = 0;

                    int tpm_data;
                    QString str = "";
                    QString str_model_type = " ";
                    QString str_protocol_version = "；协议版本:";
                    QString str_soft_version = "；软件版本:";
                    QString str_hard_version = "；硬件版本:";
                    QString str_encrypted_type = "；";
                    QString str_buffer_length = "；缓冲区长度:";

                    if(receivedFrame[1] == 1)
                    {
                        str_model_type += "WIFI模块";
                    }
                    else if(receivedFrame[1] == 2)
                    {
                        str_model_type += "Zigbee模块";
                    }
                    else if(receivedFrame[1] == 3)
                    {
                        str_model_type += "Thread模块";
                    }
                    else
                    {
                        str_model_type += "未知类型";
                    }

                    str = QString::number(receivedFrame[2],16).toUpper();
                    if(str.toInt(0,16) == 0)
                    {
                        str = "00";
                    }
                    else if((str.toInt()<16)&&(str.toInt()>0))
                    {
                        str = str.insert(0, "0");
                    }
                    str_protocol_version += str;

                    tpm_data = receivedFrame[6]<<24 | receivedFrame[5]<<16 | receivedFrame[4]<<8 | receivedFrame[3];
                    str = QString::number(tpm_data, 16).toUpper();
                    if((0<receivedFrame[6])&&(receivedFrame[6]<16))
                    {
                        str = str.insert(0, "0");
                    }
                    else if(0 == receivedFrame[6])
                    {
                        str = str.insert(0, "00");
                    }
                    str_soft_version += str;

                    tpm_data = receivedFrame[10]<<24 | receivedFrame[9]<<16 | receivedFrame[8]<<8 | receivedFrame[7];
                    str = QString::number(tpm_data, 16).toUpper();
                    if((0<receivedFrame[10])&&(receivedFrame[10]<16))
                    {
                        str = str.insert(0, "0");
                    }
                    else if(0 == receivedFrame[10])
                    {
                        str = str.insert(0, "00");
                    }
                    str_hard_version += str;

                    if(receivedFrame[11] == 0)
                    {
                        str_encrypted_type += "未加密";
                    }
                    else
                    {
                         str_encrypted_type += "未知加密类型";
                    }

                    tpm_data = receivedFrame[14]<<8 | receivedFrame[13];
                    str = QString::number(tpm_data, 16).toUpper();
                    str = str.insert(0,"0x");
                    str_buffer_length += str;
                    this->ui->label_5->setText("设备信息："+str_model_type+str_protocol_version+str_soft_version+str_hard_version+str_encrypted_type+str_buffer_length);
                }
                break;

            default:
                QMessageBox::warning(this,"错误","不能识别的命令");
                break;
        }
    }
}

void MainWindow::slotResend()
{
    if(3 != sentcnt)
    {
        switch (sentcmd)
        {
            case CmdRdId:
                this->QueryId();
                break;
            case CmdRdKey:
                QueryKey();
                break;
            case CmdRdInfo:
                QueryInfo();
                break;
            case CmdWrID:
                WrDeviceID();
                break;
            case CmdWrKey:
                WriteKey();
                break;
            default:
                break;
        }
    }
    else
    {
        EndResend();
        OffLine();
    }
}

void MainWindow::StartResend()
{
    this->timerResend.start(200);
    sentcnt++;
}

void MainWindow::EndResend()
{
    this->timerResend.stop();
    sentcnt = 0;
}

void MainWindow::QueryId()
{
    unsigned char queryFrame[]={0xf5,0xf5,0x00,0x03,CmdRdId,0x55,0x10};
    serialPort.write((char *)queryFrame,7);
    StartResend();
    sentcmd = CmdRdId;
}

void MainWindow::QueryKey()
{
#ifdef CMDCOR
    unsigned char queryFrame[] ={0xf5,0xf5,0x00,0x03,CmdRdKey,0x13,0x34};
#else
    unsigned char queryFrame[] ={0xf5,0xf5,0x00,0x03,CmdRdKey,0xb5,0x26};
#endif
    serialPort.write((char *) queryFrame,7);
    StartResend();
    sentcmd = CmdRdKey;
}

void MainWindow::QueryInfo()
{
    unsigned char queryFrame[] ={0xf5,0xf5,0x00,0x03,CmdRdInfo,0xe2,0x8b};
    serialPort.write((char *) queryFrame,7);
    StartResend();
    sentcmd = CmdRdInfo;
}

void MainWindow::WrDeviceID()
{
    unsigned char dataFrame[60];
    dataFrame[0] = CmdWrID;

    char testdeviceid[]={0x00,0x00,0x00,0x00,0x00,0x00,
                      0x00,0x00,0x00,0x00,0x00,0x00,
                      0x00,0x00,0x00,0x00,0x00,0x00,
                      0x00,0x00,0x00,0x00,0x00,0x00,
                      0x00,0x00,0x00,0x00,0x00,0x00,
                      0x00,0x00,0x00,0x00,0x00,0x00};
    deviceid = QByteArray(testdeviceid,36);
    qDebug()<<this->deviceid;

    memcpy(dataFrame+1,this->writtingDeviceid.data(),this->writtingDeviceid.length());

    unsigned char len = UserUartLinkPack(this->writeFrame,dataFrame,writtingDeviceid.length()+1,0);
    qDebug()<<__LINE__<<QByteArray((char *)this->writeFrame,len).toHex();

    serialPort.write((char *)writeFrame,len);
    StartResend();
    sentcmd = CmdWrID;
}

void MainWindow::WriteKey()
{
    qDebug()<<"写秘钥";
    this->key = "aaaaaaaaaaaaaaaa";
    unsigned char dataFrame[60];
    qDebug()<<this->key;

    dataFrame[0] = CmdWrKey;

    memcpy(dataFrame+1,this->writtingKey.data(),this->writtingKey.length());

    unsigned char len = UserUartLinkPack(this->writeFrame,dataFrame,this->writtingKey.length()+1,0);
    qDebug()<<QByteArray((char *)this->writeFrame,len).toHex();

    serialPort.write((char *)writeFrame,len);
    StartResend();
    sentcmd = CmdWrKey;
}

QString MainWindow::GetDevicetype(QString s)
{
    QString devicetype;
    int t = 0;
    t = s.toInt(0,16);
    switch (t)
    {
        case 0x0101:
            devicetype ="冷藏箱";
            break;
        case 0x0102:
            devicetype ="冷冻箱";
            break;
        case 0x0103:
            devicetype ="冷藏冷冻箱";
            break;
        case 0x0104:
            devicetype ="酒柜";
            break;
        case 0x0105:
            devicetype ="商用冷藏陈列柜";
            break;
        case 0x0106:
            devicetype ="商用冷冻陈列柜";
            break;
        case 0x0107:
            devicetype ="家用玻璃门冷藏柜";
            break;
        case 0x0108:
            devicetype ="冷藏冷冻类其他产品";
            break;
        case 0x0201:
            devicetype ="波轮洗衣机";
            break;
        case 0x0202:
            devicetype ="滚筒洗衣机";
            break;
        case 0x0203:
            devicetype ="干衣机";
            break;
        case 0x0204:
            devicetype ="波轮洗衣干衣机";
            break;
        case 0x0205:
            devicetype ="滚筒洗衣干衣机";
            break;
        case 0x0206:
            devicetype ="洗涤类其他产品";
            break;
        case 0x0301:
            devicetype ="家用空调器";
            break;
        case 0x0302:
            devicetype ="家用中央空调";
            break;
        case 0x0303:
            devicetype ="商用中央空调";
            break;
        case 0x0304:
            devicetype ="家用加湿器";
            break;
        case 0x0305:
            devicetype ="家用净化器";
            break;
        case 0x0306:
            devicetype ="除湿机";
            break;
        case 0x0307:
            devicetype ="风扇";
            break;
        case 0x0308:
            devicetype ="电暖气";
            break;
        case 0x0309:
            devicetype ="换风机";
            break;
        case 0x0310:
            devicetype ="风机盘";
            break;
        case 0x0311:
            devicetype ="空气调节类其他产品";
            break;
        case 0x0401:
            devicetype ="烤箱";
            break;
        case 0x0402:
            devicetype ="微波炉";
            break;
        case 0x0403:
            devicetype ="电饭锅";
            break;
        case 0x0404:
            devicetype ="油烟机";
            break;
        case 0x0405:
            devicetype ="打火灶";
            break;
        case 0x0406:
            devicetype ="煤气阀";
            break;
        case 0x0407:
            devicetype ="消毒柜";
            break;
        case 0x0408:
            devicetype ="豆浆机";
            break;
        case 0x0409:
            devicetype ="洗碗机";
            break;
        case 0x0410:
            devicetype ="厨电类其他产品";
            break;
        case 0501:
            devicetype ="电热水器";
            break;
        case 0502:
            devicetype ="燃气热水器";
            break;
        case 0503:
            devicetype ="太阳能热水器";
            break;
        case 0504:
            devicetype ="热泵热水器";
            break;
        case 0x0505:
            devicetype ="热水中心";
            break;
        case 0x0506:
            devicetype ="水处理类其他产品";
            break;
        case 0x0601:
            devicetype ="智能电视";
            break;
        case 0x0602:
            devicetype ="扫地机器人";
            break;
        case 0x0603:
            devicetype ="灯具";
            break;
        case 0x0604:
            devicetype ="插座";
            break;
        case 0x0605:
            devicetype ="开关";
            break;
        case 0x0606:
            devicetype ="窗帘";
            break;
        case 0x0607:
            devicetype ="门磁";
            break;
        case 0x0608:
            devicetype ="温湿度传感器";
            break;
        case 0x0609:
            devicetype ="马桶";
            break;
        case 0x0610:
            devicetype ="晾衣机";
            break;
        case 0x0611:
            devicetype ="家装类其他产品";
            break;
        case 0x0701:
            devicetype ="摄像头";
            break;
        case 0x0702:
            devicetype ="报警器";
            break;
        case 0x0703:
            devicetype ="门锁";
            break;
        case 0x0704:
            devicetype ="烟雾探测器";
            break;
        case 0x0705:
            devicetype ="燃气探测器";
            break;
        case 0x0706:
            devicetype ="门磁探测器";
            break;
        case 0x0707:
            devicetype ="红外探测器";
            break;
        case 0x0708:
            devicetype ="安防探测器";
            break;
        case 0x0709:
            devicetype ="红外转发器";
            break;
        case 0x0710:
            devicetype ="水位探测器";
            break;
        case 0x0711:
            devicetype ="安防监控类其他产品";
            break;
        case 0x0801:
            devicetype ="体感车";
            break;
        case 0x0802:
            devicetype ="跑步机";
            break;
        case 0x0803:
            devicetype ="健身车";
            break;
        case 0x0804:
            devicetype ="手环";
            break;
        case 0x0805:
            devicetype ="手表";
            break;
        case 0x0806:
            devicetype ="智能秤";
            break;
        case 0x0807:
            devicetype ="血压计";
            break;
        case 0x0808:
            devicetype ="血糖计";
            break;
        case 0x0809:
            devicetype ="体温计";
            break;
        case 0x0810:
            devicetype ="足浴盆";
            break;
        case 0x0811:
            devicetype ="按摩仪";
            break;
        case 0x0812:
            devicetype ="血氧仪";
            break;
        case 0x0813:
            devicetype ="心电仪";
            break;
        case 0x0814:
            devicetype ="健康类其他产品";
            break;
        case 0x0901:
            devicetype ="门口机";
            break;
        case 0x0902:
            devicetype ="室内机";
            break;
        case 0x0903:
            devicetype ="围墙机";
            break;
        case 0x0904:
            devicetype ="HC对讲管理模块";
            break;
        case 0x0905:
            devicetype ="物业管理机";
            break;
        case 0x0906:
            devicetype ="二次确认机";
            break;
        case 0x0907:
            devicetype ="可视对讲类其他产品";
            break;
        default:
            break;
    }
    return devicetype;
}

void MainWindow::OffLine()
{
    QString str;
    switch (sentcmd)
    {
        case CmdRdId:
            str = "读ID";
            break;

        case CmdRdInfo:
            str = "读模块信息";
            break;

        case CmdRdKey:
            str = "读KEY";
            break;

        case CmdWrID:
            str = "写ID";
            break;

        case CmdWrKey:
            str = "写KEY";
            break;

        default:
            break;
    }
    sentcmd = 0;
    QMessageBox::warning(this,"错误",str+"命令未收到回复，设备未连接或协议不支持");         //20161107设备离线弹出窗口统一
}
