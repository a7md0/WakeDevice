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

#ifndef MAIN_h
#define MAIN_h

#include <Arduino.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <Ticker.h>
#include <esp_system.h>

#include <ArduinoJson.h>
#include <MQTT.h>

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>

#include "Credentials.h"
#include "settings.h"

#include <ESP32Ping.h>
#include <WakeOnLan.h>

void setupTasks();

void wifiConnect();
void wifiConnected(system_event_id_t event);
void wifiAcquiredIP(system_event_id_t event);
void wifiDisconnected(system_event_id_t event);

void updateSystemTime();

void connectToAWS();
void messageReceived(String &topic, String &payload);
void mqttMessageQueueProcess();
void sendShadowData(void);

void wakeDeviceTask(void *pvParameters);
void deviceStatusTask(void *pvParameters);

void ntpTask(void *pvParameters);

#if defined(SCHEDULE_RESTART)
void restartTask(void *pvParameters);
#endif

void icmpTask(void *pvParameters) ;
void icmpRequstAdd(String &mac, IPAddress ip, String &topic, uint8_t maxTries);

void addDeviceStatus(String &mac, String &topic, bool status);

void prepareRestart();

void lwMQTTErr(lwmqtt_err_t reason);
void lwMQTTErrConnection(lwmqtt_return_code_t reason);

IPAddress getNetworkID(IPAddress ip, IPAddress subnet);
uint8_t subnetCIDR(IPAddress subnetMask);

#ifdef ENABLE_LED
extern void setupLED(uint8_t _ledPin);
extern bool isLEDRunning();

extern void ledOn(float duration = 0);
extern void ledOff(float duration = 0);
extern void ledFlip();

#ifdef BLINK_LED
extern void ledBlink(bool newState);
extern void ledBlinkDuration(float onTime = 1, float offTime = 1);
#endif
#endif

struct wakeMessageStruct {
	String mac;
	uint16_t port = 9;

	bool retrieveStatus = false;
	String topic;
	String ip;

	bool secureOn = false;
	String secureOnPassword;
};

struct statusMessageStruct {
	String mac;
	String topic;
	String ip;
};

struct icmpQueueStruct {
	bool waiting = false;

	String mac;
	IPAddress ip;

	String topic;

	int8_t tries = 1;

	unsigned long nextICMP = 0;
};

struct mqttMessageStruct {
	bool waiting = false;

	String topic;
	String payload;

	unsigned long nextTry = 0;
};

WiFiClientSecure net;
MQTTClient client(256);  // 128 // 256

WiFiUDP UDP;
WakeOnLan WOL(UDP);

bool timeSet = false;
unsigned long nextShadowMillis = 0;

const size_t mqttMessagesQueueSize = 12;
uint8_t mqttMessagesQueueIndex = 0;
mqttMessageStruct mqttMessagesQueue[mqttMessagesQueueSize];

SemaphoreHandle_t mqttQueueSemaphore = NULL;

const size_t icmpQueueSize = 24;
icmpQueueStruct icmpQueue[icmpQueueSize];

TaskHandle_t icmpTaskHandler = NULL;
SemaphoreHandle_t icmpQueueSemaphore = NULL;

#endif
