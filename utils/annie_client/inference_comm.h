#ifndef INFERENCE_COMM_H
#define INFERENCE_COMM_H

// Enable/disable INFCOM usage
#define INFCOM_ENABLED    0

// Compiler Protocol:
//    client: (connect)
//  * server: InfComCommand:INFCOM_CMD_SEND_MODE
//    client: InfComCommand:INFCOM_CMD_SEND_MODE with data={INFCOM_MODE_COMPILER,GPUs,W,H,C,N}
//  * server: InfComCommand:INFCOM_CMD_SEND_PROTOTXT
//    client: InfComCommand:INFCOM_CMD_SEND_PROTOTXT with data[0]=size-in-bytes message=fileName
//    client: <byte-stream-of-prototxt> <eof-marker:32-bit>
//  * server: InfComCommand:INFCOM_CMD_SEND_CAFFEMODEL
//    client: InfComCommand:INFCOM_CMD_SEND_CAFFEMODEL with data[0]=size-in-bytes message=fileName
//    client: <byte-stream-of-caffemodel> <eof-marker:32-bit>
//  * server: InfComCommand:INFCOM_CMD_COMPILER_STATUS with data={err,progress,OW,OH,OC,ON} message=log
//  * server: InfComCommand:INFCOM_CMD_COMPILER_STATUS with data={err,progress,OW,OH,OC,ON} message=log
//  * server: InfComCommand:INFCOM_CMD_COMPILER_STATUS with data={err,progress,OW,OH,OC,ON} message=log
//  * server: (same type of messages with status updates)
//  * server: InfComCommand:INFCOM_CMD_COMPILER_STATUS with data={err,progress,OW,OH,OC,ON} message=log
//  * server: InfComCommand:INFCOM_CMD_DONE
//    client: (disconnect)

// Runtime Protocol:
//    client: (connect)
//  * server: InfComCommand:INFCOM_CMD_SEND_MODE
//    client: InfComCommand:INFCOM_CMD_SEND_MODE with data={INFCOM_MODE_RUNTIME,GPUs,W,H,C,N,OW,OH,OC,ON}
//  * server: InfComCommand:INFCOM_CMD_SEND_IMAGES with data={imageCount}
//    client: InfComCommand:INFCOM_CMD_SEND_IMAGES with data={imageCount}
//    client: for each image: { <tag:32-bit> <size:32-bit> <byte-stream> <eof-marker:32-bit> }
//  * server: InfComCommand:INFCOM_CMD_INFERENCE_RESULT data={imageCount,0,<tag1>,<label1>,<tag2>,<label2>,...} upto 14 tags
//  * server: (repeat of INFCOM_CMD_SEND_IMAGES and INFCOM_CMD_INFERENCE_RESULT messages)
//  * server: InfComCommand:INFCOM_CMD_DONE
//    client: (disconnect)

// InfComCommand.magic
#define INFCOM_MAGIC                           0x02388e50

// InfComCommand.command
#define INFCOM_CMD_DONE                        0
#define INFCOM_CMD_SEND_MODE                   1
#define INFCOM_CMD_SEND_PROTOTXT               101
#define INFCOM_CMD_SEND_CAFFEMODEL             102
#define INFCOM_CMD_COMPILER_STATUS             103
#define INFCOM_CMD_SEND_IMAGES                 201
#define INFCOM_CMD_INFERENCE_RESULT            202

// InfComCommand.data[0] for INFCOM_CMD_SEND_MODE
#define INFCOM_MODE_COMPILER                   1
#define INFCOM_MODE_RUNTIME                    2

// Max Packet Size and EOF marker
#define INFCOM_MAX_PACKET_SIZE                 8192
#define INFCOM_EOF_MARKER                      0x12344321

// InfComCommand for message exchange
typedef struct {
    int magic;
    int command;
    int data[14];
    char message[64];
} InfComCommand;

#endif // INFERENCE_COMM_H
