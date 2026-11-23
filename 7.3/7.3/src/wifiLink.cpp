#include <Arduino.h>
#include <WiFi.h>

#include "wifiLink.h"

// Scris din taskul de evenimente al stivei WiFi, citit din taskul de retea.
static volatile uint16_t addressCount = 0;

static void onWifiEvent(arduino_event_id_t event)
{
    if (event != ARDUINO_EVENT_WIFI_STA_GOT_IP) return;
    if (addressCount < 0xFFFF) addressCount++;
}

// Nu blocheaza: begin() porneste doar conectarea, iar taskul de retea verifica
// isWifiConnected() inainte de fiecare incercare MQTT. Asa pornirea nu atarna
// de prezenta retelei, iar jurnalul se vede imediat.
void wifiBegin(const char *ssid, const char *password)
{
    WiFi.onEvent(onWifiEvent);
    WiFi.mode(WIFI_STA);
    // Economia de energie adauga sute de milisecunde la primirea unui apel RPC,
    // iar comutatorul din tabloul de bord ar parea ca nu reactioneaza.
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid, password);
}

bool isWifiConnected()
{
    return WiFi.status() == WL_CONNECTED;
}

uint16_t wifiConnectCount()
{
    return addressCount;
}
