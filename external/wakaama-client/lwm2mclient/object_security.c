/*******************************************************************************
 *
 * Copyright (c) 2013, 2014 Intel Corporation and others.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * The Eclipse Distribution License is available at
 *    http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    David Navarro, Intel Corporation - initial API and implementation
 *    Bosch Software Innovations GmbH - Please refer to git log
 *    Pascal Rieux - Please refer to git log
 *    
 *******************************************************************************/

/*
 *  Resources:
 *
 *          Name            | ID | Operations | Instances | Mandatory |  Type   |  Range  | Units |
 *  Server URI              |  0 |            |  Single   |    Yes    | String  |         |       |
 *  Bootstrap Server        |  1 |            |  Single   |    Yes    | Boolean |         |       |
 *  Security Mode           |  2 |            |  Single   |    Yes    | Integer |   0-3   |       |
 *  Public Key or ID        |  3 |            |  Single   |    Yes    | Opaque  |         |       |
 *  Server Public Key or ID |  4 |            |  Single   |    Yes    | Opaque  |         |       |
 *  Secret Key              |  5 |            |  Single   |    Yes    | Opaque  |         |       |
 *  SMS Security Mode       |  6 |            |  Single   |    Yes    | Integer |  0-255  |       |
 *  SMS Binding Key Param.  |  7 |            |  Single   |    Yes    | Opaque  |   6 B   |       |
 *  SMS Binding Secret Keys |  8 |            |  Single   |    Yes    | Opaque  | 32-48 B |       |
 *  Server SMS Number       |  9 |            |  Single   |    Yes    | Integer |         |       |
 *  Short Server ID         | 10 |            |  Single   |    No     | Integer | 1-65535 |       |
 *  Client Hold Off Time    | 11 |            |  Single   |    Yes    | Integer |         |   s   |
 *
 */

/*
 * Here we implement a very basic LWM2M Security Object which only knows NoSec security mode.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "liblwm2m.h"
#include "pem_utils.h"

#define LWM2M_SECURITY_URI_ID                 0
#define LWM2M_SECURITY_BOOTSTRAP_ID           1
#define LWM2M_SECURITY_MODE_ID            2
#define LWM2M_SECURITY_PUBLIC_KEY_ID          3
#define LWM2M_SECURITY_SERVER_PUBLIC_KEY_ID   4
#define LWM2M_SECURITY_SECRET_KEY_ID          5
#define LWM2M_SECURITY_SMS_SECURITY_ID        6
#define LWM2M_SECURITY_SMS_KEY_PARAM_ID       7
#define LWM2M_SECURITY_SMS_SECRET_KEY_ID      8
#define LWM2M_SECURITY_SMS_SERVER_NUMBER_ID   9
#define LWM2M_SECURITY_SHORT_SERVER_ID        10
#define LWM2M_SECURITY_HOLD_OFF_ID            11

typedef struct _security_instance_
{
    struct _security_instance_ * next;        // matches lwm2m_list_t::next
    uint16_t                     instanceId;  // matches lwm2m_list_t::id
    char *                       uri;
    bool                         isBootstrap;
    uint8_t                      securityMode;
    char *                       publicIdentity;
    uint16_t                     publicIdLen;
    char *                       serverPublicKey;
    uint16_t                     serverPublicKeyLen;
    char *                       secretKey;
    uint16_t                     secretKeyLen;
    uint8_t                      smsSecurityMode;
    char *                       smsParams; // SMS binding key parameters
    uint16_t                     smsParamsLen;
    char *                       smsSecret; // SMS binding secret key
    uint16_t                     smsSecretLen;
    uint16_t                     shortID;
    uint32_t                     clientHoldOffTime;
} security_instance_t;

static uint8_t prv_get_value(lwm2m_data_t * dataP,
                             security_instance_t * targetP)
{
    switch (dataP->id)
    {
    case LWM2M_SECURITY_URI_ID:
        lwm2m_data_encode_string(targetP->uri, dataP);
        return COAP_205_CONTENT;

    case LWM2M_SECURITY_BOOTSTRAP_ID:
        lwm2m_data_encode_bool(targetP->isBootstrap, dataP);
        return COAP_205_CONTENT;

    case LWM2M_SECURITY_SECURITY_ID:
        lwm2m_data_encode_int(targetP->securityMode, dataP);
        return COAP_205_CONTENT;

    case LWM2M_SECURITY_PUBLIC_KEY_ID:
        lwm2m_data_encode_opaque((uint8_t*)targetP->publicIdentity, targetP->publicIdLen, dataP);
        return COAP_205_CONTENT;

    case LWM2M_SECURITY_SERVER_PUBLIC_KEY_ID:
        lwm2m_data_encode_opaque((uint8_t*)targetP->serverPublicKey, targetP->serverPublicKeyLen, dataP);
        return COAP_205_CONTENT;

    case LWM2M_SECURITY_SECRET_KEY_ID:
        lwm2m_data_encode_opaque((uint8_t*)targetP->secretKey, targetP->secretKeyLen, dataP);
        return COAP_205_CONTENT;

    case LWM2M_SECURITY_SMS_SECURITY_ID:
        lwm2m_data_encode_int(targetP->smsSecurityMode, dataP);
        return COAP_205_CONTENT;

    case LWM2M_SECURITY_SMS_KEY_PARAM_ID:
        lwm2m_data_encode_opaque((uint8_t*)targetP->smsParams, targetP->smsParamsLen, dataP);
        return COAP_205_CONTENT;

    case LWM2M_SECURITY_SMS_SECRET_KEY_ID:
        lwm2m_data_encode_opaque((uint8_t*)targetP->smsSecret, targetP->smsSecretLen, dataP);
        return COAP_205_CONTENT;

    case LWM2M_SECURITY_SMS_SERVER_NUMBER_ID:
        lwm2m_data_encode_int(0, dataP);
        return COAP_205_CONTENT;

    case LWM2M_SECURITY_SHORT_SERVER_ID:
        lwm2m_data_encode_int(targetP->shortID, dataP);
        return COAP_205_CONTENT;

    case LWM2M_SECURITY_HOLD_OFF_ID:
        lwm2m_data_encode_int(targetP->clientHoldOffTime, dataP);
        return COAP_205_CONTENT;

    default:
        return COAP_404_NOT_FOUND;
    }
}

static uint8_t prv_security_read(uint16_t instanceId,
                                 int * numDataP,
                                 lwm2m_data_t ** dataArrayP,
                                 lwm2m_object_t * objectP)
{
    security_instance_t * targetP;
    uint8_t result;
    int i;

    targetP = (security_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;

    // is the server asking for the full instance ?
    if (*numDataP == 0)
    {
        uint16_t resList[] = {LWM2M_SECURITY_URI_ID,
                              LWM2M_SECURITY_BOOTSTRAP_ID,
                              LWM2M_SECURITY_SECURITY_ID,
                              LWM2M_SECURITY_PUBLIC_KEY_ID,
                              LWM2M_SECURITY_SERVER_PUBLIC_KEY_ID,
                              LWM2M_SECURITY_SECRET_KEY_ID,
                              LWM2M_SECURITY_SMS_SECURITY_ID,
                              LWM2M_SECURITY_SMS_KEY_PARAM_ID,
                              LWM2M_SECURITY_SMS_SECRET_KEY_ID,
                              LWM2M_SECURITY_SMS_SERVER_NUMBER_ID,
                              LWM2M_SECURITY_SHORT_SERVER_ID,
                              LWM2M_SECURITY_HOLD_OFF_ID};
        int nbRes = sizeof(resList)/sizeof(uint16_t);

        *dataArrayP = lwm2m_data_new(nbRes);
        if (*dataArrayP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;
        *numDataP = nbRes;
        for (i = 0 ; i < nbRes ; i++)
        {
            (*dataArrayP)[i].id = resList[i];
        }
    }

    i = 0;
    do
    {
        result = prv_get_value((*dataArrayP) + i, targetP);
        i++;
    } while (i < *numDataP && result == COAP_205_CONTENT);

    return result;
}

#ifdef LWM2M_BOOTSTRAP

static uint8_t prv_security_write(uint16_t instanceId,
                                  int numData,
                                  lwm2m_data_t * dataArray,
                                  lwm2m_object_t * objectP)
{
    security_instance_t * targetP;
    int i;
    uint8_t result = COAP_204_CHANGED;

    targetP = (security_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP)
    {
        return COAP_404_NOT_FOUND;
    }

    i = 0;
    do {
        switch (dataArray[i].id)
        {
        case LWM2M_SECURITY_URI_ID:
            if (targetP->uri != NULL) lwm2m_free(targetP->uri);
            targetP->uri = (char *)lwm2m_malloc(dataArray[i].value.asBuffer.length + 1);
            memset(targetP->uri, 0, dataArray[i].value.asBuffer.length + 1);
            if (targetP->uri != NULL)
            {
                strncpy(targetP->uri, (char*)dataArray[i].value.asBuffer.buffer, dataArray[i].value.asBuffer.length);
                result = COAP_204_CHANGED;
            }
            else
            {
                result = COAP_500_INTERNAL_SERVER_ERROR;
            }
            break;

        case LWM2M_SECURITY_BOOTSTRAP_ID:
            if (1 == lwm2m_data_decode_bool(dataArray + i, &(targetP->isBootstrap)))
            {
                result = COAP_204_CHANGED;
            }
            else
            {
                result = COAP_400_BAD_REQUEST;
            }
            break;

        case LWM2M_SECURITY_SECURITY_ID:
        {
            int64_t value;

            if (1 == lwm2m_data_decode_int(dataArray + i, &value))
            {
                if (value >= 0 && value <= 3)
                {
                    targetP->securityMode = value;
                    result = COAP_204_CHANGED;
                }
                else
                {
                    result = COAP_406_NOT_ACCEPTABLE;
                }
            }
            else
            {
                result = COAP_400_BAD_REQUEST;
            }
        }
        break;
        case LWM2M_SECURITY_PUBLIC_KEY_ID:
            if (targetP->publicIdentity != NULL) lwm2m_free(targetP->publicIdentity);
            targetP->publicIdentity = (char *)lwm2m_malloc(dataArray[i].value.asBuffer.length +1);
            memset(targetP->publicIdentity, 0, dataArray[i].value.asBuffer.length + 1);
            if (targetP->publicIdentity != NULL)
            {
                memcpy(targetP->publicIdentity, (char*)dataArray[i].value.asBuffer.buffer, dataArray[i].value.asBuffer.length);
                targetP->publicIdLen = dataArray[i].value.asBuffer.length;
                result = COAP_204_CHANGED;
            }
            else
            {
                result = COAP_500_INTERNAL_SERVER_ERROR;
            }
            break;

        case LWM2M_SECURITY_SERVER_PUBLIC_KEY_ID:
            if (targetP->serverPublicKey != NULL) lwm2m_free(targetP->serverPublicKey);
            targetP->serverPublicKey = (char *)lwm2m_malloc(dataArray[i].value.asBuffer.length +1);
            memset(targetP->serverPublicKey, 0, dataArray[i].value.asBuffer.length + 1);
            if (targetP->serverPublicKey != NULL)
            {
                memcpy(targetP->serverPublicKey, (char*)dataArray[i].value.asBuffer.buffer, dataArray[i].value.asBuffer.length);
                targetP->serverPublicKeyLen = dataArray[i].value.asBuffer.length;
                result = COAP_204_CHANGED;
            }
            else
            {
                result = COAP_500_INTERNAL_SERVER_ERROR;
            }
            break;

        case LWM2M_SECURITY_SECRET_KEY_ID:
            if (targetP->secretKey != NULL) lwm2m_free(targetP->secretKey);
            targetP->secretKey = (char *)lwm2m_malloc(dataArray[i].value.asBuffer.length +1);
            memset(targetP->secretKey, 0, dataArray[i].value.asBuffer.length + 1);
            if (targetP->secretKey != NULL)
            {
                memcpy(targetP->secretKey, (char*)dataArray[i].value.asBuffer.buffer, dataArray[i].value.asBuffer.length);
                targetP->secretKeyLen = dataArray[i].value.asBuffer.length;
                result = COAP_204_CHANGED;
            }
            else
            {
                result = COAP_500_INTERNAL_SERVER_ERROR;
            }
            break;

        case LWM2M_SECURITY_SMS_SECURITY_ID:
            // Let just ignore this
            result = COAP_204_CHANGED;
            break;

        case LWM2M_SECURITY_SMS_KEY_PARAM_ID:
            // Let just ignore this
            result = COAP_204_CHANGED;
            break;

        case LWM2M_SECURITY_SMS_SECRET_KEY_ID:
            // Let just ignore this
            result = COAP_204_CHANGED;
            break;

        case LWM2M_SECURITY_SMS_SERVER_NUMBER_ID:
            // Let just ignore this
            result = COAP_204_CHANGED;
            break;

        case LWM2M_SECURITY_SHORT_SERVER_ID:
        {
            int64_t value;

            if (1 == lwm2m_data_decode_int(dataArray + i, &value))
            {
                if (value >= 0 && value <= 0xFFFF)
                {
                    targetP->shortID = value;
                    result = COAP_204_CHANGED;
                }
                else
                {
                    result = COAP_406_NOT_ACCEPTABLE;
                }
            }
            else
            {
                result = COAP_400_BAD_REQUEST;
            }
        }
        break;

        case LWM2M_SECURITY_HOLD_OFF_ID:
        {
            int64_t value;

            if (1 == lwm2m_data_decode_int(dataArray + i, &value))
            {
                if (value >= 0 && value <= 0xFFFF)
                {
                    targetP->clientHoldOffTime = value;
                    result = COAP_204_CHANGED;
                }
                else
                {
                    result = COAP_406_NOT_ACCEPTABLE;
                }
            }
            else
            {
                result = COAP_400_BAD_REQUEST;
            }
            break;
        }
        default:
            return COAP_404_NOT_FOUND;
        }
        i++;
    } while (i < numData && result == COAP_204_CHANGED);

    return result;
}

static uint8_t prv_security_delete(uint16_t id,
                                   lwm2m_object_t * objectP)
{
    security_instance_t * targetP;

    objectP->instanceList = lwm2m_list_remove(objectP->instanceList, id, (lwm2m_list_t **)&targetP);
    if (NULL == targetP) return COAP_404_NOT_FOUND;
    if (NULL != targetP->uri)
    {
        lwm2m_free(targetP->uri);
    }

    lwm2m_free(targetP);

    return COAP_202_DELETED;
}

static uint8_t prv_security_create(uint16_t instanceId,
                                   int numData,
                                   lwm2m_data_t * dataArray,
                                   lwm2m_object_t * objectP)
{
    security_instance_t * targetP;
    uint8_t result;

    targetP = (security_instance_t *)lwm2m_malloc(sizeof(security_instance_t));
    if (NULL == targetP) return COAP_500_INTERNAL_SERVER_ERROR;
    memset(targetP, 0, sizeof(security_instance_t));

    targetP->instanceId = instanceId;
    objectP->instanceList = LWM2M_LIST_ADD(objectP->instanceList, targetP);

    result = prv_security_write(instanceId, numData, dataArray, objectP);

    if (result != COAP_204_CHANGED)
    {
        (void)prv_security_delete(instanceId, objectP);
    }
    else
    {
        result = COAP_201_CREATED;
    }

    return result;
}
#endif

void copy_security_object(lwm2m_object_t * objectDest, lwm2m_object_t * objectSrc)
{
    memcpy(objectDest, objectSrc, sizeof(lwm2m_object_t));
    objectDest->instanceList = NULL;
    objectDest->userData = NULL;
    security_instance_t * instanceSrc = (security_instance_t *)objectSrc->instanceList;
    security_instance_t * previousInstanceDest = NULL;
    while (instanceSrc != NULL)
    {
        security_instance_t * instanceDest = (security_instance_t *)lwm2m_malloc(sizeof(security_instance_t));
        if (NULL == instanceDest)
        {
            return;
        }
        memcpy(instanceDest, instanceSrc, sizeof(security_instance_t));
        instanceDest->uri = (char*)lwm2m_malloc(strlen(instanceSrc->uri) + 1);
        strcpy(instanceDest->uri, instanceSrc->uri);
        instanceSrc = (security_instance_t *)instanceSrc->next;
        if (previousInstanceDest == NULL)
        {
            objectDest->instanceList = (lwm2m_list_t *)instanceDest;
        }
        else
        {
            previousInstanceDest->next = instanceDest;
        }
        previousInstanceDest = instanceDest;
    }
}

void display_security_object(lwm2m_object_t * object)
{
#ifdef WITH_LOGS
    fprintf(stdout, "  /%u: Security object, instances:\r\n", object->objID);
    security_instance_t * instance = (security_instance_t *)object->instanceList;
    while (instance != NULL)
    {
        fprintf(stdout, "    /%u/%u: instanceId: %u, uri: %s, isBootstrap: %s, shortId: %u, clientHoldOffTime: %u\r\n",
                object->objID, instance->instanceId,
                instance->instanceId, instance->uri, instance->isBootstrap ? "true" : "false",
                instance->shortID, instance->clientHoldOffTime);
        instance = (security_instance_t *)instance->next;
    }
#endif
}

void clean_security_object(lwm2m_object_t * objectP)
{
    while (objectP->instanceList != NULL)
    {
        security_instance_t * securityInstance = (security_instance_t *)objectP->instanceList;
        objectP->instanceList = objectP->instanceList->next;
        if (NULL != securityInstance->uri)
        {
            lwm2m_free(securityInstance->uri);
        }

        if (securityInstance->securityMode == LWM2M_SECURITY_MODE_PRE_SHARED_KEY || securityInstance->securityMode == LWM2M_SECURITY_MODE_CERTIFICATE)
        {
            if (securityInstance->publicIdentity) {
                lwm2m_free(securityInstance->publicIdentity);
            }

            if (securityInstance->secretKey) {
                lwm2m_free(securityInstance->secretKey);
            }
        }

        if (securityInstance->securityMode == LWM2M_SECURITY_MODE_CERTIFICATE)
        {
            if (securityInstance->serverPublicKey) {
                lwm2m_free(securityInstance->serverPublicKey);
            }
        }
        lwm2m_free(securityInstance);
    }
}

lwm2m_object_t * get_security_object(int serverId,
                                     const char* serverUri,
                                     uint8_t securityMode,
                                     char * serverCertificate,
                                     char * clientCertificateOrPskId,
                                     char * psk,
                                     uint16_t pskLen,
                                     bool isBootstrap)
{
    lwm2m_object_t *securityObj = NULL;
    security_instance_t *targetP = NULL;

    securityObj = (lwm2m_object_t *)lwm2m_malloc(sizeof(lwm2m_object_t));
    if (!securityObj)
    {
#ifdef WITH_LOGS
        fprintf(stderr, "Failed to allocate security object\r\n");
#endif
        return NULL;
    }

    memset(securityObj, 0, sizeof(lwm2m_object_t));
    securityObj->objID = LWM2M_SECURITY_OBJECT_ID;

    // Manually create an hard-coded instance
    targetP = (security_instance_t *)lwm2m_malloc(sizeof(security_instance_t));
    if (!targetP)
    {
#ifdef WITH_LOGS
        fprintf(stderr, "Failed to allocate security instance.\r\n");
#endif
        lwm2m_free(securityObj);
        return NULL;
    }

    memset(targetP, 0, sizeof(security_instance_t));
    targetP->securityMode = securityMode;
    targetP->uri = lwm2m_strdup(serverUri);

    if (!targetP->uri) {
#ifdef WITH_LOGS
        fprintf(stderr, "Failed to allocate memory for URI\r\n");
#endif
        goto error;
    }

    if (securityMode == LWM2M_SECURITY_MODE_PRE_SHARED_KEY)
    {
        if (psk == NULL || clientCertificateOrPskId == NULL || pskLen < 1)
        {
#ifdef WITH_LOGS
            fprintf(stderr, "Bad parameters for PSK mode.\r\n");
#endif
            goto error;
        }

        targetP->publicIdentity = lwm2m_strdup(clientCertificateOrPskId);
        targetP->publicIdLen = strlen(clientCertificateOrPskId);
        targetP->secretKey = (char *)lwm2m_malloc(pskLen);
        if (!targetP->secretKey)
        {
#ifdef WITH_LOGS
            fprintf(stderr, "Failed to allocate secretKey.\r\n");
#endif
            goto error;
        }

        memcpy(targetP->secretKey, psk, pskLen);
        targetP->secretKeyLen = pskLen;
    }

    if (securityMode == LWM2M_SECURITY_MODE_CERTIFICATE)
    {
        if (!convert_pem_x509_to_der(serverCertificate, &targetP->serverPublicKey, &targetP->serverPublicKeyLen))
        {
#ifdef WITH_LOGS
            fprintf(stderr, "Failed to parse server certificate\r\n");
#endif
        }

        if (!convert_pem_x509_to_der(clientCertificateOrPskId, &targetP->publicIdentity, &targetP->publicIdLen))
        {
#ifdef WITH_LOGS
            fprintf(stderr, "Failed to parse client certificate\r\n");
#endif
            goto error;
        }

        if (!convert_pem_privatekey_to_der(psk, &targetP->secretKey, &targetP->secretKeyLen))
        {
#ifdef WITH_LOGS
            fprintf(stderr, "Failed to parse private key (Certificate mode)\r\n");
#endif
            goto error;
        }
    }

    // ARTIK Cloud does not support NoSec mode
    if (securityMode == LWM2M_SECURITY_MODE_NONE)
    {
#ifdef WITH_LOGS
        fprintf(stderr, "NoSec is not supported.\r\n");
#endif
        goto error;
    }

    targetP->isBootstrap = isBootstrap;
    targetP->shortID = serverId;
    targetP->clientHoldOffTime = 10;

    securityObj->instanceList = LWM2M_LIST_ADD(securityObj->instanceList, targetP);

    securityObj->readFunc = prv_security_read;
#ifdef LWM2M_BOOTSTRAP
    securityObj->writeFunc = prv_security_write;
    securityObj->createFunc = prv_security_create;
    securityObj->deleteFunc = prv_security_delete;
#endif

    return securityObj;

error:
    if (targetP) {
        if (targetP->serverPublicKey)
            lwm2m_free(targetP->serverPublicKey);
        if (targetP->publicIdentity)
            lwm2m_free(targetP->publicIdentity);
        if (targetP->secretKey)
            lwm2m_free(targetP->secretKey);
        if (targetP->uri)
            lwm2m_free(targetP->uri);
        lwm2m_free(targetP);
    }
    clean_security_object(securityObj);

    return NULL;
}

char * get_server_uri(lwm2m_object_t * objectP,
                      uint16_t secObjInstID)
{
    security_instance_t * targetP = (security_instance_t *)LWM2M_LIST_FIND(objectP->instanceList, secObjInstID);

    if (NULL != targetP)
    {
        return lwm2m_strdup(targetP->uri);
    }

    return NULL;
}

uint16_t get_server_id(lwm2m_object_t * objectP, uint16_t secObjInstID)
{
    security_instance_t *targetP = (security_instance_t *)LWM2M_LIST_FIND(objectP->instanceList, secObjInstID);

    if (NULL != targetP)
    {
        return targetP->shortID;
    }

    return LWM2M_MAX_ID;
}
