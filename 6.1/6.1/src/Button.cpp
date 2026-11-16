#include "Button.h"
//Constructor for button
//Assigns initial states for variable
Button::Button(int pin, unsigned long debounceDelay)
{
  this->pin = pin;
  this->state = HIGH;
  this->lastState = HIGH;
  this->pressed = false;
  this->rawState = HIGH;
  this->debounceDelay = debounceDelay;
}
//Initialises pin 
void Button::begin()
{
  pinMode(this->pin,INPUT_PULLUP);
}

//Updates the state of the class based on physical change
void Button::update()
{

  bool reading = digitalRead(this->pin);
  if (reading != this->rawState)
  {
    this->lastDebounceTime = millis();
    this->rawState = reading;
  }

  this->pressed = false;

  if ((millis() - this->lastDebounceTime) >= this->debounceDelay)
  {
    if (reading != this->state)
    {
        this->lastState = this->state;
        this->state = reading;
        this->pressed = (this->lastState == HIGH && this->state == LOW);
        return;
    }
  }
}
//Returns if the button was pressed
//WARNING
//Use only after .update() otherwise doesn't work
//Example:
//button.update();
//if(button.wasPressed())
//doSomething()
bool Button::wasPressed() const
{
  return this->pressed;
}