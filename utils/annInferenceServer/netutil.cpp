#include "netutil.h"
#include "common.h"
#include <stdio.h>
#include <stdarg.h>

#define INFCOM_DEBUG_DUMP      0 // for debugging network protocol
#define INFCOM_ENABLE_NODELAY  0 // for debugging network protocol

int sendPacket(int sock, const void * buf, size_t len, std::string& clientName)
{
#if INFCOM_ENABLE_NODELAY
    int one = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *) &one, sizeof(one));
#endif
    size_t n = send(sock, buf, len, 0);
    if(n != len) {
        return error_close(sock, "send(len:%ld) failed for %s (sent %ld bytes)", len, clientName.c_str(), n);
    }
#if INFCOM_ENABLE_NODELAY
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *) &one, sizeof(one));
#endif
    return 0;
}

int recvPacket(int sock, void * buf, size_t len, std::string& clientName)
{
    size_t n = recv(sock, buf, len, 0);
    if(n == 0) {
        info("recv(len:%ld) from %s (got %ld bytes) -- disconnecting ...", len, clientName.c_str(), n);
        close(sock);
        return -1;
    }
    else if(n != len) {
        return error_close(sock, "recv(len:%ld) failed for %s (received %ld bytes)", len, clientName.c_str(), n);
    }
    return 0;
}

int recvBuffer(int sock, void * buf, size_t len, std::string& clientName)
{
    char * byteStream = (char *) buf;
    int remaining = len;
    while(remaining > 0) {
        int pktSize = std::min(remaining, INFCOM_MAX_PACKET_SIZE);
        int n = recv(sock, byteStream, pktSize, 0);
        if(n < 1)
            break;
        remaining -= n;
        byteStream += n;
    }
    if(remaining > 0) {
        return error_close(sock, "recv(len:%ld) failed for %s (received %ld bytes)", len, clientName.c_str(), len - remaining);
    }
    return 0;
}

int sendCommand(int sock, const InfComCommand& cmd, std::string& clientName)
{
#if INFCOM_DEBUG_DUMP
    dumpCommand("sendCommand", cmd);
#endif
    return sendPacket(sock, &cmd, sizeof(cmd), clientName);
}

int recvCommand(int sock, InfComCommand& cmd, std::string& clientName, int expectedCommand)
{
    if(recvPacket(sock, &cmd, sizeof(cmd), clientName) < 0)
        return -1;

    if(cmd.magic != INFCOM_MAGIC) {
        return error_close(sock, "recv() incorrect InfComCommand from %s (magic is 0x%08x instead of 0x%08x)", clientName.c_str(), cmd.magic, INFCOM_MAGIC);
    }

    if(expectedCommand >= 0 && expectedCommand != cmd.command) {
        return error_close(sock, "recv() incorrect InfComCommand from %s (command is 0x%08x instead of 0x%08x)", clientName.c_str(), cmd.command, expectedCommand);
    }

#if INFCOM_DEBUG_DUMP
    dumpCommand("recvCommand", cmd);
#endif
    return 0;
}

void dumpCommand(const char * mesg, const InfComCommand& cmd)
{
    info("InfComCommand: %s 0x%08x %8d { %d %d - %d %d %d - %d %d %d - %d %d } %s", mesg,
        cmd.magic, cmd.command,
        cmd.data[0], cmd.data[1],
        cmd.data[2], cmd.data[3], cmd.data[4],
        cmd.data[5], cmd.data[6], cmd.data[7],
        cmd.data[8], cmd.data[9],
        cmd.message);
}

int error_close(int sock, const char * format, ...)
{
    char text[1024];
    va_list args;
    va_start(args, format);
    int r = vsprintf(text, format, args);
    va_end(args);
    printf("ERROR: %s\n", text);
    InfComCommand cmd = { INFCOM_MAGIC, INFCOM_CMD_DONE, { -1 }, { 0 } };
    sprintf(cmd.message, "%.40s", text);
    std::string clientName = "--";
    sendCommand(sock, cmd, clientName);
    recvCommand(sock, cmd, clientName, INFCOM_CMD_DONE);
    close(sock);
    return -1;
}
