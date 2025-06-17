# reverse engineering a laser measuring tape module

From [Amazon](https://www.amazon.ca/gp/product/B0DN5XTZYH), $20:

<img src="images/bilipala_device.png" alt="bilipala_device" style="zoom:33%;" />

Contains a PCB with some ARM processor (guessing by the labelled SWDCK/SWDIO pads) and a separate laser measurement module.

## Internal Photos

![internal_mcu](images/internal_mcu.jpeg)

<img src="images/laser_top.jpeg" alt="laser_top" style="zoom:33%;" /><img src="images/laser_bottom.jpeg" alt="laser_bottom" style="zoom:33%;" />

<img src="images/laser_bottom2.jpeg" alt="laser_bottom2" style="zoom:33%;" />

## Module Connector

This looks like a standard four pin JST-?? connector. I'm designating the pin closest to the edge of the board as pin 1:

| Pin  | Description               |
| ---- | ------------------------- |
| 1    | 3.3V (main PCB says 3.6V) |
| 2    | Ground                    |
| 3    | TXD (Module -> MCU)       |
| 4    | RXD (MCU -> Module)       |

Serial parameters are 115200,N81. 

## Communication Captures

For all of these, the top line is module -> MCU, bottom line is MCU -> module. The communication is split up into multiple images partly for clarity, but also because there are long (500 - 5000ms) gaps between the MCU and module traffic.

### 0.239m

![0.239-1](images/0.239-1.png)

![0.239-2](images/0.239-2.png)

![0.239-3](images/0.239-3.png)

![0.239-4](images/0.239-4.png)

Decode: `$0003260130&` `$00023335&$0003261948&` `$00022123&` `$00023335&` `$0006210000011442&` 

Interesting that that second image seems to show two back-to-back responses from the module?

### 2.809m

![2.809-1](images/2.809-1.png)

![2.809-2](images/2.809-2.png)

Decode:`$00022123&` `$00023335&` `$0006210000268942&`

## laser on?

![laseron-1](images/laseron-1.png)

![laseron-2](images/laseron-2.png)

Decode: `$0003260130&` `$00023335&$0003262756&`

The second pic here also has two back-to-back responses.

### 1.919m

![1.919-1](images/1.919-1.png)

![1.919-2](images/1.919-2.png)

Decode: `$00022123&` `$00023335&` `$0006210000191965&`

I can actually see "1919" in that second image at the end.

### 1.3m

![1.3-1](images/1.3-1.png)

![1.3-2](images/1.3-2.png)

![1.3-3](images/1.3-3.png)

![1.3-4](images/1.3-4.png)

Decode: `$0003260130&` `$00023335&$0003264069&` `$00022123&` `$00023335&` `$0006210000130040&`

 I see 1.3 there in the last bit

### err500

![err500-1](images/err500-1.png)

![err500-2](images/err500-2.png)

![err500-3](images/err500-3.png)

![er500-4](images/er500-4.png)

Decode: `$00022123&` `$00023335&` `$00022123&` `$0003260029&` `$0006210000001643&`

### err255

![err255-1](images/err255-1.png)

![err255-2](images/err255-2.png)

![err255-3](images/err255-3.png)

Decode: `$0003260130&` `$000233356&$0003269625&` `$00022123&` `$00023335&`

### err261

![err261-1](images/err261-1.png)

![err261-2](images/err261-2.png)

![err261-3](images/err261-3.png)

Decode: `$0003260130&` `$00023335&$0003263059&` `$00022123&` `$00023335&`

### Unit is on, I briefly press the power button:

![powerbutton-1](images/powerbutton-1.png)

![powerbutton-2](images/powerbutton-2.png)

(do it again)

![powerbutton2-1](images/powerbutton2-1.png)

![powerbutton2-2](images/powerbutton2-2.png)

Decode (both are the same): `$0003260029&` `$000233356&$0003269625&`

(consistent command and response, not sure what its purpose is)

### 0.793m

![0.793-1](images/0.793-1.png)

## Common messages

`$0003260130&` - I think this turns the sighting laser on?

`$00022123&` - this happens right before an immediate response, I think this is "take measurement"

`$00023335&` - this appears to be an acknowledgement from the module?

`$000621xxxxxxxxyy&` - `xxxxxxxx` is the measurement value in 0.001m steps, `yy` is a checksum

## Conclusions

1. all commands and responses start with `$` and end with `&`
2. The last two digits in each message form a checksum.
3. The measurement time seems to vary from about a half second to a few seconds. I'm guessing that this depends on the actual distance, optical conditions of the environment as well as whatever internal processing the module does.
4. All unit conversion and fancy modes of operation are done by the MCU, not the module, which is a great thing.

Some of the captures do not have a response value which equals the distance -- I didn't notice that the unit would default to measuring from the bottom of the case instead of the top (where the "business end" of the module is). Once I made sure the unit was set to measure from the top, the values the module provided over the UART always matched what was shown on the screen.

### Checksum algorithm

This is one of the most ridiculous checksums I've run across. I figured it out by eyeballing the messages and a couple educated guesses:

1. Pair up the digits in the message to form values that each range from 0-99
2. Sum all these values
3. Take the answer and wrap it at 100

The last two digits of the message should equal the checksum.

e.g.

* `$00023335&`: 00 + 02 + 33 = 35, which is what the same as the last two digits of the message
* `$0003260130&`: 00 + 03 + 26 + 01 = 30
* `$0006210000001643&`: 00 + 06 + 21 + 00 + 00 + 00 + 16 = 43
* `$0006210000008815&`: 00 + 06 + 21 + 00 + 00 + 00 + 88 = 115, % 100 is 15

## Example Code

I have this working with just a basic Arduino sketch:

```c
#define RXD_PIN				(0)
#define TXD_PIN				(1)
#define MODULE_UART			(1)
#define LASER_TIMEOUT_MS	(1500)

HardwareSerial muart(MODULE_UART);

int do_measure(uint32_t timeout_ms)
{
	String rxdata;					/* cheater way to concatenate data */
    uint32_t timeout;				/* millis() value of when to stop waiting */
	int distance;					/* measured distance, in mm (0 = no result) */

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
            char *mv;			/* pointer to just the value in the response */

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
```

## Update - found the module on Alibaba

So much for being smart - I managed to find the module [on Alibaba](https://www.alibaba.com/product-detail/RS232ttl-Serial-Laser-Rangefinder-Module-with_62507863905.html). Interestingly, they consider the checksum part of the measurement, and just repeat the measurement result for "bad measurement".

![image-20250617142623067](./images/alibaba.png)
