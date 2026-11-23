#ifndef RPC_COMMAND_H
#define RPC_COMMAND_H

#include <stdint.h>

// Granita cu exteriorul: subiectul si corpul vin de la server, deci nu au nici
// o garantie de forma. Aici se verifica tot (subiect, JSON, metoda, parametru)
// si se iese cu o structura care nu mai poate minti. Modulul este determinist,
// deci se testeaza pe calculator, inclusiv cazurile stricate.
enum RpcMethod : uint8_t {
    RPC_SET_LED,
    RPC_GET_LED
};

enum RpcParseResult : uint8_t {
    RPC_PARSE_OK,
    RPC_PARSE_BAD_TOPIC,            // alt subiect sau <id> lipsa/neintreg
    RPC_PARSE_BAD_JSON,
    RPC_PARSE_UNKNOWN_METHOD,
    RPC_PARSE_BAD_PARAMS            // metoda buna, parametru care nu e boolean
};

// isOn are inteles numai pentru RPC_SET_LED. requestId este cel din subiect:
// raspunsul trebuie trimis pe exact acel numar, altfel tabloul de bord
// considera apelul netrimis.
struct RpcRequest {
    uint32_t requestId;
    RpcMethod method;
    bool isOn;
};

// requestOut se scrie numai cand se intoarce RPC_PARSE_OK.
RpcParseResult parseRpcRequest(const char *topic, const char *json,
                               uint16_t jsonLength, RpcRequest *requestOut);

const char *rpcParseResultName(RpcParseResult result);

#endif
