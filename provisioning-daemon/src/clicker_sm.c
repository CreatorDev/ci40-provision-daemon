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

#include "clicker_sm.h"
#include "log.h"

#include <unistd.h>

queue_Task * clicker_sm_GetNextTask(Clicker *clicker)
{
    if (clicker == NULL)
    {
        LOG(LOG_WARN, "clicker_sm_GetNextTask: Passed clicker is NULL");
        return NULL;
    }
    if (clicker->taskInProgress)
        return NULL;

    if (clicker->localKey == NULL)
    {
        return queue_NewQueueTask
        (
            queue_TaskType_GENERATE_ALICE_KEY,
            clicker->clickerID,
            NULL,
            0
        );
    }

    if (clicker->localKey != NULL && clicker->remoteKey != NULL && clicker->sharedKey == NULL && clicker->provisioningInProgress)
    {
        return queue_NewQueueTask
        (
            queue_TaskType_GENERATE_SHARED_KEY,
            clicker->clickerID,
            NULL,
            0
        );
    }

    if (clicker->psk == NULL && clicker->provisioningInProgress)
    {
        return queue_NewQueueTask
        (
            queue_TaskType_GENERATE_PSK,
            clicker->clickerID,
            NULL,
            0
        );
    }

    return NULL;
}
