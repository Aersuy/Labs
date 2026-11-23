#ifndef DISTANCE_BUFFER_H
#define DISTANCE_BUFFER_H

#include <stdint.h>

// Tamponul partajat dintre taskul care formeaza cadrul si rutina de
// intrerupere TWI care il trimite masterului.
//
// Pe AVR, apelul Wire.onRequest() se executa in ISR-ul TWI, deci nu poate lua
// un mutex si nu are voie sa apeleze functii FreeRTOS care blocheaza. De aceea
// nu se foloseste modelul cu semafor din labul 3.x: sincronizarea se face cu
// doua sloturi si comutarea unui singur octet.
void publishFrame(const uint8_t *frame, uint8_t frameLen);

// Intoarce cadrul comis si lungimea lui prin frameLenOut. Se apeleaza numai
// din ISR: pe un AVR cu un singur nucleu, un task nu poate intrerupe o rutina
// de intrerupere, deci cadrul indicat nu poate fi rescris in timpul copierii
// facute de Wire.write().
const uint8_t *committedFrame(uint8_t *frameLenOut);

#endif
