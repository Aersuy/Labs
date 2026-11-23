# Lab 7.3 — comunicare Internet prin MQTT cu o resursa Cloud

Nodul de senzor si actuator din 7.2 (Arduino Uno, KY-013 + LED) ramane neschimbat
pe fir; un ESP32 face puntea dintre protocolul binar al laboratorului si
ThingsBoard, unde datele se vad si se comanda de pe un tablou de bord.

```
KY-013 ──A0┐                                       ┌─ WiFi ─ demo.thingsboard.io:1883
           │  Arduino Uno (node)                   │
LED ────D8─┤  FreeRTOS, 3 taskuri      ESP32 (gateway)
           │                                       │
      D11 TX ──[1k8]──┬──> GPIO16 (RX2)            └─ grafic + comutator + indicator
                   [3k3]
                      └── GND
      D10 RX <──────────── GPIO17 (TX2)
           GND ─────────── GND
```

Cablare: TX-ul nodului este de 5 V, deci intra in RX-ul ESP32 prin divizorul
1k8 / 3k3 (aproximativ 3,2 V). In sens invers firul merge direct: 3,3 V sunt
citite corect de intrarea de 5 V a nodului. Masa este comuna, dar **5V si 3V3 nu
se leaga niciodata intre ele** — fiecare placa se alimenteaza pe USB-ul ei, asa
se vad si cele doua jurnale in acelasi timp.

## Ce face fiecare placa

| Placa | Taskuri | Rol |
|---|---|---|
Uno (`mainNode.cpp`) | RECV, PROC, RESP | citeste KY-013, comanda LED-ul, raspunde la cereri cu expirare si numarare de pierderi |
ESP32 (`mainGateway.cpp`) | RECV, EXCH, MQTT | intreaba nodul la fiecare secunda, publica telemetria, primeste apeluri RPC si comuta actuatorul |

Protocolul dintre ele este cel din 7.2, neschimbat: `STX ID SEQ CMD LEN PAYLOAD
CHK ETX`, cu `CMD_READ_TEMP` pentru senzor si `CMD_SET_LED` pentru actuator.

O comanda primita de la tabloul de bord nu se executa in taskul de retea: ea
trece prin coada catre taskul care detine legatura seriala, iar confirmarea RPC
pleaca abia dupa ce nodul a raspuns. Astfel comutatorul din tablou arata starea
reala a LED-ului, nu ce i s-a cerut.

## Pregatire

1. Dispozitivul in ThingsBoard: [demo.thingsboard.io](https://demo.thingsboard.io)
   → *Entities* → *Devices* → **+** → nume (de exemplu `lab73`) → *Add*. Apoi
   deschide dispozitivul → *Copy access token*.
2. Datele locale:

   ```
   copy include\secrets.example.h include\secrets.h
   ```

   Completeaza `WIFI_SSID`, `WIFI_PASSWORD` si `TB_ACCESS_TOKEN`. Fisierul
   `include/secrets.h` este ignorat de git si nu trebuie sa apara in capturi de
   ecran: simbolul de acces este echivalentul unei parole.
3. Reteaua trebuie sa fie de 2,4 GHz — ESP32 nu vede benzile de 5 GHz.

## Compilare, incarcare, teste

```
pio test -e native                                  # 10 suite, 70 de cazuri
pio run -e node    -t upload --upload-port COM3     # Arduino Uno
pio run -e gateway -t upload --upload-port COM4     # ESP32
pio device monitor --port COM3 -b 9600              # jurnalul nodului
pio device monitor --port COM4 -b 115200            # jurnalul punctii
```

## Tabloul de bord

Telemetria pleaca pe `v1/devices/me/telemetry` ca
`{"temperature":23.4,"ledOn":true}`, deci in ThingsBoard apar doua chei:

- `temperature` → widget *Time series chart*;
- `ledOn` → widget *LED indicator* (sau *Switch* doar pentru afisare).

Comanda merge prin RPC, nu prin telemetrie: widgetul *Switch control* se
configureaza cu metoda `setLedState` (valoarea trimisa este `true` / `false`).
Metoda `getLedState` intoarce starea curenta fara sa o schimbe, utila pentru un
buton de verificare sau pentru incercari din *Device* → *Latest telemetry*.

## Ce se vede cand ceva nu merge

| Jurnal | Inteles |
|---|---|
`conectare MQTT refuzata (stare -2)` | nu ajunge la server: WiFi slab, port blocat, gazda gresita |
`conectare MQTT refuzata (stare 4 sau 5)` | simbolul de acces nu este primit: `TB_ACCESS_TOKEN` gresit |
`CMD_READ_TEMP seq=.. fara raspuns de la nod` | legatura seriala: fire inversate, divizor lipsa, mase nelegate |
`apel RPC refuzat: parametru invalid` | widgetul trimite altceva decat `true`/`false` |
`telemetrie ... ` la secunda, dar graficul gol | dispozitivul din tablou nu este cel al simbolului folosit |

Pe nod, un LED care clipeste rapid pe D13 inseamna RAM epuizat
(`vApplicationMallocFailedHook`) — de aceea LED-ul comandat prin protocol sta pe
D8, ca sa nu ascunda exact acest semnal.
