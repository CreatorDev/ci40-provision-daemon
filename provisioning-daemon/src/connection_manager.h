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

#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#include "clicker.h"
#include "commands.h"
#include "event.h"
#include <stdint.h>
#include <stdbool.h>
#include <semaphore.h>
#include <glib.h>

#define TCP_PORT                                (49300)
#define MAX_CLIENTS                             (30)
#define KEEP_ALIVE_INTERVAL_MS                  (2000)
#define KEEP_ALIVE_TIMEOUT_MS                   (30000)
#define CHECK_CONNECTIONS_INTERVAL_MS           (2000)

typedef struct {
    int             clickerID;  /**< Clicker to which data should be send */
    NetworkCommand  command;
    gpointer        data;       /**< if not NULL then this pointer will be released (g_free) after send! */
    uint16_t        dataSize;
} NetworkDataPack;

/**
 * @brief Initiates socket, binds to it and start listening for incoming connections
 * @param[in] tcpPort Port on which incoming connections will be expected
 */
int con_BindAndListen(int tcpPort);

/**
 *  @brief Accepts incoming connections and handles read from socket. Should be called periodically.
 */
void con_ProcessConnections(void);

/**
 * @brief Disconnect specified clicker
 * @param[in] clickerID to disconnect
 */
void con_Disconnect(int clickerID);

/**
 * @brief check if given event is clicker module relevant. If yes then proper handling is executed.
 * @param[in] event Event to be consumed.
 * @return true if event was handled, otherwise false
 */
bool con_ConsumeEvent(Event* event);

/**
 * @brief Allocate and returns NetworkDataPack struct filled with data passed as arguments.
 * NOTE: If copyData is false then this method will take ownership of data argument.
 * @param[in] clickerID identifier of clicker to which data should be send
 * @param[in] cmd Command to send
 * @param[in] data Pointer to data passed to remote clicker, might be NULL
 * @param[in] dataLen Size of data to send, might be 0
 * @return pointer to newly created and filled NetworkDataPack
 */
NetworkDataPack* con_BuildNetworkDataPack(int clickerID, NetworkCommand cmd, uint8_t* data, uint16_t dataLen,
        bool copyData);

/**
 * @brief Returns IP address of clicker with given id. You don't own this pointer, copy data if needed but DO NOT
 * cache it.
 * @return IP of clicker or NULL if not found
 */
char* con_GetIPForClicker(int clickerId);
#endif
