#include "inference_receiver.h"
#include "inference_comm.h"
#include <QThread>
#include <QTcpSocket>

bool inference_receiver::abortRequsted = false;

void inference_receiver::abort()
{
    abortRequsted = true;
}

inference_receiver::inference_receiver(
        QString serverHost_, int serverPort_, QString modelName_,
        int GPUs_, int * inputDim_, int * outputDim_, const char * runtimeOptions_,
        QVector<QByteArray> * imageBuffer_,
        runtime_receiver_status * progress_,
        QObject *parent) : QObject(parent)
{
    perfRate = 0;
    perfImageCount = 0;
    perfTimer.start();
    imageCount = 0;
    labelCount = 0;
    imageBuffer = imageBuffer_;
    serverHost = serverHost_;
    serverPort = serverPort_;
    modelName = modelName_;
    GPUs = GPUs_;
    inputDim = inputDim_;
    outputDim = outputDim_;
    runtimeOptions = runtimeOptions_;
    progress = progress_;
}

inference_receiver::~inference_receiver()
{

}

void inference_receiver::getReceivedList(QVector<int>& indexQ, QVector<int>& labelQ, QVector<QString>& summaryQ)
{
    std::lock_guard<std::mutex> guard(mutex);
    while(imageIndex.length() > 0) {
        indexQ.push_back(imageIndex.front());
        labelQ.push_back(imageLabel.front());
        summaryQ.push_back(imageSummary.front());
        imageIndex.pop_front();
        imageLabel.pop_front();
        imageSummary.pop_front();
    }
}

void inference_receiver::run()
{
    // connect to the server for inference run-time mode
    //    - configure the connection in inference run-time mode
    //    - keep sending images and tag if server can accept more work
    //    - when results are received add the results to imageIndex, imageLabel, imageSummary queues

    progress->images_sent = 0;
    progress->images_received = 0;
    progress->completed_send = false;
    progress->completed = false;

    QTcpSocket * tcpSocket = new QTcpSocket(this);
    tcpSocket->connectToHost(serverHost, serverPort);
    if(tcpSocket->waitForConnected(3000)) {
        int nextImageToSend = 0;
        while(tcpSocket->state() == QAbstractSocket::ConnectedState) {
            if(abortRequsted)
                break;
            bool receivedCommand = false;
            if(tcpSocket->waitForReadyRead()) {
                InfComCommand cmd;
                while(tcpSocket->bytesAvailable() >= (qint64)sizeof(cmd) &&
                      tcpSocket->read((char *)&cmd, sizeof(cmd)) == sizeof(cmd))
                {
                    receivedCommand = true;
                    if(abortRequsted)
                        break;
                    if(cmd.magic != INFCOM_MAGIC) {
                        progress->errorCode = -1;
                        progress->message.sprintf("ERROR: got invalid magic 0x%08x", cmd.magic);
                        break;
                    }
                    auto send = [](QTcpSocket * sock, runtime_receiver_status * progress, const void * buf, size_t len) -> bool {
                        sock->write((const char *)buf, len);
                        if(!sock->waitForBytesWritten(3000)) {
                            progress->errorCode = -1;
                            progress->message.sprintf("ERROR: write(%ld) failed", len);
                            return false;
                        }
                        return true;
                    };
                    auto sendImage = [](QTcpSocket * sock, runtime_receiver_status * progress, QByteArray& byteArray, int tag) -> bool {
                        const char * buf = byteArray.constData();
                        int len = byteArray.size();
                        int header[2] = { tag, len };
                        sock->write((const char *)&header[0], sizeof(header));
                        if(!sock->waitForBytesWritten()) {
                            progress->errorCode = -1;
                            progress->message.sprintf("ERROR: sendImage: write(header:%ld) - tag:%d", sizeof(header), tag);
                            return false;
                        }
                        int pos = 0;
                        while(pos < len) {
                            if(abortRequsted)
                                break;
                            int pktSize = std::min(INFCOM_MAX_PACKET_SIZE, len-pos);
                            sock->write(&buf[pos], pktSize);
                            if(!sock->waitForBytesWritten()) {
                                progress->errorCode = -1;
                                progress->message.sprintf("ERROR: sendImage: write(pkt:%d) failed after %d/%d bytes - tag:%d", pktSize, pos, len, tag);
                                return false;
                            }
                            pos += pktSize;
                        }
                        int eofMarked = INFCOM_EOF_MARKER;
                        sock->write((const char *)&eofMarked, sizeof(eofMarked));
                        if(!sock->waitForBytesWritten()) {
                            progress->errorCode = -1;
                            progress->message.sprintf("ERROR: sendImage: write(eofMarked:%ld) - tag:%d", sizeof(eofMarked), tag);
                            return false;
                        }
                        return true;
                    };
                    if(cmd.command == INFCOM_CMD_DONE) {
                        break;
                    }
                    else if(cmd.command == INFCOM_CMD_SEND_MODE) {
                        InfComCommand reply = {
                            INFCOM_MAGIC, INFCOM_CMD_SEND_MODE,
                            { INFCOM_MODE_INFERENCE, GPUs,
                              inputDim[0], inputDim[1], inputDim[2], outputDim[0], outputDim[1], outputDim[2] },
                            { 0 }
                        };
                        QString text = modelName;
                        if(runtimeOptions || *runtimeOptions) {
                            text += " ";
                            text += runtimeOptions;
                        }
                        strncpy(reply.message, text.toStdString().c_str(), sizeof(reply.message));
                        if(!send(tcpSocket, progress, &reply, sizeof(reply)))
                            break;
                    }
                    else if(cmd.command == INFCOM_CMD_SEND_IMAGES) {
                        int count_requested = cmd.data[0];
                        int count = std::min(imageCount - nextImageToSend, count_requested);
                        InfComCommand reply = {
                            INFCOM_MAGIC, INFCOM_CMD_SEND_IMAGES,
                            { count },
                            { 0 }
                        };
                        if(!send(tcpSocket, progress, &reply, sizeof(reply)))
                            break;
                        bool failed = false;
                        for(int i = 0; i < count; i++) {
                            // send the image at nextImageToSend
                            if(!sendImage(tcpSocket, progress, (*imageBuffer)[nextImageToSend], nextImageToSend)) {
                                failed = true;
                                break;
                            }
                            // update nextImageToSend
                            nextImageToSend++;
                            progress->images_sent++;
                        }
                        if(failed)
                            break;
                        if(nextImageToSend >= imageCount) {
                            if(progress->repeat_images) {
                                nextImageToSend = 0;
                            }
                            else if(progress->completed_load && progress->images_loaded == progress->images_sent) {
                                progress->completed_send = true;
                            }
                        }
                    }
                    else if(cmd.command == INFCOM_CMD_INFERENCE_RESULT) {
                        int count = cmd.data[0];
                        int status = cmd.data[1];
                        if(status == 0 && count > 0 && count < (int)((sizeof(cmd)-16)/(2*sizeof(int)))) {
                            std::lock_guard<std::mutex> guard(mutex);
                            for(int i = 0; i < count; i++) {
                                int tag = cmd.data[2 + 2*i + 0];
                                int label = cmd.data[2 + 2*i + 1];
                                imageIndex.push_back(tag);
                                imageLabel.push_back(label);
                                imageSummary.push_back((*labelName)[label]);
                                perfImageCount++;
                                progress->images_received++;
                            }
                            if(!progress->repeat_images && progress->completed_load &&
                                progress->images_loaded == progress->images_received)
                            {
                                abort();
                            }
                        }
                    }
                    else {
                        progress->errorCode = -1;
                        progress->message.sprintf("ERROR: got invalid command 0x%08x", cmd.command);
                        break;
                    }
                }
            }
            if(!receivedCommand) {
                QThread::msleep(2);
            }
        }
    }
    else {
        progress->errorCode = -1;
        progress->message.sprintf("ERROR: Unable to connect to %s:%d", serverHost.toStdString().c_str(), serverPort);
    }
    if(abortRequsted)
        progress->message += " [aborted]";
    tcpSocket->close();
    progress->completed = true;

    if(progress->errorCode) {
        qDebug("inference_receiver::run() terminated: errorCode=%d", progress->errorCode);
    }
}

float inference_receiver::getPerfImagesPerSecond()
{
    std::lock_guard<std::mutex> guard(mutex);
    qint64 msec = perfTimer.elapsed();
    if(!progress->completed && msec > 2000) {
        perfRate = (float)perfImageCount * 1000.0 / (float)msec;
        perfImageCount = 0;
        perfTimer.start();
    }
    return perfRate;
}

void inference_receiver::setImageCount(int imageCount_, int labelCount_, QVector<QString> * labelName_)
{
    imageCount = imageCount_;
    labelCount = labelCount_;
    labelName = labelName_;
}
