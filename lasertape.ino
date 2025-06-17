#include <Arduino.h>

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define RXD_PIN			(0)
#define TXD_PIN			(1)
#define MODULE_UART		(1)
#define LASER_TIMEOUT_MS	(1500)

HardwareSerial muart(MODULE_UART);

int do_measure(uint32_t timeout_ms)
{
	String rxdata;		/* cheater way to concatenate data */
	uint32_t timeout;	/* millis() value of when to stop waiting */
	int distance;		/* measured distance, in mm (0 = no result) */

	/*
	 * clear out any existing serial data, set a timeout of 0 (nonblocking mode)
	 * and send the "take measurement" command
	 */
	muart.flush();
	muart.setTimeout(0);
	muart.print("$00022123&");

	/*
	 * collect as much serial data as we can get in the next two seconds
	 * any time there is no more data available, see if the module has
	 * sent us a measurement result so we can drop out earlier than the
	 * timeout. The module seems to take anywhere from 500-1500ms depending on
	 * the distance measured and whatever internal processing delays it has
	 */
	distance = 0;
	timeout = millis() + timeout_ms;
	while (millis() < timeout) {
		bool new_data;

		/* read as much data as we can without blocking and append it to the buffer */
		new_data = false;
		while (muart.available()) {
			rxdata += muart.readString();
			new_data = true;
		};

		/* if we pulled in any new data, try to find the measurement response */
		if (new_data) {
			const char *buf;	/* buffer of entire response from module */
			const char *mr;		/* pointer to the measurement response message */
			char *mv;		/* pointer to just the value in the response */

			/*
			 * I don't do Arduino very well. Just use C string/pointer manipulation.
			 * if the "measurement result" response is in the buffer, and there's
			 * enough characters in the buffer for the whole measurement result,
			 * then strip out the actual measurement and use it.
			 *
			 * *buf is the whole buffer (everything received up to this point).
			 * it will look something like this:
			 * $00023335&$0003263766&$00023335&$0006210000008714&
			 *
			 * I use strstr() to find the start of the result response ($000621)
			 * and if it's found, *mr will point to it.
			 *
			 * $000621xxxxxxxxyy&
			 *                  ^-- mr[17]    = end of packet
			 *                ^^--- mr[15-16] = checksum
			 *        ^^^^^^^^----- mr[7-14]  = measurement in mm
			 *  ^^^^^^------------- mr[1-6]   = measurement result header
			 * ^------------------- mr[0]     = start of packet
			 *
			 * I set *mv to the start of the value in the response, then cut off
			 * the checksum/footer and use atoi() to convert the ASCII character
			 * buffer to an actual integer value which is the distance in mm.
			 */
			buf = rxdata.c_str();
			if ((mr = strstr(buf, "$000621")) && strlen(mr) > 11) {
				mv = (char *)&mr[7];
				mv[8] = 0x00;

				/* we've got a measurement, drop out early */
				distance = atoi(mv);
				break;
			}
		}
	};	/* while(not timed out) */

	return distance;
}

void setup()
{
	Serial.begin(115200);
	muart.begin(115200, SERIAL_8N1, RXD_PIN, TXD_PIN);
}

void loop()
{
	int distance;

	if ((distance = do_measure(LASER_TIMEOUT_MS)) > 0) {
		Serial.printf("distance = %dmm\n", distance);
	}
}