import os
import sys
import socket
import struct

# InfComCommand
INFCOM_MAGIC                         = 0x02388e50
INFCOM_CMD_DONE                      = 0
INFCOM_CMD_SEND_MODE                 = 1
INFCOM_CMD_CONFIG_INFO               = 101
INFCOM_CMD_MODEL_INFO                = 102
INFCOM_CMD_SEND_MODELFILE1           = 201
INFCOM_CMD_SEND_MODELFILE2           = 202
INFCOM_CMD_COMPILER_STATUS           = 203
INFCOM_CMD_INFERENCE_INITIALIZATION  = 301
INFCOM_CMD_SEND_IMAGES               = 302
INFCOM_CMD_INFERENCE_RESULT          = 303
INFCOM_MODE_CONFIGURE                = 1
INFCOM_MODE_COMPILER                 = 2
INFCOM_MODE_INFERENCE                = 3
INFCOM_EOF_MARKER                    = 0x12344321
INFCOM_MAX_IMAGES_PER_PACKET         = 6
INFCOM_MAX_PACKET_SIZE               = 8192

# process command-lines
if len(sys.argv) < 2:
    print('Usage: python annInferenceApp.py [-v] [-host:<hostname>] [-port:<port>] -model:<modelName> [-synset:<synset.txt>] [-output:<output.csv>] <folder>|<file(s)>')
    sys.exit(1)
host = 'localhost'
port = 28282
modelName = ''
imageDirPath = ''
imageFileList = []
outputFileName = None
synsetFileName = None
verbose = False
arg = 1
while arg < len(sys.argv):
    if sys.argv[arg][:6] == '-host:':
        host = sys.argv[arg][6:]
        arg = arg + 1
    elif sys.argv[arg][:6] == '-port:':
        port = int(sys.argv[arg][6:])
        arg = arg + 1
    elif sys.argv[arg][:7] == '-model:':
        modelName = sys.argv[arg][7:]
        arg = arg + 1
    elif sys.argv[arg][:8] == '-output:':
        outputFileName = sys.argv[arg][8:]
        arg = arg + 1
    elif sys.argv[arg][:8] == '-synset:':
        synsetFileName = sys.argv[arg][8:]
        arg = arg + 1
    elif sys.argv[arg] == '-v':
        verbose = True
        arg = arg + 1
    elif sys.argv[arg][:1] == '-':
        print('ERROR: invalid option: ' + sys.argv[arg])
        sys.exit(1)
    else:
        break
if arg == len(sys.argv)-1:
    if os.path.isdir(sys.argv[arg]):
        imageDirPath = sys.argv[arg]
        imageFileList = os.listdir(imageDirPath)
        if imageDirPath[-1] != '/':
            imageDirPath = imageDirPath + '/'
    else:
        imageFileList = [sys.argv[arg]]
elif arg < len(sys.argv)-1:
    imageFileList = sys.argv[arg:]

def recvpkt(sock):
    data = sock.recv(128)
    if len(data) != 128:
        return (0,0,(0,),'')
    msg = ''
    for c in data[64:]:
        if c == chr(0):
            break
        msg = msg + c
    info = (struct.unpack('i', data[:4])[0],struct.unpack('i', data[4:8])[0],struct.unpack('i'*14, data[8:64]),msg)
    #print('RECV' + str(info))
    return info

def sendpkt(sock,pkt):
    #print('SEND' + str(pkt))
    data =        struct.pack('i',pkt[0])
    data = data + struct.pack('i',pkt[1])
    vl = pkt[2]
    for i in range(14):
        v = 0
        if i < len(vl):
            v = vl[i]
        data = data + struct.pack('i',v)
    vl = pkt[3]
    if len(vl) < 64:
        vl = vl + chr(0) * (64-len(vl))
    data = data + vl
    sock.send(data)

def sendImageFile(sock,tag,fileName):
    fp = open(fileName,'r')
    buf = fp.read()
    fp.close()
    sock.send(struct.pack('ii',tag,len(buf)) + buf + struct.pack('i',INFCOM_EOF_MARKER))

def getConfig(host,port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.connect((host, port))
    except:
        print('ERROR: unable to connect to %s:%d' % (host,port))
        sys.exit(1)
    maxGPUs = 0
    numModels = 0
    modelList = []
    while True:
        info = recvpkt(sock)
        if info[0] != INFCOM_MAGIC:
            print('RECV',info)
            print('ERROR: missing INFCOM_MAGIC')
            break
        if info[1] == INFCOM_CMD_DONE:
            sendpkt(sock,(INFCOM_MAGIC,INFCOM_CMD_DONE,(0,),''))
            break
        elif info[1] == INFCOM_CMD_SEND_MODE:
            sendpkt(sock,(INFCOM_MAGIC,INFCOM_CMD_SEND_MODE,(INFCOM_MODE_CONFIGURE,),''))
        elif info[1] == INFCOM_CMD_CONFIG_INFO:
            sendpkt(sock,info)
            numModels = info[2][0]
            maxGPUs = info[2][1]
            modelCount = 0
        elif info[1] == INFCOM_CMD_MODEL_INFO:
            sendpkt(sock,info)
            model = [info[3], info[2][:3], info[2][3:6], info[2][6], struct.unpack('f'*6,struct.pack('i'*6,*info[2][7:13]))]
            modelList.append(model)
            modelCount = modelCount + 1
            if modelCount >= numModels:
                sendpkt(sock,(INFCOM_MAGIC,INFCOM_CMD_DONE,(0,),''))
                break
        else:
            sendpkt(sock,info)
            print(info)
            print('ERROR: unsupported command')
            break
    sock.close()
    return [maxGPUs, modelList]

def runInference(host,port,GPUs,model,imageDirPath,imageFileList,synsetFileName,outputFileName,verbose):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.connect((host, port))
    except:
        print('ERROR: unable to connect to %s:%d' % (host,port))
        sys.exit(1)
    sendCount = 0
    resultCount = 0
    resultList = [-1] * len(imageFileList)
    if synsetFileName:
        fp = open(synsetFileName, 'r')
        synsetList = fp.readlines()
        synsetList = [x.strip() for x in synsetList]
        fp.close()
    fp = None
    if outputFileName:
        fp = open(outputFileName, 'w')
    while True:
        info = recvpkt(sock)
        if info[0] != INFCOM_MAGIC:
            print('RECV',info)
            print('ERROR: missing INFCOM_MAGIC')
            break
        if info[1] == INFCOM_CMD_DONE:
            sendpkt(sock,(INFCOM_MAGIC,INFCOM_CMD_DONE,(0,),''))
            break
        elif info[1] == INFCOM_CMD_SEND_MODE:
            sendpkt(sock,(INFCOM_MAGIC,INFCOM_CMD_SEND_MODE,(INFCOM_MODE_INFERENCE,GPUs,model[1][0],model[1][1],model[1][2],model[2][0],model[2][1],model[2][2]),model[0]))
        elif info[1] == INFCOM_CMD_INFERENCE_INITIALIZATION:
            sendpkt(sock,info)
            print('OK: ' + info[3])
        elif info[1] == INFCOM_CMD_SEND_IMAGES:
            if sendCount >= len(imageFileList):
                sendpkt(sock,(INFCOM_MAGIC,INFCOM_CMD_SEND_IMAGES,(-1,),''))
            else:
                sendpkt(sock,(INFCOM_MAGIC,INFCOM_CMD_SEND_IMAGES,(1,),''))
                tag = sendCount
                sendImageFile(sock,tag,imageDirPath + imageFileList[tag])
                sendCount = sendCount + 1
        elif info[1] == INFCOM_CMD_INFERENCE_RESULT:
            sendpkt(sock,info)
            count = info[2][0]
            status = info[2][1]
            if status != 0:
                print('ERROR: INFERENCE ' + str(info))
                break
            for i in range(count):
                tag = info[2][2 + i * 2 + 0]
                label = info[2][2 + i * 2 + 1]
                if tag >= 0:
                    resultList[tag] = label
                    resultCount = resultCount + 1
                    line = '%s,%d' % (imageFileList[tag], label)
                    if synsetFileName:
                        line = line + ',' + synsetList[label]
                    if fp:
                        fp.write(line + '\n')
                    if verbose or not fp:
                        print(line)
                else:
                    print('RECEIVED INCORRECT RESULT: ', tag, label)
            if resultCount >= len(imageFileList):
                sendpkt(sock,(INFCOM_MAGIC,INFCOM_CMD_DONE,(0,),''))
                break
        else:
            sendpkt(sock,info)
            print(info)
            print('ERROR: unsupported command')
            break
    sock.close()
    if fp:
        fp.close()
    return resultList

# get configuration from server
config = getConfig(host,port)
GPUs = config[0]
if modelName == '':
    print('OK: server has ' + str(GPUs) + ' GPUs')
    for v in config[1]:
        print('OK: server has model ' + str(v))
    sys.exit(0)
model = []
for v in config[1]:
    if v[0] == modelName:
        model = v
        break
if len(model) == 0:
    print(config)
    print('ERROR: unable to find model: ' + modelName)
    sys.exit(1)
print('OK: found model ' + str(model))

# run inference
if len(imageFileList) > 0:
    runInference(host,port,GPUs,model,imageDirPath,imageFileList,synsetFileName,outputFileName,verbose)
    if outputFileName:
        print('OK: saved inference results in ' + outputFileName)
