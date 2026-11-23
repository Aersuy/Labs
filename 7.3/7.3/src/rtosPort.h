#ifndef RTOS_PORT_H
#define RTOS_PORT_H

// Singura cusatura de platforma din proiect. Lab 7.3 ruleaza acelasi cod de
// cozi pe doua nuclee FreeRTOS diferite: portul feilipu pentru AVR, care isi
// publica antetele ca <Arduino_FreeRTOS.h>, si cel din nucleul ESP-IDF, care le
// pune sub freertos/. Numele difera, API-ul folosit aici nu.
//
// Alternativa ar fi fost un #ifdef in fiecare fisier care deschide o coada;
// aici ea sta o singura data, intr-un antet care spune in nume ce face - acelasi
// tratament pe care serialLink.h il da legaturii seriale, doar ca pe invers:
// acolo doua implementari pentru un antet, aici doua antete pentru un API.
#ifdef ARDUINO_ARCH_AVR
  #include <Arduino_FreeRTOS.h>
  #include <task.h>
  #include <queue.h>
#else
  #include <freertos/FreeRTOS.h>
  #include <freertos/task.h>
  #include <freertos/queue.h>
#endif

#endif
