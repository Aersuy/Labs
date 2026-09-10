#include <Arduino.h>
#include "ledClass.h"
#include "keypadClass.h"
#include "lcdClass.h"
Led ledGreen(13);
Led ledRed(12);
byte rowPins[4] = {A0, A1, A2, A3};
byte colPins[4] = {11, 10, 9, 8};
Keypad4x4 keypad(rowPins,colPins);
Lcd lcd(2,3,4,5,6,7);
int codeIndex = 0;
char enteredCode[5];
char code[5] = "1234";
void setup() {
  lcd.begin();
  printf("Introdu codul:\n");
}

void loop() {

  char key = keypad.getKey();
  if(key)
  {
    if(key == '#' || strlen(enteredCode) == 4)
    { 
      enteredCode[codeIndex] = '\0';
      if(strcmp(enteredCode,code) == 0)
      {
        ledGreen.turnOn();
        lcd.clearScreen();
        printf("Cod Valid");
        delay(10000);
      } else
      {
        ledRed.turnOn();
        lcd.clearScreen();
        printf("Cod Invalid");
        delay(10000);
      }
      ledRed.turnOff();
      ledGreen.turnOff();
      lcd.clearScreen();
      codeIndex = 0;
      printf("Introdu codul \n");
      
    } else 
    {
      printf("%c", key);
      enteredCode[codeIndex] = key;
      codeIndex++;
    }
  }

}
