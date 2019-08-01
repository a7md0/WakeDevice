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

			if (millis() >= nextShadowMillis)
				sendShadowData();

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
	xTaskCreate(ntpTask, "NTP_TASK", 2048, NULL, tskIDLE_PRIORITY, NULL);

#if defined(SCHEDULE_RESTART)
	xTaskCreate(restartTask, "RESTART_TASK", 2048, NULL, tskIDLE_PRIORITY, NULL);
#endif
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
	IPAddress gatewayIP = WiFi.gatewayIP();
	IPAddress broadcastIP = WOL.calculateBroadcastAddress(localIP, subnetMask);

	Sprint("IP: " + localIP.toString());
	Sprint(" | Subnet: " + subnetMask.toString());
	Sprint(" | Gateway: " + gatewayIP.toString());
	Sprintln(" | Broadcast: " + broadcastIP.toString());
#else
	WOL.calculateBroadcastAddress(WiFi.localIP(), WiFi.subnetMask());
#endif

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
					deviceStruct *device = new deviceStruct;

					device->mac = obj["MAC"].as<String>();

					if (obj.containsKey("port"))
						device->port = obj["port"].as<uint16_t>();

					if (obj.containsKey("retrieveStatus") && obj.containsKey("channel") && obj.containsKey("ip")) {
						device->retrieveStatus = obj["retrieveStatus"].as<bool>();
						device->channel = obj["channel"].as<String>();
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
				if (obj.containsKey("channel") && obj.containsKey("device")) {
					statusStruct *status = new statusStruct;

					status->channel = obj["channel"].as<String>();
					status->mac = obj["device"]["MAC"].as<String>();
					status->ip = obj["device"]["IP"].as<String>();

					xTaskCreatePinnedToCore(deviceStatusTask, "deviceStatusTask", 2048, (void *)status, 6, NULL, 1);
				}
			} break;
			default:
				break;
		}
	}
}

void mqttMessageQueueProcess() {
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
}

void sendShadowData(void) {
	DynamicJsonDocument jsonBuffer(JSON_OBJECT_SIZE(6) + 100);

	JsonObject root = jsonBuffer.to<JsonObject>();
	JsonObject state = root.createNestedObject("state");
	JsonObject state_reported = state.createNestedObject("reported");

	IPAddress localIP = WiFi.localIP();
	IPAddress subnetMask = WiFi.subnetMask();

	state_reported["network_id"] = String(getNetworkID(localIP, subnetMask).toString() + "/" + subnetCIDR(subnetMask));
	state_reported["free_memory"] = esp_get_free_heap_size();
	state_reported["up_time"] = millis() / 1000;

	Sprintf("[%s] Sending: ", MQTT_PUB_SHADOW);
	Sjson(root, Serial);
	Sprintln();

	char shadow[measureJson(root) + 1];
	serializeJson(root, shadow, sizeof(shadow));

	if (!client.publish(MQTT_PUB_SHADOW, shadow, sizeof(shadow))) {
		lwMQTTErr(client.lastError());
		nextShadowMillis = millis() + 10000;
	} else
		nextShadowMillis = millis() + UPDATE_FREQUENT;
}

void wakeDeviceTask(void *pvParameters) {
	deviceStruct *device = (deviceStruct *)pvParameters;

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
		bool pingResult = false;
		deviceIP.fromString(device->ip.c_str());

		for (uint8_t i = 0; i < PING_RETRY_NUM; i++) {
			Sprint("> ping ");
			Sprintln(deviceIP.toString());

			pingResult = Ping.ping(deviceIP);

			Sprintf(">> %d\n", pingResult);

			if (pingResult == true || i == PING_RETRY_NUM - 1)
				break;

			vTaskDelay(pdMS_TO_TICKS(PING_BETWEEN_DELAY_MS));
		}

		addDeviceStatus(device->mac, device->channel, pingResult);
	}

	delete pvParameters;
	vTaskDelete(NULL);
}

void deviceStatusTask(void *pvParameters) {
	statusStruct *status = (statusStruct *)pvParameters;
	IPAddress deviceIP;

	deviceIP.fromString(status->ip.c_str());

	Sprint("> ping ");
	Sprintln(deviceIP.toString());

	bool pingResult = Ping.ping(deviceIP);

	Sprintf(">> %d\n", pingResult);

	addDeviceStatus(status->mac, status->channel, pingResult);

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

void addDeviceStatus(String &mac, String &topic, bool status) {
	bool addedToQueue = false;

	for (uint8_t i = 0; i < mqttMessagesQueueSize; i++) {
		if (mqttMessagesQueue[i].waiting == false) {
			DynamicJsonDocument jsonBuffer(JSON_OBJECT_SIZE(4) + 100);

			JsonObject rootJSON = jsonBuffer.to<JsonObject>();
			rootJSON["MAC"] = mac.c_str();
			rootJSON["pingResult"] = status;

			char data[measureJson(rootJSON) + 1];
			serializeJson(rootJSON, data, sizeof(data));

			Sjson(rootJSON, Serial);

			mqttMessagesQueue[i].waiting = true;
			mqttMessagesQueue[i].topic = topic;
			mqttMessagesQueue[i].payload = data;
			mqttMessagesQueue[i].nextTry = 0;

			addedToQueue = true;

			break;
		}
	}

	if (!addedToQueue) {
		vTaskDelay(pdMS_TO_TICKS(FAILED_DELAY_MS));
		addDeviceStatus(mac, topic, status);
		Sprintln("!addedToQueue");
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
