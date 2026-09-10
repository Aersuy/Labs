#include "lcdClass.h"
//Initialising the static variables that are needed
LiquidCrystal* Lcd::lcdDriver = nullptr;
FILE* Lcd::fileStream = nullptr;
int Lcd::currentRow = 0;
int Lcd::currentCol = 0;

//LCD class construction, creates the LiquidCrystal object
Lcd::Lcd(int rs,int enable,int d4, int d5, int d6, int d7)
{
  Lcd::lcdDriver = new LiquidCrystal(rs,enable,d4,d5,d6,d7);

}
//starts up the lcd driver and redirects stdout to writechar
void Lcd::begin()
{
  Lcd::fileStream = new FILE;
  Lcd::lcdDriver->begin(16,2);
  fdev_setup_stream(Lcd::fileStream,Lcd::writeChar,NULL,_FDEV_SETUP_WRITE);
  stdout = Lcd::fileStream;
}
//function for writing text onto LED
//For it to work, calling .begin() is mandatory as we need to reroute stdout
//Manages the location of the cursor
int Lcd::writeChar(char c, FILE *stream)
{
  if(c != '\n')
  {
    if(Lcd::currentCol == 15)
    {
      Lcd::currentRow += 1;
      Lcd::currentRow = Lcd::currentRow % 2;
      Lcd::currentCol = 0;
      Lcd::lcdDriver->setCursor(Lcd::currentCol,Lcd::currentRow);
    }
    Lcd::lcdDriver->write(c);
    Lcd::currentCol++;
  }
  if(c == '\n')
  {
    Lcd::currentRow += 1;
    Lcd::currentRow = Lcd::currentRow % 2;
    Lcd::currentCol = 0;
    Lcd::lcdDriver->setCursor(Lcd::currentCol,Lcd::currentRow);
  }
  return 0;
}
//Clears the screen
//Resets the cursor
void Lcd::clearScreen()
{
  Lcd::lcdDriver->clear();
  Lcd::currentCol = 0;
  Lcd::currentRow = 0;
}
//Set the cursor to an arbitrary position
void Lcd::setCursorPosition(int col, int row)
{
  Lcd::lcdDriver->setCursor(col,row);
  Lcd::currentCol = col;
  Lcd::currentRow = row;
}