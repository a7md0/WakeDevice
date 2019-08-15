/**
 * Wake Device
 * Copyright (C) 2019 Ahmed Al-Qaidom

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "settings.h"

#ifdef ENABLE_LED
#include <Arduino.h>
#include <Ticker.h>

bool currentState = false;
bool initialized = false;

uint8_t ledPin = LED_BUILTIN;

#ifdef BLINK_LED
Ticker ledTicker;
bool blinkState = false;
float onDuration = BLINK_ON, offDuration = BLINK_OFF;
#endif

void setupLED(uint8_t _ledPin);
bool isLEDRunning();

void ledOn(float duration = 0);
void ledOff(float duration = 0);
void ledFlip();

void ledTurn(bool newState);
void ledTurn(bool newState, float duration);

#ifdef BLINK_LED
void ledBlink(bool newState);
void ledBlinkDuration(float onTime = 1, float offTime = 1);


void startBlinking();
void stopBlinking();
#endif

void setupLED(uint8_t _ledPin) {
	pinMode(ledPin, OUTPUT);

	ledPin = _ledPin;
	initialized = true;
	currentState = (bool)digitalRead(ledPin);
}

bool isLEDRunning() {
	return currentState;
}

void ledOn(float duration) {
	if (duration == 0)
		ledTurn(true);
	else
		ledTurn(true, duration);
}

void ledOff(float duration) {
	if (duration == 0)
		ledTurn(false);
	else
		ledTurn(false, duration);
}

void ledFlip() {
	ledTurn(!currentState);
}


void ledTurn(bool newState) {
	if (!initialized)
		return;

	currentState = newState;

	digitalWrite(ledPin, (newState ? HIGH : LOW));
}

void ledTurn(bool newState, float duration) {
	if (!initialized)
		return;

	ledTurn(newState);
	ledTicker.once(duration, ledFlip);
}

#ifdef BLINK_LED
void ledBlink(bool newState) {
	blinkState = newState;

	ledOff();
	if (newState)
		startBlinking();
	else
		ledOff();
}

void ledBlinkDuration(float onTime, float offTime) {
	onDuration = onTime, offDuration = offTime;
}

void startBlinking() {
	if (!blinkState)
		return;
	ledTurn(true);
	ledTicker.once(onDuration, stopBlinking);
}

void stopBlinking() {
	if (!blinkState)
		return;
	ledTurn(false);
	ledTicker.once(offDuration, startBlinking);
}
#endif
#endif