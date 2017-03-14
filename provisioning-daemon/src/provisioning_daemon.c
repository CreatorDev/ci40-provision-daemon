/***************************************************************************************************
 * Copyright (c) 2016, Imagination Technologies Limited and/or its affiliated group companies
 * and/or licensors
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions
 *    and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/***************************************************************************************************
 * Includes
 **************************************************************************************************/

#include <bits/alltypes.h>
#include <bits/signal.h>
#include <libconfig.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "clicker.h"
#include "clicker_sm.h"
#include "commands.h"
#include "connection_manager.h"
#include "crypto/bigint.h"
#include "crypto/crypto_config.h"
#include "crypto/diffie_hellman_keys_exchanger.h"
#include "crypto/encoder.h"
#include "errors.h"
#include "led.h"
#include "log.h"
#include "processing_queue.h"
#include "provision_history.h"
#include "ubus_agent.h"
#include "utils.h"
#include "buttons.h"
#include "event.h"

/***************************************************************************************************
 * Definitions
 **************************************************************************************************/

/** Calculate size of array. */
#define ARRAY_SIZE(x) ((sizeof x) / (sizeof *x))

#define LED_BLINK_INTERVAL_MS                     (500)
#define P_LED_BLINK_INTERVAL_MS                 (100)
#define HIGHLIGHT_INTERVAL_MS                    (500)
#define DEFAULT_PATH_CONFIG_FILE                "/etc/config/provisioning_daemon"


#define CONFIG_BOOTSTRAP_URI                    "coaps://deviceserver.creatordev.io:15684"
#define CONFIG_DEFUALT_TCP_PORT                 (49300)
#define CONFIG_DEFAULT_ENDPOINT_PATTERN         "cd_{t}_{i}"
#define CONFIG_DEFAULT_LOCAL_PROV_CTRL          (true)
#define CONFIG_DEFAULT_REMOTE_PROV_CTRL         (false)
//! @cond Doxygen_Suppress

/***************************************************************************************************
 * Globals
 **************************************************************************************************/



/**
 * Describes states provisioning daemon can be in.
 */
typedef enum {
    pd_Mode_LISTENING, // no provisioning being performed, in this state user can choose clicker to provision
    pd_Mode_PROVISIONING, // provisioning has been started
    pd_Mode_ERROR
} pd_Mode;

typedef struct {
    int tcpPort;
    const char *defaultRouteUri;
    const char *bootstrapUri;
    const char *dnsServer;
    const char *endPointNamePattern;
    int logLevel;
    int localProvisionControl;
    int remoteProvisionControl;
} pd_Config;


/**
 * Currently selected clicker. NULL when there is no clicker selected.
 */
static Clicker *_SelectedClicker = NULL;

/**
 * Flag passed to main loop telling that selected clicker has changed so appropriate action can be taken.
 */
static int _SelectedClickerChanged = 0;

static bool _ModeChanged = false;

/**
 * Main loop condition.
 */
static short int _KeepRunning = 1;

/**
 * Time in millis of last led state has been changed.
 */
static unsigned long _LastBlinkTime = 0;

static int g_activeLedOn = 1;

/**
 * Current app mode telling in which step of provisioning app currently  is.
 */
static pd_Mode _Mode = pd_Mode_LISTENING;

unsigned long _ModeErrorTime = 0;

FILE * g_debugStream = NULL;
int g_debugLevel = LOG_INFO;


static pd_DeviceServerConfig _DeviceServerConfig;
static pd_NetworkConfig _NetworkConfig;

static config_t _Cfg;

pd_Config _PDConfig = {
    .tcpPort = 0,
    .defaultRouteUri = NULL,
    .bootstrapUri = NULL,
    .dnsServer = NULL,
    .endPointNamePattern = NULL,
    .logLevel = 0,
    .localProvisionControl = false,
    .remoteProvisionControl = false
};

static sem_t semaphore;
sem_t debugSemapthore;
/***************************************************************************************************
 * Implementation
 **************************************************************************************************/


/**
 * @brief Handles Ctrl+C signal. Helps exit app gracefully.
 */
static void CtrlCHandler(int signal)
{
    LOG(LOG_INFO, "Exit triggered...");
    _KeepRunning = 0;
}

static void HandleKeyCommand(Clicker *clicker, uint8_t *data)
{
    uint8_t dataLength = data[1];
    if (clicker->remoteKey != NULL)
        FREE_AND_NULL(clicker->remoteKey);
    clicker->remoteKey = malloc(dataLength);
    memcpy(clicker->remoteKey, &data[2], dataLength);
    LOG(LOG_INFO, "Received exchange key from clicker : %d", clicker->clickerID);
    PRINT_BYTES(clicker->remoteKey, P_MODULE_LENGTH);

}

/**
 * @brief Handles variuos commands received from clickers
 */
static void CommandHandler(Clicker *clicker, uint8_t *buffer)
{
    switch (buffer[0]) {
        case NetworkCommand_KEY:
            LOG(LOG_DBG, "Received KEY command");
            HandleKeyCommand(clicker, buffer);
            break;
        default:
            LOG(LOG_WARN, "Unknown command received : %d", buffer[0]);
            break;
    }
}

static void ClickerDisconnectionHandler(Clicker *clicker)
{
    if (_SelectedClicker == clicker)
        _SelectedClicker = NULL;
}

static void ClickerConnectionHandler(Clicker *clicker, char *ip)
{
    char hash[10];
    GenerateClickerTimeHash(hash);
    char ipFragment[5];
    memset(ipFragment, 0, 5);
    strncpy(ipFragment, ip + strlen(ip) - 4, 4);
    GenerateClickerName(clicker->name, COMMAND_ENDPOINT_NAME_LENGTH, (char*)_PDConfig.endPointNamePattern, hash, ipFragment);
    LOG(LOG_INFO, "New clicker connected, ip : %s, id : %d, name : %s", ip, clicker->clickerID, clicker->name);
}

/**
 * @bried Set the leds according to current app state.
 */
static void UpdateLeds(void)
{
    int interval = (_Mode == pd_Mode_LISTENING || _Mode == pd_Mode_ERROR) ? LED_BLINK_INTERVAL_MS : P_LED_BLINK_INTERVAL_MS;
    unsigned long currentTime = GetCurrentTimeMillis();
    if (currentTime - _LastBlinkTime > interval)
    {
        _LastBlinkTime = currentTime;
        if (g_activeLedOn)
            g_activeLedOn = 0;
        else
            g_activeLedOn = 1;
    }

    if (_Mode == pd_Mode_ERROR)
    {
        if (g_activeLedOn)
            SetAllLeds(true);
        else
            SetAllLeds(false);
    }
    else
    {
        SetLeds(con_GetConnectionsCount(), clicker_GetIndexOfClicker(_SelectedClicker), g_activeLedOn);
    }
}

/**
 * Validates whether currently selected clicker is still valid.
 */
static void UpdateSelectedClicker(void)
{
    int connectedClickersCount = con_GetConnectionsCount();

    // if there are no connected clickers reset selected clicker
    if (connectedClickersCount == 0)
    {
        _SelectedClicker = NULL;
        return;
    }
    // if there are connected clickers but we have not selected any yet
    // select first clicker
    if (_SelectedClicker == NULL)
    {
        _SelectedClicker = clicker_GetClickerAtIndex(0);
        _SelectedClickerChanged = 1;
        return;
    }
    if (clicker_GetIndexOfClicker(_SelectedClicker) > connectedClickersCount -1)
    {
        if (_SelectedClicker->next != NULL)
            _SelectedClicker = _SelectedClicker->next;
        else
            _SelectedClicker = clicker_GetClickerAtIndex(0);
    }
}



/**
 * @brief Tries to change selected clicker to next one if possible.
 * @return 1 if selected clicker has been successfully changed, 0 otherwise
 */
static int TryChangeSelectedClicker(void)
{
    sem_wait(&semaphore);
    LOG(LOG_DBG, "Try change selected clicker");
    int connectedClickersCount = con_GetConnectionsCount();
    if (connectedClickersCount == 0)
    {
        LOG(LOG_DBG, "End try change selected clicker 1");
        sem_post(&semaphore);
        return 0;
    }
    if (_SelectedClicker->next == NULL)
    {
        _SelectedClicker = clicker_GetClickerAtIndex(0);
        _SelectedClickerChanged = 1;
        LOG(LOG_DBG, "End try change selected clicker 2");
        sem_post(&semaphore);
        return 1;
    }
    _SelectedClicker = _SelectedClicker->next;
    _SelectedClickerChanged = 1;
    LOG(LOG_DBG, "End try change selected clicker 3");
    sem_post(&semaphore);
    return 1;
}

//to remove
//static void Switch1PressedCallback(void)
//{
//    if (_Mode == pd_Mode_LISTENING)
//        _Switch1Pressed = 1;
//}
//
//static void Switch2PressedCallback(void)
//{
//    _Switch2Pressed = 1;
//}

static void HandleButton1Press(void)
{
//to remove
//    _Switch1Pressed = 0;
    TryChangeSelectedClicker();
}

static void HandleButton2Press(void)
{
//to remove
//    _Switch2Pressed = 0;
    if (_Mode == pd_Mode_LISTENING)
    {
        if (_SelectedClicker != NULL)
        {
            _Mode = pd_Mode_PROVISIONING;
            _ModeChanged = true;
            _SelectedClicker->provisioningInProgress = true;
            history_RemoveProvisioned(_SelectedClicker->clickerID);
        }
    }
    else
    {
        _Mode = pd_Mode_LISTENING;
        _ModeChanged = true;
    }
}

bool pd_StartProvision(void)
{
    int tmp = _Mode;
    HandleButton2Press();
    return (_Mode == pd_Mode_LISTENING) && (_Mode != tmp);
}

static void HandleModeChanged(void)
{
    _ModeChanged = false;
    if (_Mode == pd_Mode_LISTENING)
        LOG(LOG_INFO, "Switched to LISTENING mode");
    else if (_Mode == pd_Mode_PROVISIONING)
        LOG(LOG_INFO, "Started provisioning of clicker with id : %d", _SelectedClicker->clickerID);
    else
        LOG(LOG_INFO, "Switched to ERROR mode");
}

int pd_GetSelectedClickerId(void)
{
    int result = -1;
    sem_wait(&semaphore);
    if (_SelectedClicker != NULL)
        result = _SelectedClicker->clickerID;

    sem_post(&semaphore);
    return result;
}

int pd_SetSelectedClicker(int id)
{
    sem_wait(&semaphore);

    int result = (_SelectedClicker != NULL) ? _SelectedClicker->clickerID : -1;
    Clicker* tmp = clicker_GetClickerByID(id);
    if (tmp != NULL)
    {
        _SelectedClicker = tmp;
        result = tmp->clickerID;
        _SelectedClickerChanged = 1;
    }
    sem_post(&semaphore);
    return result;
}

void TryToSendPsk(Clicker *clicker)
{
    if (clicker->sharedKey != NULL && clicker->psk != NULL)
    {
        memset(&_DeviceServerConfig, 0, sizeof(_DeviceServerConfig));
        _DeviceServerConfig.securityMode = 0;

        memcpy(_DeviceServerConfig.psk, clicker->psk, clicker->pskLen);
        _DeviceServerConfig.pskKeySize = clicker->pskLen;

        memcpy(_DeviceServerConfig.identity, clicker->identity, clicker->identityLen);
        _DeviceServerConfig.identitySize = clicker->identityLen;

        memcpy(_DeviceServerConfig.bootstrapUri, _PDConfig.bootstrapUri, strnlen(_PDConfig.bootstrapUri, 200));

        uint8_t dataLen = 0;
        uint8_t *encodedData = softap_encodeBytes((uint8_t *)&_DeviceServerConfig, sizeof(_DeviceServerConfig) ,
                clicker->sharedKey, &dataLen);
        NetworkDataPack* netData = con_BuildNetworkDataPack(clicker->clickerID, NetworkCommand_DEVICE_SERVER_CONFIG,
                encodedData, dataLen);
        event_PushEventWithPtr(EventType_CONNECTION_SEND_COMMAND, netData, true);
        LOG(LOG_INFO, "Sending Device Server Config to clicker with id : %d", clicker->clickerID);

        memset(&_NetworkConfig, 0, sizeof(_NetworkConfig));
        memcpy(&_NetworkConfig.defaultRouteUri, _PDConfig.defaultRouteUri, strnlen(_PDConfig.defaultRouteUri, 100));
        memcpy(&_NetworkConfig.dnsServer, _PDConfig.dnsServer, strnlen(_PDConfig.dnsServer, 100));
        memcpy(&_NetworkConfig.endpointName, clicker->name, strnlen(clicker->name, 24));

        dataLen = 0;
        encodedData = softap_encodeBytes((uint8_t *)&_NetworkConfig, sizeof(_NetworkConfig) , clicker->sharedKey, &dataLen);
        netData = con_BuildNetworkDataPack(clicker->clickerID, NetworkCommand_NETWORK_CONFIG, encodedData, dataLen);
        event_PushEventWithPtr(EventType_CONNECTION_SEND_COMMAND, netData, true);
        FREE_AND_NULL(encodedData);

        LOG(LOG_INFO, "Sent Network Config to clicker with id : %d", clicker->clickerID);
        LOG(LOG_INFO, "Provisioning of clicker with id : %d finished, going back to LISTENING mode", clicker->clickerID);
        clicker->provisionTime = GetCurrentTimeMillis();
        clicker->provisioningInProgress = false;
    }
}

static bool ReadConfigFile(const char *filePath)
{
    config_init(&_Cfg);
    if(! config_read_file(&_Cfg, filePath))
    {
        LOG(LOG_ERR, "Failed to open config file at path : %s", filePath);
        return false;
    }

    if (_PDConfig.bootstrapUri == NULL)
    {
        if(!config_lookup_string(&_Cfg, "BOOTSTRAP_URI", &_PDConfig.bootstrapUri))
        {
            LOG(LOG_ERR, "Config file does not contain BOOTSTRAP_URI property, using default:%s", CONFIG_BOOTSTRAP_URI);
            _PDConfig.bootstrapUri = CONFIG_BOOTSTRAP_URI;
        }
    }

    if (_PDConfig.defaultRouteUri == NULL)
    {
        if(!config_lookup_string(&_Cfg, "DEFAULT_ROUTE_URI", &_PDConfig.defaultRouteUri))
        {
            LOG(LOG_ERR, "Config file does not contain DEFAULT_ROUTE_URI property");
            return false;
        }
    }

    if (_PDConfig.dnsServer == NULL)
    {
        if(!config_lookup_string(&_Cfg, "DNS_SERVER", &_PDConfig.dnsServer))
        {
            LOG(LOG_ERR, "Config file does not contain DNS_SERVER property.");
            return false;
        }
    }

    if (_PDConfig.endPointNamePattern == NULL)
    {
        if(!config_lookup_string(&_Cfg, "ENDPOINT_NAME_PATTERN", &_PDConfig.endPointNamePattern))
        {
            LOG(LOG_ERR, "Config file does not contain ENDPOINT_NAME_PATTERN property, using default:%s.",
                    CONFIG_DEFAULT_ENDPOINT_PATTERN);
            _PDConfig.endPointNamePattern = CONFIG_DEFAULT_ENDPOINT_PATTERN;
        }
    }

    if (_PDConfig.logLevel == 0)
    {
        if(!config_lookup_int(&_Cfg, "LOG_LEVEL", &_PDConfig.logLevel))
        {
            LOG(LOG_ERR, "Config file does not contain LOG_LEVEL property. Using default value: Warning");
            _PDConfig.logLevel = LOG_WARN;
        }
    }

    if (_PDConfig.tcpPort == 0)
    {
        if(!config_lookup_int(&_Cfg, "PORT", &_PDConfig.tcpPort))
        {
            LOG(LOG_ERR, "Config file does not contain PORT property, using default: %d", CONFIG_DEFUALT_TCP_PORT);
            _PDConfig.tcpPort = CONFIG_DEFUALT_TCP_PORT;
            return false;
        }
    }

    if (_PDConfig.localProvisionControl == false)
    {
        if(!config_lookup_bool(&_Cfg, "LOCAL_PROVISION_CTRL", &_PDConfig.localProvisionControl))
        {
            LOG(LOG_ERR, "Config file does not contain LOCAL_PROVISION_CTRL property, using default: %d",
                    CONFIG_DEFAULT_LOCAL_PROV_CTRL);
            _PDConfig.localProvisionControl = CONFIG_DEFAULT_LOCAL_PROV_CTRL;
        }
    }

    if (_PDConfig.remoteProvisionControl == false)
    {
        if(!config_lookup_bool(&_Cfg, "REMOTE_PROVISION_CTRL", &_PDConfig.remoteProvisionControl))
        {
            LOG(LOG_ERR, "Config file does not contain REMOTE_PROVISION_CTRL property, using default: %d",
                    CONFIG_DEFAULT_REMOTE_PROV_CTRL);
            _PDConfig.remoteProvisionControl = CONFIG_DEFAULT_REMOTE_PROV_CTRL;
        }
    }

    return true;
}

static void Daemonise(void)
{
    pid_t pid;

    // fork off the parent process
    pid = fork();

    if (pid < 0)
    {
        LOG(LOG_ERR, "Failed to start daemon\n");
        exit(-1);
    }

    if (pid > 0)
    {
        LOG(LOG_DBG, "Daemon running as %d\n", pid);
        exit(0);
    }

    umask(0);

    // close off standard file descriptors
    close (STDIN_FILENO);
    close (STDOUT_FILENO);
    close (STDERR_FILENO);
}

static int ParseCommandArgs(int argc, char *argv[], const char **fptr)
{
    int opt, tmp;
    opterr = 0;
    char *configFilePath = NULL;

    while (1)
    {
        opt = getopt(argc, argv, "v:c:l:dr");
        if (opt == -1)
            break;

        switch (opt)
        {
            case 'v':
                tmp = (unsigned int)strtoul(optarg, NULL, 0);
                if (tmp >= LOG_FATAL && tmp <= LOG_DBG)
                {
                    g_debugLevel = tmp;
                }
                else
                {
                    LOG(LOG_ERR, "Invalid debug level");
                    return -1;
                }
                break;

            case 'c':
                configFilePath = malloc(strlen(optarg));
                sprintf(configFilePath, "%s", optarg);
                break;

            case 'l':
                *fptr = optarg;
                break;

            case 'd':
                Daemonise();
                break;

            case 'r':
                _PDConfig.remoteProvisionControl = true;
                break;

            default:
                return -1;
        }
    }

    if (configFilePath == NULL)
    {
        configFilePath = malloc(strlen(DEFAULT_PATH_CONFIG_FILE));
        sprintf(configFilePath, "%s", DEFAULT_PATH_CONFIG_FILE);
    }

    if (ReadConfigFile(configFilePath) == false)
        return -1;

    if (configFilePath != NULL)
        free(configFilePath);

    return 1;
}

void CleanupOnExit(void)
{
    ubusagent_Close();
    bi_releaseConst();
    queue_Stop();
    if (_PDConfig.localProvisionControl)
    {
        buttons_Shutdown();
    }

    config_destroy(&_Cfg);
    sem_destroy(&semaphore);
    sem_destroy(&debugSemapthore);
    history_destroy();
}

int main(int argc, char **argv)
{
    sem_init(&debugSemapthore, 0, 1);
    int ret;
    const char *fptr = NULL;
    FILE *logFile;
    if ((ret = ParseCommandArgs(argc, argv, &fptr)) < 0)
    {
        LOG(LOG_ERR, "Invalid command args");
        return -1;
    }

    if (fptr)
    {
        logFile = fopen(fptr, "w");

        if (logFile != NULL)
            g_debugStream  = logFile;
        else
            LOG(LOG_ERR, "Failed to create or open %s file", fptr);
    }

    event_Init();

    g_debugLevel = _PDConfig.logLevel;

    bi_generateConst();

    signal(SIGINT, CtrlCHandler);
    sem_init(&semaphore, 0, 1);
    history_init();

    if (_PDConfig.localProvisionControl)
    {
        buttons_Init();
    }

    clicker_Init();

    if (ubusagent_Init() == false)
    {
        LOG(LOG_ERR, "Unable to register to uBus!");
        CleanupOnExit();
        return -1;
    }
    if (_PDConfig.remoteProvisionControl)
    {
        LOG(LOG_INFO, "Enabling provision control through uBus.");
        if (ubusagent_EnableRemoteControl() == false)
            LOG(LOG_ERR, "Problems with uBus, remote control is disabled!");
    }
    con_BindAndListen(_PDConfig.tcpPort, CommandHandler, ClickerConnectionHandler, ClickerDisconnectionHandler);
    queue_Start();
    LOG(LOG_INFO, "Entering main loop");
    while(_KeepRunning)
    {
        long long int loopStartTime = GetCurrentTimeMillis();

        con_ProcessConnections();

        UpdateSelectedClicker();

        //---- EVENT LOOP ----
        Event* event = event_PopEvent();

        // Handle buttons press
        if (_PDConfig.localProvisionControl && event != NULL && event->type == EventType_BUTTON_PRESSED)
        {
            if (event->intData == BUTTON_1_ID)
                HandleButton1Press();

            if (event->intData == BUTTON_2_ID)
                HandleButton2Press();
        }
        //order of consumers DO MATTER !
        con_ConsumeEvent(event);
        clicker_ConsumeEvent(event);

        if (event != NULL) {
            event_releaseEvent(&event);
        }
        //-----------------

        if (_ModeChanged)
            HandleModeChanged();

        UpdateLeds();

        if (_SelectedClickerChanged)
        {
            LOG(LOG_INFO, "Selected Clicker ID : %d", _SelectedClicker->clickerID);

            // Send ENABLE_HIGHLIGHT command to active clicker and DISABLE_HIGHLIGHT to inactive clickers
            NetworkDataPack* netData = con_BuildNetworkDataPack(_SelectedClicker->clickerID,
                    NetworkCommand_ENABLE_HIGHLIGHT, NULL, 0);
            event_PushEventWithPtr(EventType_CONNECTION_SEND_COMMAND, netData, true);

            Clicker * clicker = clicker_GetClickers();
            while (clicker != NULL)
            {
                if (clicker != _SelectedClicker)
                {
                    netData = con_BuildNetworkDataPack(clicker->clickerID,
                            NetworkCommand_DISABLE_HIGHLIGHT, NULL, 0);
                    event_PushEventWithPtr(EventType_CONNECTION_SEND_COMMAND, netData, true);
                }

                clicker = clicker->next;
            }
            _SelectedClickerChanged = 0;
            _Mode = pd_Mode_LISTENING;
        }

        Clicker *clk = clicker_GetClickers();
        while (clk != NULL)
        {
            queue_Task *nextTask = clicker_sm_GetNextTask(clk);
            if (nextTask != NULL)
            {
                queue_AddTask(nextTask);
                clk->taskInProgress = true;
            }
            clk = clk->next;

        }

        queue_Task *lastResult = queue_PopResult();

        if (lastResult != NULL)
        {
            Clicker *clicker = clicker_GetClickerByID(lastResult->clickerID);

            if (clicker != NULL)
            {
                switch (lastResult->type)
                {
                    case queue_TaskType_GENERATE_ALICE_KEY:
                        clicker->localKey = lastResult->outData;
                        LOG(LOG_INFO, "Generated local Key");
                        PRINT_BYTES(clicker->localKey, P_MODULE_LENGTH);
                        LOG(LOG_INFO, "Sending local Key to clicker with id : %d", clicker->clickerID);
                        NetworkDataPack* netData = con_BuildNetworkDataPack(clicker->clickerID,
                                NetworkCommand_KEY, clicker->localKey, lastResult->outDataLength);
                        event_PushEventWithPtr(EventType_CONNECTION_SEND_COMMAND, netData, true);
                        break;
                    case queue_TaskType_GENERATE_SHARED_KEY:
                        LOG(LOG_INFO, "Generated Shared Key");
                        clicker->sharedKey = lastResult->outData;
                        PRINT_BYTES(clicker->sharedKey, P_MODULE_LENGTH);
                        TryToSendPsk(clicker);
                        break;
                    case queue_TaskType_GENERATE_PSK:
                        if (lastResult->outData <= 0)
                        {
                            LOG(LOG_WARN, "Couldn't get PSK from Device Server");
                            clicker->error = pd_Error_GENERATE_PSK;
                            _Mode = pd_Mode_ERROR;
                            clicker->provisioningInProgress = false;
                            break;
                        }
                        LOG(LOG_INFO, "Received PSK from Device Server: %s, dataLen:%d", (char*)lastResult->outData,
                                lastResult->outDataLength);

                        {
                          queue_pskIdentityPair* pair = (queue_pskIdentityPair*)lastResult->outData;
                          clicker->psk = malloc(pair->pskLen/2);
                          HexStringToByteArray(pair->psk, clicker->psk, pair->pskLen/2);
                          clicker->pskLen = pair->pskLen/2;
                          clicker->identity = malloc(pair->identityLen + 1);
                          strncpy(clicker->identity, pair->identity, pair->identityLen);
                          clicker->identityLen = pair->identityLen;
                        }

                        TryToSendPsk(clicker);
                        FREE_AND_NULL(lastResult->outData);
                        history_AddAsProvisioned(clicker->clickerID, clicker->name);
                        break;
                    default:
                        break;
                }
                clicker->taskInProgress = false;
            }
            FREE_AND_NULL(lastResult);
        }

        // disconnect clickers with errors
        // Clicker *clicker = clicker_GetClickers();
        // while (clicker != NULL)
        // {
        //     if (clicker->error > 0)
        //     {
        //         _Mode = pd_Mode_ERROR;
        //         _ModeErrorTime = GetCurrentTimeMillis();
        //         LOG(LOG_INFO, "Disconnecting clicker with id : %d due to error : %d", clicker->clickerID, clicker->error);
        //         con_Disconnect(clicker);
        //         _ModeChanged = 1;
        //     }
        //     clicker = clicker->next;
        // }


        if (_Mode == pd_Mode_ERROR && loopStartTime - _ModeErrorTime > 5000)
        {
            _Mode = pd_Mode_LISTENING;
            _ModeChanged = 1;
        }

        // disconnect already provisioned clicker after 3s timeout, timeout is important as if we disconnect clicker to early it may try to reconnect
        if (_SelectedClicker != NULL && _SelectedClicker->provisionTime > 0)
        {
            if (loopStartTime - _SelectedClicker->provisionTime > 3000)
                con_Disconnect(_SelectedClicker);
        }

        long long int loopEndTime = GetCurrentTimeMillis();
        if (loopEndTime - loopStartTime < 50)
            usleep(1000*(50-(loopEndTime-loopStartTime)));
    }

    CleanupOnExit();

    return 0;
}
