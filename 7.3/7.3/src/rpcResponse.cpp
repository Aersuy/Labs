#include "rpcResponse.h"

#include <stdio.h>
#include <stddef.h>

#include "thingsBoardTopics.h"

// Cheia este aceeasi ca in telemetrie: tabloul de bord citeste starea LED-ului
// dintr-un singur nume, indiferent daca ea vine periodic sau ca raspuns la apel.
static const char RPC_RESPONSE_FORMAT[] = "{\"ledOn\":%s}";

uint8_t formatRpcResponseTopic(uint32_t requestId, char *out, uint8_t len)
{
    if (out == NULL || len == 0) return 0;
    if (len < RPC_RESPONSE_TOPIC_MIN_LEN)
    {
        out[0] = '\0';
        return 0;
    }

    int written = snprintf(out, len, "%s%lu", TB_RPC_RESPONSE_PREFIX,
                           (unsigned long)requestId);
    return (uint8_t)written;
}

uint8_t formatRpcResponse(bool isOn, char *out, uint8_t len)
{
    if (out == NULL || len == 0) return 0;
    if (len < RPC_RESPONSE_TEXT_MIN_LEN)
    {
        out[0] = '\0';
        return 0;
    }

    int written = snprintf(out, len, RPC_RESPONSE_FORMAT, isOn ? "true" : "false");
    return (uint8_t)written;
}
