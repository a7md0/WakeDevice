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

#include "main.h"

void setup() {
#ifdef PRINT_TO_SERIAL
	Serial.begin(MONITOR_SPEED);  // 115200
	Sprintln("Booting\n");
#endif

#ifdef ENABLE_LED
	setupLED(LED_BUILTIN);
	ledOn();
#ifdef BLINK_LED
	ledBlinkDuration(BLINK_ON, BLINK_OFF);
#endif
#endif

	wifiConnect();

	net.setCACert(caCert);
	net.setCertificate(clientCert);
	net.setPrivateKey(privKey);

	client.begin(AWS_HOST, AWS_PORT, net);
	client.onMessage(messageReceived);

	WOL.setRepeat(REPEAT_MAGIC_PACKET, REPEAT_MAGIC_PACKET_DELAY_MS);

	setupTasks();
}

void loop() {
	if (WiFi.isConnected()) {
		if (client.connected()) {
			client.loop();

			mqttMessageQueueProcess();
		} else {
#ifdef ENABLE_LED
#ifdef BLINK_LED
			ledBlink(false);
#endif
			ledOn();

#endif
			if (time(nullptr) > BUILD_TIMESTAMP && timeSet == true)
				connectToAWS();
		}
	}
}

void setupTasks() {
	mqttQueueSemaphore = xSemaphoreCreateMutex();
	icmpQueueSemaphore = xSemaphoreCreateMutex();

	xTaskCreate(ntpTask, "NTP_TASK", 2048, NULL, tskIDLE_PRIORITY, NULL);

#if defined(SCHEDULE_RESTART)
	xTaskCreate(restartTask, "RESTART_TASK", 2048, NULL, tskIDLE_PRIORITY, NULL);
#endif

	xTaskCreatePinnedToCore(icmpTask, "ICMP_TASK", 4096, NULL, 6, &icmpTaskHandler, 1);
	vTaskSuspend(icmpTaskHandler);
}

void wifiConnect() {
	Sprintf("Connecting to %s\n", WIFI_SSID);

	WiFi.setHostname(THING_NAME);

	WiFi.mode(WIFI_STA);
	WiFi.setAutoReconnect(true);

	WiFi.onEvent(wifiConnected, SYSTEM_EVENT_STA_CONNECTED);
	WiFi.onEvent(wifiAcquiredIP, SYSTEM_EVENT_STA_GOT_IP);
	WiFi.onEvent(wifiDisconnected, SYSTEM_EVENT_STA_DISCONNECTED);

#ifndef WIFI_USE_DHCP
	IPAddress ip;
	IPAddress gateway;
	IPAddress subnet;
	IPAddress primaryDNS;
	IPAddress secondaryDNS;

	ip.fromString(STATIC_IP);
	gateway.fromString(STATIC_DEFAULT_GATEWAY);
	subnet.fromString(STATIC_SUBNET_MASK);
	primaryDNS.fromString(STATIC_PRIMARY_DNS);
	secondaryDNS.fromString(STATIC_SECONDARY_DNS);

	WiFi.config(ip, gateway, subnet, primaryDNS, secondaryDNS);
#endif
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void wifiConnected(system_event_id_t event) {
	Sprint("SSID: " + String(WiFi.SSID()));
	Sprint(" | Channel: " + String(WiFi.channel()));
	Sprintln(" | RSSI: " + String(WiFi.RSSI()));
}

void wifiAcquiredIP(system_event_id_t event) {
#ifdef PRINT_TO_SERIAL
	IPAddress localIP = WiFi.localIP();
	IPAddress subnetMask = WiFi.subnetMask();

	IPAddress networkID = getNetworkID(localIP, subnetMask);
	uint8_t cidr = subnetCIDR(subnetMask);

	Sprint("Network: " + networkID.toString());
	Sprint("/" + cidr);
	Sprintln(" | IPv4: " + localIP.toString());
#endif

	WOL.calculateBroadcastAddress(WiFi.localIP(), WiFi.subnetMask());
	updateSystemTime();
}

void wifiDisconnected(system_event_id_t event) {
#ifdef ENABLE_LED
#ifdef BLINK_LED
	ledBlink(false);
#endif
	ledOn();
#endif

	Sprintln("STA disconnect detected");
	WiFi.reconnect();
}

void updateSystemTime() {
	struct tm timeinfo;

	if (!WiFi.isConnected())
		return;

	Sprintln("Setting time using NTP");
	configTime(gmtOffsetSec, daylightOffsetSec, NTP_SERV1, NTP_SERV2, NTP_SERV3);

	if (getLocalTime(&timeinfo, NTP_TIMEOUT_MS)) {
		Sprint("Current time: ");
		Sprintln(asctime(&timeinfo));

		timeSet = true;
	} else
		updateSystemTime();
}

void connectToAWS() {
	Sprint("AWS connecting ");
	while (!client.connected()) {
		if (client.connect(THING_NAME)) {
			Sprintln("connected!");

			if (!client.subscribe(AWS_WAKE_CHANNEL))
				lwMQTTErr(client.lastError());
#ifdef ENABLE_LED
			else {
				ledOff();
#ifdef BLINK_LED
				ledBlink(true);
#endif
			}
#endif

		} else {
			Sprint("failed, reason -> ");
			lwMQTTErrConnection(client.returnCode());

			Sprint(" < try again in ");
			Sprint(RETRY_CONN_AWS_SEC);
			Sprintln(" seconds");

			vTaskDelay(pdMS_TO_TICKS(RETRY_CONN_AWS_SEC * 1000));
		}
	}
}

void messageReceived(String &topic, String &payload) {
	Sprintln("Recieved [" + topic + "]: " + payload);

	if (strcmp(topic.c_str(), AWS_WAKE_CHANNEL) == 0) {
		DynamicJsonDocument doc(2048);
		DeserializationError error = deserializeJson(doc, payload.c_str(), 2048);
		JsonObject obj = doc.as<JsonObject>();

		if (error) {
			Sprint("deserializeJson() failed: ");
			Sprintln(error.c_str());
			return;
		}

		if (!obj.containsKey("id")) {
			Sprint("Failed: no msg id");
			return;
		}
		const int msgID = obj["id"].as<int>();

		switch (msgID) {
			case 1: {
				if (obj.containsKey("MAC")) {
					wakeMessageStruct *device = new wakeMessageStruct;

					device->mac = obj["MAC"].as<String>();

					if (obj.containsKey("port"))
						device->port = obj["port"].as<uint16_t>();

					if (obj.containsKey("retrieveStatus") && obj.containsKey("topic") && obj.containsKey("ip")) {
						device->retrieveStatus = obj["retrieveStatus"].as<bool>();
						device->topic = obj["topic"].as<String>();
						device->ip = obj["ip"].as<String>();
					}

					if (obj.containsKey("secureOn") && obj.containsKey("secureOnPassword")) {
						device->secureOn = obj["secureOn"].as<bool>();
						device->secureOnPassword = obj["secureOnPassword"].as<String>();
					}

					xTaskCreatePinnedToCore(wakeDeviceTask, "wakeDeviceTask", 2048, (void *)device, 5, NULL, 1);
				}
			} break;
			case 2: {
				if (obj.containsKey("topic") && obj.containsKey("device")) {
					statusMessageStruct *status = new statusMessageStruct;

					status->topic = obj["topic"].as<String>();
					status->mac = obj["device"]["MAC"].as<String>();
					status->ip = obj["device"]["IP"].as<String>();

					xTaskCreatePinnedToCore(deviceStatusTask, "deviceStatusTask", 2048, (void *)status, 4, NULL, 1);
				}
			} break;
			default:
				break;
		}
	}
}

void mqttMessageQueueProcess() {
	if (xSemaphoreTake(mqttQueueSemaphore, pdMS_TO_TICKS(10)) == pdFALSE)
		return;

	if (mqttMessagesQueueIndex == mqttMessagesQueueSize)
		mqttMessagesQueueIndex = 0;

	if (mqttMessagesQueue[mqttMessagesQueueIndex].waiting == true && millis() >= mqttMessagesQueue[mqttMessagesQueueIndex].nextTry) {
		String data = mqttMessagesQueue[mqttMessagesQueueIndex].payload;

		Sprintf("[%s] Sending: ", mqttMessagesQueue[mqttMessagesQueueIndex].topic.c_str());
		Sprintln(data.c_str());
		Sprintln();

		bool res = client.publish(mqttMessagesQueue[mqttMessagesQueueIndex].topic, data);
		if (res)
			mqttMessagesQueue[mqttMessagesQueueIndex].waiting = false;
		else {
			mqttMessagesQueue[mqttMessagesQueueIndex].nextTry = millis() + FAILED_DELAY_MS;
			lwMQTTErr(client.lastError());
		}

		mqttMessagesQueueIndex++;
	}

	xSemaphoreGive(mqttQueueSemaphore);
}

void wakeDeviceTask(void *pvParameters) {
	wakeMessageStruct *device = (wakeMessageStruct *)pvParameters;

	bool status;
	IPAddress deviceIP;

	if (device->secureOn == false) {
		Sprintf("WOL -> %s => ", device->mac.c_str());
		status = WOL.sendMagicPacket(device->mac, device->port);
	} else {
		Sprintf("Secure WOL -> %s -> ", device->mac.c_str());
		Sprintf("%s => ", device->secureOnPassword.c_str());
		status = WOL.sendSecureMagicPacket(device->mac, device->secureOnPassword, device->port);
	}

	Sprintln(status);

	if (device->retrieveStatus == true) {
		deviceIP.fromString(device->ip.c_str());

		icmpRequstAdd(device->mac, deviceIP, device->topic, PING_RETRY_NUM);
	}

	delete pvParameters;
	vTaskDelete(NULL);
}

void deviceStatusTask(void *pvParameters) {
	statusMessageStruct *status = (statusMessageStruct *)pvParameters;
	IPAddress deviceIP;

	deviceIP.fromString(status->ip.c_str());
	icmpRequstAdd(status->mac, deviceIP, status->topic, 1);

	delete pvParameters;
	vTaskDelete(NULL);
}

void ntpTask(void *pvParameters) {
	struct tm timeinfo;

	for (;;) {
		if (!WiFi.isConnected()) {
			vTaskDelay(pdMS_TO_TICKS(FAILED_DELAY_MS));  // 30000 = 30S
			continue;
		}

		vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));  // 3600000 = 1H

		Sprintln("Updating time using NTP");
		getLocalTime(&timeinfo, 0);
	}
}

#if defined(SCHEDULE_RESTART)
void restartTask(void *pvParameters) {
	for (;;) {
		vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));

		if (millis() >= ((unsigned long)SCHEDULE_RESTART_MILLIS * 1000))  // SCHEDULE_RESTART_MILLIS
			break;
	}

	prepareRestart();
	vTaskDelete(NULL);
}
#endif

void icmpTask(void *pvParameters) {
	for (;;) {
		bool queueIsEmpty = true;

		if (!WiFi.isConnected()) {
			vTaskDelay(pdMS_TO_TICKS(FAILED_DELAY_MS));
			continue;
		}

		if (xSemaphoreTake(icmpQueueSemaphore, pdMS_TO_TICKS(1000)) == pdTRUE) {
			for (uint8_t i = 0; i < icmpQueueSize; i++) {
				if (icmpQueue[i].waiting == true && millis() >= icmpQueue[i].nextICMP) {
					Sprint("> ping ");
					Sprintln(icmpQueue[i].ip.toString());

					bool pingResult = Ping.ping(icmpQueue[i].ip);

					Sprintf(">> %d\n", pingResult);

					icmpQueue[i].tries--;
					icmpQueue[i].nextICMP = millis() + PING_BETWEEN_DELAY_MS;

					Sprintf("tries: %d\n", icmpQueue[i].tries);

					if (pingResult == true || icmpQueue[i].tries == 0) {
						icmpQueue[i].waiting = false;
						addDeviceStatus(icmpQueue[i].mac, icmpQueue[i].topic, pingResult);
					}
				}

				if (icmpQueue[i].waiting == true)
					queueIsEmpty = false;
			}

			xSemaphoreGive(icmpQueueSemaphore);
		}

		if (queueIsEmpty == true)
			vTaskSuspend(NULL);
		else
			vTaskDelay(pdMS_TO_TICKS(10));
	}
}

void icmpRequstAdd(String &mac, IPAddress ip, String &topic, uint8_t maxTries) {
	bool addedToQueue = false;

	if (xSemaphoreTake(icmpQueueSemaphore, pdMS_TO_TICKS(1000)) == pdTRUE) {
		for (uint8_t i = 0; i < icmpQueueSize; i++) {
			if (icmpQueue[i].waiting == true && icmpQueue[i].ip == ip) {
				addedToQueue = true;
				break;
			}

			if (icmpQueue[i].waiting == false) {
				icmpQueue[i].waiting = true;

				icmpQueue[i].mac = mac;
				icmpQueue[i].ip = ip;

				icmpQueue[i].topic = topic;

				icmpQueue[i].tries = maxTries;

				addedToQueue = true;
				break;
			}
		}

		xSemaphoreGive(icmpQueueSemaphore);
	}

	if (addedToQueue)
		vTaskResume(icmpTaskHandler);
	else {
		vTaskDelay(pdMS_TO_TICKS(FAILED_DELAY_MS));
		icmpRequstAdd(mac, ip, topic, maxTries);
	}
}

void addDeviceStatus(String &mac, String &topic, bool status) {
	bool addedToQueue = false;

	if (xSemaphoreTake(mqttQueueSemaphore, pdMS_TO_TICKS(1000)) == pdTRUE) {
		for (uint8_t i = 0; i < mqttMessagesQueueSize; i++) {
			if (mqttMessagesQueue[i].waiting == false) {
				DynamicJsonDocument jsonBuffer(JSON_OBJECT_SIZE(4) + 100);

				JsonObject rootJSON = jsonBuffer.to<JsonObject>();
				rootJSON["MAC"] = mac.c_str();
				rootJSON["pingResult"] = status;

				char data[measureJson(rootJSON) + 1];
				serializeJson(rootJSON, data, sizeof(data));

				mqttMessagesQueue[i].waiting = true;

				mqttMessagesQueue[i].topic = topic;
				mqttMessagesQueue[i].payload = data;

				mqttMessagesQueue[i].nextTry = 0;

				addedToQueue = true;
				break;
			}
		}

		xSemaphoreGive(mqttQueueSemaphore);
	}

	if (!addedToQueue) {
		vTaskDelay(pdMS_TO_TICKS(FAILED_DELAY_MS));
		addDeviceStatus(mac, topic, status);
	}
}

void prepareRestart() {
	Sprint("prepareRestart()");

	if (client.connected()) {
		client.unsubscribe(AWS_WAKE_CHANNEL);
		client.disconnect();
	}
	if (WiFi.isConnected())
		WiFi.disconnect();

	esp_restart();
}

void lwMQTTErr(lwmqtt_err_t reason) {
	if (reason == lwmqtt_err_t::LWMQTT_SUCCESS)
		Sprint("Success");
	else if (reason == lwmqtt_err_t::LWMQTT_BUFFER_TOO_SHORT)
		Sprint("Buffer too short");
	else if (reason == lwmqtt_err_t::LWMQTT_VARNUM_OVERFLOW)
		Sprint("Varnum overflow");
	else if (reason == lwmqtt_err_t::LWMQTT_NETWORK_FAILED_CONNECT)
		Sprint("Network failed connect");
	else if (reason == lwmqtt_err_t::LWMQTT_NETWORK_TIMEOUT)
		Sprint("Network timeout");
	else if (reason == lwmqtt_err_t::LWMQTT_NETWORK_FAILED_READ)
		Sprint("Network failed read");
	else if (reason == lwmqtt_err_t::LWMQTT_NETWORK_FAILED_WRITE)
		Sprint("Network failed write");
	else if (reason == lwmqtt_err_t::LWMQTT_REMAINING_LENGTH_OVERFLOW)
		Sprint("Remaining length overflow");
	else if (reason == lwmqtt_err_t::LWMQTT_REMAINING_LENGTH_MISMATCH)
		Sprint("Remaining length mismatch");
	else if (reason == lwmqtt_err_t::LWMQTT_MISSING_OR_WRONG_PACKET)
		Sprint("Missing or wrong packet");
	else if (reason == lwmqtt_err_t::LWMQTT_CONNECTION_DENIED)
		Sprint("Connection denied");
	else if (reason == lwmqtt_err_t::LWMQTT_FAILED_SUBSCRIPTION)
		Sprint("Failed subscription");
	else if (reason == lwmqtt_err_t::LWMQTT_SUBACK_ARRAY_OVERFLOW)
		Sprint("Suback array overflow");
	else if (reason == lwmqtt_err_t::LWMQTT_PONG_TIMEOUT)
		Sprint("Pong timeout");
}

void lwMQTTErrConnection(lwmqtt_return_code_t reason) {
	if (reason == lwmqtt_return_code_t::LWMQTT_CONNECTION_ACCEPTED)
		Sprint("Connection Accepted");
	else if (reason == lwmqtt_return_code_t::LWMQTT_UNACCEPTABLE_PROTOCOL)
		Sprint("Unacceptable Protocol");
	else if (reason == lwmqtt_return_code_t::LWMQTT_IDENTIFIER_REJECTED)
		Sprint("Identifier Rejected");
	else if (reason == lwmqtt_return_code_t::LWMQTT_SERVER_UNAVAILABLE)
		Sprint("Server Unavailable");
	else if (reason == lwmqtt_return_code_t::LWMQTT_BAD_USERNAME_OR_PASSWORD)
		Sprint("Bad UserName/Password");
	else if (reason == lwmqtt_return_code_t::LWMQTT_NOT_AUTHORIZED)
		Sprint("Not Authorized");
	else if (reason == lwmqtt_return_code_t::LWMQTT_UNKNOWN_RETURN_CODE)
		Sprint("Unknown Return Code");
}

IPAddress getNetworkID(IPAddress ip, IPAddress subnet) {
	IPAddress networkID;

	for (size_t i = 0; i < 4; i++)
		networkID[i] = subnet[i] & ip[i];

	return networkID;
}

uint8_t subnetCIDR(IPAddress subnetMask) {
	uint8_t CIDR = 0;

	for (uint8_t i = 0; i < 4; i++) {
		if (subnetMask[i] == 0x80)		 // 128
			CIDR += 1;					 // 1 bit
		else if (subnetMask[i] == 0xC0)  // 192
			CIDR += 2;					 // 2 bit
		else if (subnetMask[i] == 0xE0)  // 224
			CIDR += 3;					 // 3 bit
		else if (subnetMask[i] == 0xF0)  // 242
			CIDR += 4;					 // 4 bit
		else if (subnetMask[i] == 0xF8)  // 248
			CIDR += 5;					 // 5 bit
		else if (subnetMask[i] == 0xFC)  // 252
			CIDR += 6;					 // 6 bit
		else if (subnetMask[i] == 0xFE)  // 254
			CIDR += 7;					 // 7 bit
		else if (subnetMask[i] == 0xFF)  // 255
			CIDR += 8;					 // 8 bit
	}

	return CIDR;
}
