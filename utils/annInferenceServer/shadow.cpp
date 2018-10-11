#include <sys/stat.h>
#include "shadow.h"
#include "netutil.h"
#include "common.h"
#include <errno.h>
#include <iostream>
#include <algorithm>
#include "lmdb.h"
#define LMDB_MAP_SIZE   (1 * 1024 * 1024 * 1024)    // 1GB

int createPath( mode_t mode, const std::string& rootPath, std::string path )
{
    struct stat st;
    for( std::string::iterator iter = path.begin() ; iter != path.end(); )
    {
         std::string::iterator newIter = std::find( iter, path.end(), '/' );
         std::string newPath = rootPath + "/" + std::string( path.begin(), newIter);

         if( stat( newPath.c_str(), &st) != 0)
         {
             if( mkdir( newPath.c_str(), mode) != 0 && errno != EEXIST )
             {
                std::cout << "cannot create folder [" << newPath << "] : " << strerror(errno) << std::endl;
                return -1;
             }
         }
         else
            if( !S_ISDIR(st.st_mode) )
             {
                 errno = ENOTDIR;
                 std:: cout << "path [" << newPath << "] not a dir " << std::endl;
                 return -1;
             }
             else
                 std::cout << "path [" << newPath << "] already exists " << std::endl;


         iter = newIter;
         if( newIter != path.end() )
             ++ iter;
    }
    return 0;
}

int runShadow(int sock, Arguments * args, std::string& clientName, InfComCommand * cmd)
{
    MDB_env *lmdbEnv[8] = { 0 };        // to store upto 8 LMDBs
    std::string shadowFolder = args->getlocalShadowRootDir();
    int folderMode = args->getlocalLmdbName().empty()? 0: 1;
    if (shadowFolder.empty())
    {
        return error_close(sock, "runShadow: Server is not running in shadow mode");
        return -1;
    }

    for(bool endOfSequence = false; !endOfSequence; ) {
        InfComCommand cmd = {
            INFCOM_MAGIC, INFCOM_CMD_SHADOW_SEND_FOLDERNAMES, { 6 }, { 0 }      // limit to 6 because max 6 result can fit into data
        };
        ERRCHK(sendCommand(sock, cmd, clientName));
        ERRCHK(recvCommand(sock, cmd, clientName, INFCOM_CMD_SHADOW_SEND_FOLDERNAMES));
        int folderCountReceived = cmd.data[0];
        if (folderCountReceived <= 0) endOfSequence = true;
        if (folderCountReceived > 0) {
            int resultCount = folderCountReceived;
            InfComCommand result_cmd = {
                INFCOM_MAGIC, INFCOM_CMD_SHADOW_RESULT, { resultCount, INFCOM_CMD_SHADOW_SEND_FOLDERNAMES }, { 0 }
            };
            for(int i=0; i < resultCount; i++) {
                int header[2] = { 0, 0 };
                ERRCHK(recvBuffer(sock, &header, sizeof(header), clientName));
                int folder_tag = header[0];
                int size = header[1];
                result_cmd.data[2+i*2+0] = folder_tag;
                result_cmd.data[2+i*2+1] = -1;           // not present
                if (folder_tag < 0)
                {
                    result_cmd.data[2+i*2+1] = 0;
                    result_cmd.data[2+i*2+0] = folder_tag;
                    endOfSequence = true;
                    resultCount = i;
                    break;
                }
                // do sanity check with unreasonable parameters
                if(size <= 0 || size > 1024) {
                    return error_close(sock, "invalid shadowfolder(tag:%d,size:%d) from %s", folder_tag, size, clientName.c_str());
                }
                std::string folderNameDir = args->getlocalShadowRootDir() + "/";
                char * buff = new char [size];
                ERRCHK(recvBuffer(sock, buff, size, clientName));
                folderNameDir.append(std::string(buff, size));
                struct stat sb;
                if (stat(folderNameDir.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode))
                {
                    result_cmd.data[2+i*2+1] = 0;
                }
                int eofMarker = 0;
                ERRCHK(recvBuffer(sock, &eofMarker, sizeof(eofMarker), clientName));
                if(eofMarker != INFCOM_EOF_MARKER) {
                    return error_close(sock, "shadowfolder: eofMarker 0x%08x (incorrect)", eofMarker);
                }
            }
            if(resultCount > 0) {
                result_cmd.data[0] = resultCount;
                ERRCHK(sendCommand(sock, result_cmd, clientName));
                ERRCHK(recvCommand(sock, result_cmd, clientName, INFCOM_CMD_SHADOW_RESULT));
            }
        }
    }
    // create Folder
    for(bool endOfSequence = false; !endOfSequence; ) {

        int cmdName = folderMode ? INFCOM_CMD_SHADOW_CREATE_LMDB:INFCOM_CMD_SHADOW_CREATE_FOLDER;

        InfComCommand cmd = {
            INFCOM_MAGIC, cmdName, { 6 }, { 0 }     // limit to 6 because max 6 <tag,present> fits into data
        };
        ERRCHK(sendCommand(sock, cmd, clientName));
        ERRCHK(recvCommand(sock, cmd, clientName, cmdName));
        int folderCountReceived = cmd.data[0];
        if (folderCountReceived <= 0) endOfSequence = true;
        if (folderCountReceived > 0)
        {
            int resultCount = folderCountReceived;
            InfComCommand result_cmd = {
                INFCOM_MAGIC, INFCOM_CMD_SHADOW_RESULT, { resultCount, cmdName }, { 0 }
            };
            for(int i=0; i < resultCount; i++) {
                int header[2] = { 0, 0 };
                ERRCHK(recvBuffer(sock, &header, sizeof(header), clientName));
                int folder_tag = header[0];
                int size = header[1];
                result_cmd.data[2+i*2+0] = folder_tag;
                result_cmd.data[2+i*2+1] = -1;           // not present
                if (folder_tag < 0)
                {
                    result_cmd.data[2+i*2+1] = 0;
                    result_cmd.data[2+i*2+0] = folder_tag;
                    endOfSequence = true;
                    resultCount = i;
                    break;
                }
                // do sanity check with unreasonable parameters
                if(size <= 0 || size > 1024) {
                    return error_close(sock, "invalid shadowfolder(tag:%d,size:%d) from %s", folder_tag, size, clientName.c_str());
                }
                char * buff = new char [size];
                ERRCHK(recvBuffer(sock, buff, size, clientName));
                result_cmd.data[2+i*2+0] = folder_tag;
                int status = -1;
                if (!folderMode){
                    status = createPath(0777, args->getlocalShadowRootDir(), std::string(buff, size));  // equivalent of 0700
                    result_cmd.data[2+i*2+1] = status;
                }else {
                    // create LMDB under shadow root
                    MDB_env *env;
                    if (!mdb_env_create(&env) && !mdb_env_set_mapsize(env, LMDB_MAP_SIZE)) {
                        std::string lmdbDir = args->getlocalShadowRootDir() + "/annInferenceLDMB" + std::to_string(folder_tag);
                        status = mdb_env_open(env, lmdbDir.c_str(), 0, 0664);
                    }
                    if (folder_tag < 8)
                        lmdbEnv[folder_tag] = env;      // storing for later use
                    else
                        return error_close(sock, "invalid shadowfolder for LMDB(tag:%d,size:%d) from %s", folder_tag, size, clientName.c_str());
                    result_cmd.data[2+i*2+1] = status;
                }

                int eofMarker = 0;
                ERRCHK(recvBuffer(sock, &eofMarker, sizeof(eofMarker), clientName));
                if(eofMarker != INFCOM_EOF_MARKER) {
                    return error_close(sock, "shadowfolder: eofMarker 0x%08x (incorrect)", eofMarker);
                }
            }
            if(resultCount > 0) {
                result_cmd.data[0] = resultCount;
                ERRCHK(sendCommand(sock, result_cmd, clientName));
                ERRCHK(recvCommand(sock, result_cmd, clientName, INFCOM_CMD_SHADOW_RESULT));
            }
        }
    }
    // sendFiles protocol
    for(bool endOfSequence = false; !endOfSequence; ) {
        InfComCommand cmd = {
            INFCOM_MAGIC, INFCOM_CMD_SHADOW_SEND_FILES, { 6 }, { 0 }        // limit to 6 because max 6 <tag,present> fits into data
        };
        ERRCHK(sendCommand(sock, cmd, clientName));
        ERRCHK(recvCommand(sock, cmd, clientName, INFCOM_CMD_SHADOW_SEND_FILES));
        int fileCountReceived = cmd.data[0];
        int folder_tag        = cmd.data[2];      // required for LMDB
        if (fileCountReceived <= 0) endOfSequence = true;
        int resultCount = fileCountReceived;         // max 6 <tag,present> fits into data
        if(folderMode && (folder_tag < 0 || folder_tag > 7)) {
            return error_close(sock, "invalid shadow folder(tag:%d,) from %s", folder_tag, clientName.c_str());
        }
        InfComCommand result_cmd = {
            INFCOM_MAGIC, INFCOM_CMD_SHADOW_RESULT, { resultCount, INFCOM_CMD_SHADOW_SEND_FILES }, { 0 }
        };
        for(int i=0; i < resultCount; i++) {
            int header[2] = { 0 };
            ERRCHK(recvBuffer(sock, &header, sizeof(header), clientName));
            int tag = header[0];
            int size_filename = (header[1]&0xFFFF0000)>>16;
            int size_data     = header[1]&0xFFFF;
            int size          = size_filename + size_data;
            result_cmd.data[2+i*2+0] = tag;
            result_cmd.data[2+i*2+1] = -1;           // not present
            // do sanity check with unreasonable parameters
            if(tag < 0 || size <= 0 || size > 50000000) {
                return error_close(sock, "invalid filename(tag:%d,size:%d) from %s", tag, size, clientName.c_str());
            }

            // get filename from folder_tag and file_tag
            std::string fileName = args->getlocalShadowRootDir() + "/";
            if (!folderMode){
                struct stat sb;
                char * buff = new char [size];
                ERRCHK(recvBuffer(sock, buff, size, clientName));
                if(size_filename) fileName.append(std::string(buff, size_filename));
                result_cmd.data[2+i*2+1] = 0;       // new file created
                if (stat(fileName.c_str(), &sb) == 0 && S_ISREG(sb.st_mode))
                {
                    result_cmd.data[2+i*2+1] = 1;       // file already present
                }else
                {
                    // open file and write data
                    FILE *fp = fopen(fileName.c_str(), "wb");
                    if(!fp) {
                        return error_close(sock, "shadowmode: couldn't open file %s errno:%d", fileName.c_str(), errno);
                    }
                    fwrite(&buff[size_filename], 1, size_data, fp);
                    fclose(fp);
                }
            } else {
                char * buff = new char [size];
                ERRCHK(recvBuffer(sock, buff, size, clientName));
                 //open LMDB and write into records
                 MDB_dbi dbi;
                 MDB_txn *txn;
                 if (!mdb_txn_begin(lmdbEnv[folder_tag], NULL, 0, &txn) && !mdb_open(txn, NULL, 0, &dbi)){

                     MDB_val key, data;
                     key.mv_size = sizeof(int);
                     key.mv_data = &tag;
                     data.mv_size = size_data;
                     data.mv_data = &buff[size_filename];
                     // todo: resize and decode and copy decoded bitmap to LMDB
                     int status = mdb_put(txn, dbi, &key, &data, MDB_NOOVERWRITE);
                     status |= mdb_txn_commit(txn);
                     if(!status) {
                         return error_close(sock, "shadowmode: couldn't write to lmdb:%d", fileName.c_str(), status);
                     }
                     mdb_close(lmdbEnv[folder_tag], dbi);
                 }
            }

            int eofMarker = 0;
            ERRCHK(recvBuffer(sock, &eofMarker, sizeof(eofMarker), clientName));
            if(eofMarker != INFCOM_EOF_MARKER) {
                return error_close(sock, "shadowfolder: eofMarker 0x%08x (incorrect)", eofMarker);
            }
        }
        if(resultCount > 0) {

            result_cmd.data[0] = resultCount;
            ERRCHK(sendCommand(sock, result_cmd, clientName));
            ERRCHK(recvCommand(sock, result_cmd, clientName, INFCOM_CMD_SHADOW_RESULT));
        }
    }
    // send and wait for INFCOM_CMD_DONE message
    InfComCommand reply = {
        INFCOM_MAGIC, INFCOM_CMD_DONE, { 0 }, { 0 }
    };
    ERRCHK(sendCommand(sock, reply, clientName));
    ERRCHK(recvCommand(sock, reply, clientName, INFCOM_CMD_DONE));
    // close LMDB environments if any
    for (int i=0; i<8; i++)
        if (lmdbEnv[i])	mdb_env_close(lmdbEnv[i]);

    return 0;
}

