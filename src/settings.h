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

#ifndef SETTINGS_h
#define SETTINGS_h

#define PRINT_TO_SERIAL

#define ENABLE_LED
#define LED_BUILTIN 2

#define BLINK_LED
#define BLINK_ON 1
#define BLINK_OFF 15

#define REPEAT_MAGIC_PACKET 3  //At least 1
#define REPEAT_MAGIC_PACKET_DELAY_MS 100

#define RETRY_CONN_AWS_SEC 5

#define UPDATE_FREQUENT 900000 * 6

#define SCHEDULE_RESTART // comment to disable scheduled restart
#define SCHEDULE_RESTART_MILLIS 86400 * 7 // 7 days

#define PING_RETRY_NUM 12 // try ping 12 times with delay of PING_BETWEEN_DELAY_MS between ( == 2m )
#define PING_BETWEEN_DELAY_MS 10000 // 10000MS (10 Seconds)

#define FAILED_DELAY_MS 5000
#define LONG_DELAY_MS 3600000 // 3600000 = 1H

#define TIME_ZONE 3 // +3 GMT
#define DST 0 // 1 To enable Daily save Time
#define NTP_TIMEOUT_MS 30000 // 30000 = 30S

#define gmtOffsetSec TIME_ZONE * 3600
#define daylightOffsetSec DST * 3600

#define NTP_SERV1 "0.asia.pool.ntp.org"
#define NTP_SERV2 "1.asia.pool.ntp.org"
#define NTP_SERV3 "0.europe.pool.ntp.org"

#if defined(PRINT_TO_SERIAL)
#define Sprintln(a) (Serial.println(a))
#define Sprint(a) (Serial.print(a))
#define Sprintf(a, b) (Serial.printf(a, b))
#define Sjson(a, b) (serializeJson(a, b))
#else
#define Sprintln(a)
#define Sprint(a)
#define Sprintf(a, b)
#define Sjson(a, b)
#endif

#endif