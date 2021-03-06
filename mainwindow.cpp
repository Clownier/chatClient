#include "mainwindow.h"
#include "ui_mainwindow.h"
//#include "clientsocket.h"
#include "logindialog.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->pushButton->setVisible(false);
    ui->pushButton_2->setVisible(false);
    ui->pushButton_5->setVisible(false);
    ui->checkBox->setVisible(false);
    mChatClient.create();
    mChatClient.bind();
    mChatClient.connect();
    QString *userName = new QString;
    QString *password = new QString;
    loginDialog login(mChatClient,userName,password,this);
    login.setModal(true);
    if(login.exec() == QDialog::Accepted){
        qDebug()<<"login success!";
        initListWidget();
        std::thread sub(&MainWindow::listenOther,this);
        sub.detach();
        MyUuid = *userName;
        qDebug()<<MyUuid.toStdString().data();

    }
    else
        QTimer::singleShot(0, this, SLOT(close()));
    connect(this,SIGNAL(getData(QString)),this,SLOT(showOtherText(QString)));
}

MainWindow::~MainWindow()
{
    delete ui;
    mChatClient.close();
}
void MainWindow::listenOther(){
    ServerSocket server(mChatClient.serverPort+1);
    while(1){
        aimSocket = server.poll();
        QString address;
        address.sprintf( "the other user from %s:%d visit!\n",
                         inet_ntoa(aimSocket.client_Addr.sin_addr),aimSocket.client_Addr.sin_port);
        qDebug()<<address;
        if(first){
            otherThread = std::thread(&MainWindow::recv,this);
            otherThread.detach();
            first = false;
        }
    }
}
void MainWindow::showOfflineMes(){
    vector<QString>::iterator itor;
    for(itor = offlineMes.begin();itor!=offlineMes.end();itor++){
        QString str = *itor;
        if(str.indexOf(aimUserUuid)==-1)
            continue;
        QJsonArray arr = QJsonDocument::fromJson(str.toLatin1(),NULL).array();

        if(arr.at(1).toString()==MyUuid){
            ui->widgetChat->addItem(arr.at(3).toString(),1);
        }else{
            ui->widgetChat->addItem(arr.at(3).toString(),2);
        }
    }
}

void MainWindow::recv(){
    while(1){
        QString text = aimSocket.Qrecv();
        if(text.length() == 0){
            continue;
        }
        QJsonParseError *error = new QJsonParseError;
        QJsonArray array = QJsonDocument::fromJson(text.toLatin1(),error).array();
        if(array.at(0).toInt() == 327){
            QString Name = array.at(1).toString();
            Name.append("(online)");
            ui->currentUser->setText(Name);
            ui->widgetChat->clear();
            aimUserOnline = true;
        }else if(array.at(0).toInt() == 328){
            QString text = array.at(1).toString();
            emit getData(text);
        }
    }
}

void MainWindow::showOtherText(QString text){
    QJsonArray arr;arr.insert(0,328);
    arr.insert(1,aimUserUuid);arr.insert(2,MyUuid);
    arr.insert(3,text);QJsonDocument doc;
    doc.setArray(arr);
    offlineMes.push_back(doc.toJson(QJsonDocument::Compact));
    ui->widgetChat->addItem(text,1);
}

QString MainWindow::recvInfo(){
    QString res;
    while(true){
        QString ack;
        while(ack.length()==0){
            ack = mChatClient.Qrecv();
        }
        qDebug()<<"ack = "<<ack<<"\n";
        QJsonParseError *error = new QJsonParseError;
        QJsonArray array = QJsonDocument::fromJson(ack.toLatin1(),error).array();
        qDebug()<<error->errorString();
        if(array.at(0).toInt() == 584){
            offlineMes.push_back(ack);
        }
        else if(array.at(0).toInt() == 258){
            res.append(array.at(1).toString());
        }else if(array.at(0).toInt() == 259){
            res.append(array.at(1).toString());
            break;
        }
    }
    return res;
}

void MainWindow::initListWidget(){
    QString Info = recvInfo();
    userList=ui->userListWidget;
    QStringList strlist = Info.split("#");
    QString info = strlist.at(0);
    userList->addItem("CoolChat小助手{00000}");
    userList->addItems(info.split(";"));
    MyName = strlist.at(1);
//    while(true){
//        QString ack;
//        while(ack.length()==0){
//            ack = mChatClient.Qrecv();
//        }
//        qDebug()<<"ack = "<<ack<<"\n";
//        QJsonParseError *error = new QJsonParseError;
//        QJsonArray array = QJsonDocument::fromJson(ack.toLatin1(),error).array();
//        qDebug()<<error->errorString();
//        /*if(array.at(0).toInt() == 257){
//            offlineMes.push_back(ack);
//        }
//        else*/ if(array.at(0).toInt() == 583){
//            offlineMes.push_back(ack);
//        }else if(array.at(0).toInt() == 256){
//            break;
//        }
//    }
}

void MainWindow::on_userListWidget_currentTextChanged(const QString &currentText)
{
    //点击用户，将界面刷新
    //TODO 将用户id传到服务器,返回用户是否在线
    ui->widgetChat->clear();
    qDebug()<<currentText.toStdString().data()<<"is clicked!";
    if(currentText == "CoolChat小助手{00000}"){
        ui->currentUser->setText(currentText);
        aimUserOnline = false;
        return;
    }
    int  index = currentText.indexOf("{");
    aimUserUuid = currentText.mid(index,currentText.length()-index);
    QJsonArray arr;arr.insert(0,583);arr.insert(1,aimUserUuid);
    QJsonDocument doc;doc.setArray(arr);
    QString send = doc.toJson(QJsonDocument::Compact);
    mChatClient.Qsend(send);


    QString req = mChatClient.Qrecv();
    while(req.length() == 0)
        req = mChatClient.Qrecv();
    QJsonParseError *error = new QJsonParseError;
    QJsonArray array = QJsonDocument::fromJson(req.toLatin1(),error).array();
    QString showText = currentText.mid(0,index);
    if(array.at(0)==583){
        aimUserOnline = true;
        showText.append("(online)");
        QString aimIp = array.at(1).toString();
        int aimPort = array.at(2).toInt();
        aimSocket.create();
        aimSocket.bind();
        aimSocket.connect(aimIp,aimPort+1);
        QJsonArray arr;arr.insert(0,327);arr.insert(1,MyName);
        QJsonDocument doc;doc.setArray(arr);
        QString send = doc.toJson(QJsonDocument::Compact);
        aimSocket.Qsend(send);
        if(first){
            otherThread = std::thread(&MainWindow::recv,this);
            otherThread.detach();
            first = false;
        }
    }
    else{
        aimUserOnline = false;
        showText.append("(offline)");
    }

    ui->currentUser->setText(showText);
    showOfflineMes();
}

void MainWindow::on_sendText_clicked()
{
    QString send = ui->textEditSnd->toPlainText();
    ui->widgetChat->addItem(send,2);
    QJsonArray arr;
    if(!aimUserOnline){
        //将消息发送到服务器
        arr.insert(0,584);
        arr.insert(1,aimUserUuid);arr.insert(2,MyUuid);
        arr.insert(3,send);QJsonDocument doc;
        doc.setArray(arr);
        send = doc.toJson(QJsonDocument::Compact);
        mChatClient.Qsend(send);
        offlineMes.push_back(send);
//        ui->widgetChat->addItem("this is once",1);
    }else{
        arr.insert(0,328);arr.insert(1,send);
        QJsonDocument doc;doc.setArray(arr);
        send = doc.toJson(QJsonDocument::Compact);
        aimSocket.Qsend(send);
        arr.replace(1,aimUserUuid);
        arr.insert(2,MyUuid);
        arr.insert(3,ui->textEditSnd->toPlainText());
        doc.setArray(arr);
        offlineMes.push_back(doc.toJson(QJsonDocument::Compact));
    }
}
