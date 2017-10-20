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
    info("number of pre-configured models: %d", modelCount);
    for(size_t i = 0; i < modelCount; i++) {
        // send: INFCOM_CMD_MODEL_INFO { iw, ih, ic, ow, oh, oc } "modelName"
        std::tuple<std::string,int,int,int,int,int,int,std::string> model_config = args->getConfiguredModelInfo(i);
        InfComCommand model_info = {
            INFCOM_MAGIC, INFCOM_CMD_MODEL_INFO,
            { std::get<1>(model_config), std::get<2>(model_config), std::get<3>(model_config),
              std::get<4>(model_config), std::get<5>(model_config), std::get<6>(model_config) },
            { 0 }
        };
        strncpy(model_info.message, std::get<0>(model_config).c_str(), sizeof(model_info.message));
        ERRCHK(sendCommand(sock, model_info, clientName));
        info("pre-configured model#%d: %s [input %dx%dx%d] [output %dx%dx%d]", i, model_info.message,
             model_info.data[2], model_info.data[1], model_info.data[0],
             model_info.data[5], model_info.data[4], model_info.data[4]);
    }

    // wait for INFCOM_CMD_DONE message
    InfComCommand reply;
    ERRCHK(recvCommand(sock, reply, clientName, INFCOM_CMD_DONE));

    return 0;
}
