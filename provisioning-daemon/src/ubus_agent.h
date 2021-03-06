/************************************************************************************************************************
 Copyright (c) 2016, Imagination Technologies Limited and/or its affiliated group companies.
 All rights reserved.
 Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
 following conditions are met:
     1. Redistributions of source code must retain the above copyright notice, this list of conditions and the
        following disclaimer.
     2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
        following disclaimer in the documentation and/or other materials provided with the distribution.
     3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote
        products derived from this software without specific prior written permission.
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
************************************************************************************************************************/

#ifndef UBUS_AGENT_H_
#define UBUS_AGENT_H_

#include <stdint.h>
#include <stdbool.h>

#define PSK_ARRAYS_SIZE 255

typedef struct  {
    int clickerId;

    char psk[PSK_ARRAYS_SIZE];
    uint8_t pskLen;

    char identity[PSK_ARRAYS_SIZE];
    uint8_t identityLen;
} PreSharedKey;

/**
 * @brief Init ubus mechanism, register objects
 */
bool ubusagent_Init(void);
void ubusagent_Destroy(void);

bool ubusagent_EnableRemoteControl(void);

/**
 * @brief Send "generatePsk" call to "imgtec" object through ubus.
 * This call is blocking, and may take up to 10 seconds to return.
 *
 * @param[in] clickerId which asks for psk, it will be used with event: EventType_PSK_OBTAINED
 * @return true if request has been successfully performed, false otherwise. This
 * only means that request went away successfully there still may error return in EventType_PSK_OBTAINED event.
 */
bool ubusagent_SendGeneratePskMessage(int clickerId);

#endif /* UBUS_AGENT_H_ */
