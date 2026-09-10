#ifndef LCDCLASS_H
#define LCDCLASS_H

#include <Arduino.h>
#include <LiquidCrystal.h>
#include <stdio.h>


class Lcd {
  public:
    Lcd(int rs,int enable,int d4, int d5, int d6, int d7);
    void begin();
    void clearScreen();
    static int writeChar(char c, FILE* stream);
    void setCursorPosition(int col,int row);
  private:
    static LiquidCrystal* lcdDriver;
    static FILE* fileStream;
    static int currentRow;
    static int currentCol;
    static constexpr int screenLength = 16;
    static constexpr int screenHeight = 2;


};

#endif