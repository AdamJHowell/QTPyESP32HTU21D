/**
 * This sketch is a branch of my PubSubWeather sketch.
 * This sketch will use a HTU21D (SHT20/SHT21 compatible) sensor to measure temperature and humidity.
 * The ESP-32 SDA pin is GPIO21, and SCL is GPIO22.
 * @copyright   Copyright © 2022 Adam Howell
 * @licence     The MIT License (MIT)
 */
#include "WiFi.h"						// This header is part of the standard library.  https://www.arduino.cc/en/Reference/WiFi
#include <Wire.h>						// This header is part of the standard library.  https://www.arduino.cc/en/reference/wire
#include <PubSubClient.h>			// PubSub is the MQTT API.  Author: Nick O'Leary  https://github.com/knolleary/pubsubclient
#include "SHT2x.h"					// Rob Tillaart's excellent SHT20-series library: https://github.com/RobTillaart/SHT2x
#include "privateInfo.h"			// I use this file to hide my network information from random people browsing my GitHub repo.
#include <Adafruit_NeoPixel.h>	// The Adafruit NeoPixel library to drive the RGB LED on the QT Py.	https://github.com/adafruit/Adafruit_NeoPixel


#define NUMPIXELS        1


/**
 * Declare network variables.
 * Adjust the commented-out variables to match your network and broker settings.
 * The commented-out variables are stored in "privateInfo.h", which I do not upload to GitHub.
 */
//const char* wifiSsid = "yourSSID";				// Typically kept in "privateInfo.h".
//const char* wifiPassword = "yourPassword";		// Typically kept in "privateInfo.h".
//const char* mqttBroker = "yourBrokerAddress";	// Typically kept in "privateInfo.h".
//const int mqttPort = 1883;							// Typically kept in "privateInfo.h".
const char* mqttTopic = "espWeather";
const String sketchName = "QTPyESP32HTU21D";
const char* notes = "Adafruit QT Py ESP32-S2 with HTU21D";
char ipAddress[16];
char macAddress[18];
int loopCount = 0;
int loopDelay = 60000;
uint32_t start;
uint32_t stop;


// Create class objects.
WiFiClient espClient;							// Network client.
PubSubClient mqttClient( espClient );		// MQTT client.
Adafruit_NeoPixel pixels( NUMPIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800 );
SHT2x htu21d;


/**
 * The setup() function runs once when the device is booted, and then loop() takes over.
 */
void setup()
{
	// Start the Serial communication to send messages to the computer.
	Serial.begin( 115200 );
	if( !Serial )
		delay( 1000 );
	Serial.println( "Setup is initializing the I2C bus for the Stemma QT port." );
	Wire.setPins( SDA1, SCL1 );	// This is what selects the Stemma QT port, otherwise the two pin headers will be I2C.
	Wire.begin();

	Serial.println( "Setup is initializing the HTU21D sensor." );
	Serial.println( __FILE__ );
	Serial.print( "SHT2x_LIB_VERSION: \t" );
	Serial.println( SHT2x_LIB_VERSION );

	htu21d.begin();

	uint8_t stat = htu21d.getStatus();
	Serial.print( stat, HEX );

#if defined( NEOPIXEL_POWER )
	// If this board has a power control pin, we must set it to output and high in order to enable the NeoPixels.
	// We put this in an #ifdef so it can be reused for other boards without compilation errors.
	pinMode( NEOPIXEL_POWER, OUTPUT );
	digitalWrite( NEOPIXEL_POWER, HIGH );
#endif
	// Initialize the NeoPixel.
	pixels.begin();
	pixels.setBrightness( 20 );

	// Set the ipAddress char array to a default value.
	snprintf( ipAddress, 16, "127.0.0.1" );

	// Set the MQTT client parameters.
	mqttClient.setServer( mqttBroker, mqttPort );

	// Get the MAC address and store it in macAddress.
	snprintf( macAddress, 18, "%s", WiFi.macAddress().c_str() );

	// Try to connect to the configured WiFi network, up to 10 times.
	wifiConnect( 20 );

	Serial.println( "Setup is complete!" );
} // End of setup() function.


void wifiConnect( int maxAttempts )
{
	// Announce WiFi parameters.
	String logString = "WiFi connecting to SSID: ";
	logString += wifiSsid;
	Serial.println( logString );

	// Connect to the WiFi network.
	Serial.printf( "Wi-Fi mode set to WIFI_STA %s\n", WiFi.mode( WIFI_STA ) ? "" : " - Failed!" );
	WiFi.begin( wifiSsid, wifiPassword );

	int i = 0;
	/*
     WiFi.status() return values:
     0 : WL_IDLE_STATUS when WiFi is in process of changing between statuses
     1 : WL_NO_SSID_AVAIL in case configured SSID cannot be reached
     3 : WL_CONNECTED after successful connection is established
     4 : WL_CONNECT_FAILED if wifiPassword is incorrect
     6 : WL_DISCONNECTED if module is not configured in station mode
  */
	// Loop until WiFi has connected.
	while( WiFi.status() != WL_CONNECTED && i < maxAttempts )
	{
		delay( 1000 );
		Serial.println( "Waiting for a connection..." );
		Serial.print( "WiFi status: " );
		Serial.println( WiFi.status() );
		logString = ++i;
		logString += " seconds";
		Serial.println( logString );
	}

	// Print that WiFi has connected.
	Serial.println( '\n' );
	Serial.println( "WiFi connection established!" );
	Serial.print( "MAC address: " );
	Serial.println( macAddress );
	Serial.print( "IP address: " );
	snprintf( ipAddress, 16, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] );
	Serial.println( ipAddress );
} // End of wifiConnect() function.


// mqttConnect() will attempt to (re)connect the MQTT client.
void mqttConnect( int maxAttempts )
{
	int i = 0;
	// Loop until MQTT has connected.
	while( !mqttClient.connected() && i < maxAttempts )
	{
		Serial.print( "Attempting MQTT connection..." );
		// Connect to the broker using the MAC address for a clientID.  This guarantees that the clientID is unique.
		if( mqttClient.connect( macAddress ) )
		{
			Serial.println( "connected!" );
		}
		else
		{
			Serial.print( " failed, return code: " );
			Serial.print( mqttClient.state() );
			Serial.println( " try again in 2 seconds" );
			// Wait 5 seconds before retrying.
			delay( 5000 );
		}
		i++;
	}
} // End of mqttConnect() function.



/**
 * The loop() function begins after setup(), and repeats as long as the unit is powered.
 */
void loop()
{
	loopCount++;
	Serial.println();
	Serial.println( sketchName );

	// Read the sensor data.
	start = micros();
	htu21d.read();
	stop = micros();

	// Print the sensor data to the serial port.
	Serial.print( "Measurement took " );
	Serial.print( ( stop - start ) / 1000 );
	Serial.println( " milliseconds." );
	Serial.print( "\tTemp: " );
	Serial.print( htu21d.getTemperature(), 1 );
	Serial.print( "\tRH %: " );
	Serial.println( htu21d.getHumidity(), 1 );

	// Set color to red and wait a half second.
	pixels.fill( 0xFF0000 );
	pixels.show();
	delay( 500 );


	// Check the mqttClient connection state.
	if( !mqttClient.connected() )
	{
		// Reconnect to the MQTT broker.
		mqttConnect( 10 );
	}
	// The loop() function facilitates the receiving of messages and maintains the connection to the broker.
	mqttClient.loop();


	// Set color to blue and wait a half second.
	pixels.fill( 0x0000FF );
	pixels.show();
	delay( 500 );

	// Prepare a String to hold the JSON.
	char mqttString[512];
	// Write the readings to the String in JSON format.
	snprintf( mqttString, 512, "{\n\t\"sketch\": \"%s\",\n\t\"mac\": \"%s\",\n\t\"ip\": \"%s\",\n\t\"tempC\": %.2f,\n\t\"humidity\": %.2f,\n\t\"uptime\": %d,\n\t\"notes\": \"%s\"\n}", sketchName, macAddress, ipAddress, htu21d.getTemperature(), htu21d.getHumidity(), loopCount, notes );
	if( mqttClient.connected() )
	{
		// Publish the JSON to the MQTT broker.
		mqttClient.publish( mqttTopic, mqttString );
		Serial.print( "Published to topic " );
		Serial.println( mqttTopic );
	}
	else
	{
		Serial.println( "Lost connection to the MQTT broker between the start of this loop and now!" );
	}
	// Print the JSON to the Serial port.
	Serial.println( mqttString );

	String logString = "loopCount: ";
	logString += loopCount;
	Serial.println( logString );

	// Set color to green and wait one minute.
	pixels.fill( 0x00FF00 );
	pixels.show();

	Serial.print( "Pausing for " );
	Serial.print( loopDelay / 1000 );
	Serial.println( " seconds.\n" );
	delay( loopDelay );
} // End of loop() function.
