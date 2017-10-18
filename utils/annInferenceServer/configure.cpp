#include "configure.h"
#include "netutil.h"
#include "common.h"

int runConfigure(int sock, Arguments * args, std::string& clientName, InfComCommand * cmd)
{
    //////
    /// \brief send the configuration info
    ///

    // send: INFCOM_CMD_CONFIG_INFO { modelCount, numGpus }
    int modelCount = args->getNumConfigureddModels();
    InfComCommand config_info = {
        INFCOM_MAGIC, INFCOM_CMD_CONFIG_INFO,
        { modelCount, args->getNumGPUs() },
        { 0 }
    };
    ERRCHK(sendCommand(sock, config_info, clientName));
    for(size_t i = 0; i < modelCount; i++) {
        // send: INFCOM_CMD_MODEL_INFO { iw, ih, ic, ow, oh, oc } "modelName"
        std::tuple<std::string,int,int,int,int,int,int> info = args->getConfiguredModelInfo(i);
        InfComCommand model_info = {
            INFCOM_MAGIC, INFCOM_CMD_MODEL_INFO,
            { std::get<1>(info), std::get<2>(info), std::get<3>(info),
              std::get<4>(info), std::get<5>(info), std::get<6>(info) },
            { 0 }
        };
        strncpy(model_info.message, std::get<0>(info).c_str(), sizeof(model_info.message));
        ERRCHK(sendCommand(sock, model_info, clientName));
    }

    // wait for INFCOM_CMD_DONE message
    InfComCommand reply;
    ERRCHK(recvCommand(sock, reply, clientName, INFCOM_CMD_DONE));

    return 0;
}
