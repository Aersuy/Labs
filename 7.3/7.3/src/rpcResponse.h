#ifndef RPC_RESPONSE_H
#define RPC_RESPONSE_H

#include <stdint.h>

// Confirmarea trimisa inapoi serverului dupa ce actuatorul a fost chiar comutat.
// Tabloul de bord asteapta raspunsul pe subiectul cu acelasi <id> ca cererea;
// fara el, widgetul de comutator ramane in starea "se trimite".
//
// Determinist: subiectul si corpul se construiesc din id si din starea reala a
// LED-ului, amandoua primite ca argumente.

// Prefixul are 27 de caractere, iar un uint32_t are cel mult 10 cifre.
constexpr uint8_t RPC_RESPONSE_TOPIC_MIN_LEN = 40;

// "{\"ledOn\":false}" plus terminator.
constexpr uint8_t RPC_RESPONSE_TEXT_MIN_LEN = 20;

// Amandoua intorc numarul de caractere scrise (fara terminator), sau 0 cand
// tamponul este prea mic - caz in care out primeste un sir vid.
uint8_t formatRpcResponseTopic(uint32_t requestId, char *out, uint8_t len);
uint8_t formatRpcResponse(bool isOn, char *out, uint8_t len);

#endif
