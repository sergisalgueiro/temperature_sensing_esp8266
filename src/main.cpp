#include "credentials.h"

#include <Arduino.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <DHTesp.h>
#include <ESP8266WiFi.h>
#include <queue>

#define debug

constexpr auto DHT22_PIN{14};
constexpr auto SEPARATOR{","};
using reading = std::tuple<time_t, float, float>;
std::queue<reading> readings;

// Set the DHT22 sensor pin
DHTesp dht;

WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 360000);

template <typename T>
void println(T line)
{
#ifdef debug
    Serial.println(line);
#endif // debug
}

template <typename T>
void print(T print)
{
#ifdef debug
    Serial.print(print);
#endif // debug
}

void setup_wifi()
{
    delay(10);
    // Connect to Wi-Fi
    print("Connection to ");
    println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        print(".");
    }
    println("");
    println("WiFi connected");
}

String removeLastCharacter(String input)
{
    // Checks whether the string has at least one character
    if (input.length() > 0)
    {
        // Use substring() to get a substring excluding the last character
        return input.substring(0, input.length() - 1);
    }
    else
    {
        // The string is empty, return an empty string
        return "";
    }
}

void end_loop()
{
    delay(10000);
}

void setup()
{
#ifdef debug
    Serial.begin(115200);
#endif // debug
    delay(2000);
    setup_wifi();
    mqttClient.setServer(mqtt_server, mqtt_port);
    dht.setup(12, DHTesp::AM2302); // Connect DHT sensor to GPIO 12
    timeClient.begin();
}

void reconnect()
{
    static constexpr auto MAX_RETRIES{2u};
    auto retries{0u};
    while (!mqttClient.connected() && retries < MAX_RETRIES)
    {
        print("Attempting MQTT connection " + String(retries) + " ...");
        if (mqttClient.connect("ESP8266Client_1", mqtt_user, mqtt_password))
        {
            println("connected");
        }
        else
        {
            print("failed, rc=");
            print(mqttClient.state());
            println(" try again in 5 seconds");
            delay(5000);
            retries++;
        }
    }
}

void loop()
{
    // Sync time
    if (timeClient.update())
    {
        time_t const epoch{timeClient.getEpochTime()};
        timeval tv{epoch, 0};
        settimeofday(&tv, nullptr);
        println("Time updated: " + String(asctime(gmtime(&epoch))));
    }

    // Read data from DHT22
    float temperature = dht.getTemperature();
    println("Temperature: " + String(temperature) + "°C");
    float humidity = dht.getHumidity();
    println("Humidity: " + String(humidity) + "%");

    // Check if the reading is valid
    if (isnan(temperature) || isnan(humidity))
    {
        println("Error reading the DHT22_1 sensor!");
        end_loop();
    }

    // Get reading timestamp
    auto const timeNowUTC{time(nullptr)};
    readings.push({timeNowUTC, temperature, humidity});

    // Reconnect to MQTT broker if necessary
    if (!mqttClient.connected())
    {
        reconnect();
    }
    // auto const mqttConnected{mqttClient.loop()};
    mqttClient.loop();

    // Prepare payload
    while (!readings.empty())
    {
        auto const &[time, temperature, humidity]{readings.front()};
        String payload = String(time) + SEPARATOR + String(temperature) + SEPARATOR + String(humidity);
        println(payload);
        auto const result{mqttClient.publish(mqtt_topic, payload.c_str())};
        println("Publishing result: " + String(result));
        if (!result)
        {
            break;
        }
        readings.pop();
    }

    // Please wait before reading again
    end_loop();
}
