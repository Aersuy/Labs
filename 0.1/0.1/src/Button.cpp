#include "Button.h"
//Constructor for button
//Assigns initial states for variable
Button::Button(int pin)
{
  this->pin = pin;
  this->state = HIGH;
  this->lastState = HIGH;
  this->pressed = false;
}
//Initialises pin 
void Button::begin()
{
  pinMode(this->pin,INPUT_PULLUP);
}

//Updates the state of the class based on physical change
void Button::update()
{
  this->lastState = this->state;
  this->state = digitalRead(this->pin);
  this->pressed = (this->lastState == HIGH && this->state == LOW);
}
//Returns if the button was pressed
//WARNING
//Use only after .update() otherwise doesn't work
//Example:
//button.update();
//if(button.wasPressed())
//doSomething()
bool Button::wasPressed()
{
  return this->pressed;
}