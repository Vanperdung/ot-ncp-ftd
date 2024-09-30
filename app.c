/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
#include <assert.h>
#include <openthread-core-config.h>
#include <openthread/config.h>

#include <openthread/ncp.h>
#include <openthread/diag.h>
#include <openthread/tasklet.h>

#include <openthread/dataset_ftd.h>
#include <openthread/instance.h>
#include <openthread/message.h>
#include <openthread/thread.h>
#include <openthread/udp.h>
#include <openthread/platform/logging.h>
#include <common/code_utils.hpp>
#include <common/logging.hpp>

#include "openthread-system.h"
#include "app.h"

#include "reset_util.h"
#include "sl_simple_led.h"
#include "sl_simple_led_instances.h"
#include <string.h>

/**
 * This function initializes the NCP app.
 *
 * @param[in]  aInstance  The OpenThread instance structure.
 *
 */
extern void otAppNcpInit(otInstance *aInstance);

otInstance* gInstance = NULL;
otUdpSocket sUdpSocket;
bool isCreateUDP = false;
bool isSleep = false;

#ifndef LED_INSTANCE
#define LED_INSTANCE    sl_led_pwr
#endif

otInstance *otGetInstance(void)
{
    return gInstance;
}

void sl_ot_create_instance(void)
{
#if OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE
    size_t   otInstanceBufferLength = 0;
    uint8_t *otInstanceBuffer       = NULL;

    // Call to query the buffer size
    (void)otInstanceInit(NULL, &otInstanceBufferLength);

    // Call to allocate the buffer
    otInstanceBuffer = (uint8_t *)malloc(otInstanceBufferLength);
    assert(otInstanceBuffer);

    // Initialize OpenThread with the buffer
    gInstance = otInstanceInit(otInstanceBuffer, &otInstanceBufferLength);
#else
    gInstance = otInstanceInitSingle();
#endif
    assert(gInstance);
}

void sl_ot_ncp_init(void)
{
    otAppNcpInit(gInstance);
}

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/

void app_init(void)
{
    OT_SETUP_RESET_JUMP(argv);
}

void receiveCallback(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo)
{
    OT_UNUSED_VARIABLE(aContext);
    OT_UNUSED_VARIABLE(aMessageInfo);
    uint8_t buf[64];
    int     length;
    otMessage       *message = NULL;
    otMessageInfo    messageInfo;
    
    length = otMessageRead(aMessage, otMessageGetOffset(aMessage), buf, sizeof(buf) - 1);
    if (!strncmp((char *)buf, REQUEST_SLEEP_MESSAGE, length) && !isSleep)
    {
        VerifyOrExit((message = otUdpNewMessage(otGetInstance(), NULL)) != NULL);
        memset(&messageInfo, 0, sizeof(messageInfo));
        memcpy(&messageInfo.mPeerAddr, &aMessageInfo->mPeerAddr, sizeof(otIp6Address));
        messageInfo.mPeerPort = UDP_PORT;

        sl_led_turn_on(&LED_INSTANCE);
        isSleep = true;

        SuccessOrExit(otMessageAppend(message, ACK, (uint16_t)strlen(ACK)));
        SuccessOrExit(otUdpSend(otGetInstance(), &sUdpSocket, message, &messageInfo));

        message = NULL;
    }
    else if (!strncmp((char *)buf, REQUEST_WAKEUP_MESSAGE, length) && isSleep)
    {
        VerifyOrExit((message = otUdpNewMessage(otGetInstance(), NULL)) != NULL);
        memset(&messageInfo, 0, sizeof(messageInfo));
        memcpy(&messageInfo.mPeerAddr, &aMessageInfo->mPeerAddr, sizeof(otIp6Address));
        messageInfo.mPeerPort = UDP_PORT;

        sl_led_turn_off(&LED_INSTANCE);
        isSleep = false;

        SuccessOrExit(otMessageAppend(message, ACK, (uint16_t)strlen(ACK)));
        SuccessOrExit(otUdpSend(otGetInstance(), &sUdpSocket, message, &messageInfo));

        message = NULL;
    }

exit:
    if (message != NULL)
    {
        otMessageFree(message);
    }
    return;
}

void initUdp(void)
{
    otError    error;
    otSockAddr bindAddr;

    // Initialize bindAddr
    memset(&bindAddr, 0, sizeof(bindAddr));
    bindAddr.mPort = UDP_PORT;

    // Open the socket
    error = otUdpOpen(otGetInstance(), &sUdpSocket, receiveCallback, NULL);
    if (error != OT_ERROR_NONE)
    {
        return;
    }

    // Bind to the socket. Close the socket if bind fails.
    error = otUdpBind(otGetInstance(), &sUdpSocket, &bindAddr, OT_NETIF_THREAD);
    if (error != OT_ERROR_NONE)
    {
        IgnoreReturnValue(otUdpClose(otGetInstance(), &sUdpSocket));
        return;
    }

    isCreateUDP = true;
}


/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
void app_process_action(void)
{
    otTaskletsProcess(gInstance);
    otSysProcessDrivers(gInstance);
    otDeviceRole role = otThreadGetDeviceRole(gInstance);
    if ((role == OT_DEVICE_ROLE_CHILD || role == OT_DEVICE_ROLE_ROUTER || role == OT_DEVICE_ROLE_LEADER) && (!isCreateUDP)) 
    {
        initUdp();  
    } 
}

/**************************************************************************//**
 * Application Exit.
 *****************************************************************************/
void app_exit(void)
{
    otInstanceFinalize(gInstance);
#if OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE
    free(otInstanceBuffer);
#endif
    // TO DO : pseudo reset?
}
