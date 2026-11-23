#include "rpcCommand.h"

#include <ArduinoJson.h>
#include <stddef.h>
#include <string.h>

#include "thingsBoardTopics.h"

// Corpul unui apel RPC are cateva zeci de octeti. Limita exista ca un mesaj
// umflat sa fie refuzat inainte de analiza, nu dupa ce a cerut memorie.
constexpr uint16_t RPC_JSON_MAX_LEN = 256;

static bool parseUnsignedDecimal(const char *text, uint32_t *valueOut)
{
    if (*text == '\0') return false;

    uint32_t value = 0;
    for (const char *reader = text; *reader != '\0'; reader++)
    {
        if (*reader < '0' || *reader > '9') return false;

        uint32_t digit = (uint32_t)(*reader - '0');
        if (value > (UINT32_MAX - digit) / 10u) return false;   // ar depasi
        value = value * 10u + digit;
    }
    *valueOut = value;
    return true;
}

// Abonarea se face pe .../rpc/request/+, deci orice subiect care ajunge aici ar
// trebui sa aiba prefixul; se verifica oricum, pentru ca brokerul nu este sub
// controlul nostru si un subiect strain nu are voie sa fie interpretat.
static bool parseRequestId(const char *topic, uint32_t *idOut)
{
    size_t prefixLength = strlen(TB_RPC_REQUEST_PREFIX);
    if (strncmp(topic, TB_RPC_REQUEST_PREFIX, prefixLength) != 0) return false;

    return parseUnsignedDecimal(topic + prefixLength, idOut);
}

static bool readBooleanValue(JsonVariantConst value, bool *isOnOut)
{
    if (value.is<bool>())
    {
        *isOnOut = value.as<bool>();
        return true;
    }
    if (!value.is<int>()) return false;

    int number = value.as<int>();
    if (number != 0 && number != 1) return false;

    *isOnOut = (number == 1);
    return true;
}

// Widgetul de comutator trimite params ca boolean, unele configuratii il
// ambaleaza ca {"value":true}, iar incercarile manuale trimit 0/1. Toate trei
// spun acelasi lucru; orice altceva (sir, lipsa, numar oarecare) este refuzat.
static bool readLedParam(JsonVariantConst params, bool *isOnOut)
{
    if (readBooleanValue(params, isOnOut)) return true;

    return readBooleanValue(params["value"], isOnOut);
}

static RpcParseResult readMethod(JsonObjectConst root, RpcRequest *requestOut)
{
    const char *method = root["method"].as<const char *>();
    if (method == NULL) return RPC_PARSE_UNKNOWN_METHOD;

    if (strcmp(method, RPC_METHOD_GET_LED) == 0)
    {
        requestOut->method = RPC_GET_LED;
        return RPC_PARSE_OK;
    }
    if (strcmp(method, RPC_METHOD_SET_LED) != 0) return RPC_PARSE_UNKNOWN_METHOD;

    requestOut->method = RPC_SET_LED;
    if (!readLedParam(root["params"], &requestOut->isOn)) return RPC_PARSE_BAD_PARAMS;
    return RPC_PARSE_OK;
}

static RpcParseResult readRoot(const char *json, uint16_t jsonLength,
                               JsonDocument *documentOut)
{
    if (jsonLength == 0 || jsonLength > RPC_JSON_MAX_LEN) return RPC_PARSE_BAD_JSON;
    if (deserializeJson(*documentOut, json, jsonLength) != DeserializationError::Ok)
    {
        return RPC_PARSE_BAD_JSON;
    }
    return documentOut->is<JsonObjectConst>() ? RPC_PARSE_OK : RPC_PARSE_BAD_JSON;
}

RpcParseResult parseRpcRequest(const char *topic, const char *json,
                               uint16_t jsonLength, RpcRequest *requestOut)
{
    if (topic == NULL || requestOut == NULL) return RPC_PARSE_BAD_TOPIC;
    if (json == NULL) return RPC_PARSE_BAD_JSON;

    RpcRequest parsed;
    parsed.isOn = false;
    if (!parseRequestId(topic, &parsed.requestId)) return RPC_PARSE_BAD_TOPIC;

    JsonDocument document;
    RpcParseResult result = readRoot(json, jsonLength, &document);
    if (result != RPC_PARSE_OK) return result;

    result = readMethod(document.as<JsonObjectConst>(), &parsed);
    if (result == RPC_PARSE_OK) *requestOut = parsed;
    return result;
}

const char *rpcParseResultName(RpcParseResult result)
{
    switch (result)
    {
        case RPC_PARSE_OK: return "OK";
        case RPC_PARSE_BAD_TOPIC: return "subiect invalid";
        case RPC_PARSE_BAD_JSON: return "JSON invalid";
        case RPC_PARSE_UNKNOWN_METHOD: return "metoda necunoscuta";
        case RPC_PARSE_BAD_PARAMS: return "parametru invalid";
        default: break;
    }
    return "necunoscut";
}
