#include "credentials.h"
#include "DHTReader.h"

#include <Arduino.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <queue>

// #define debug

constexpr auto SEPARATOR{","};
constexpr auto READ_DHT22_INTERVAL_MS{2 * 60 * 1000};

using reading = std::tuple<time_t, float, float>;
std::queue<reading> readings;

DHTReader dht;
WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 0, 360000);

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
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
}

void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
    // Convert the message payload from bytes to a string
    String message;
    for (unsigned int i = 0; i < length; i++)
    {
        message += (char)payload[i];
    }
    println("Message arrived [" + String(topic) + "] " + message);
}

void setup()
{
    pinMode(D0, WAKEUP_PULLUP);
#ifdef debug
    Serial.begin(115200);
    // Serial.begin(74880);
#endif // debug
    delay(2000);
    setup_wifi();
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(mqtt_callback);
    timeClient.begin();
    dht.setup(D6, DHTesp::DHT22); // Connect DHT sensor to GPIO 12
    dht.set_read_interval(READ_DHT22_INTERVAL_MS);
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
            mqttClient.subscribe(mqtt_sub_topic);
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

    if (!timeClient.isTimeSet())
    {
        println("Time not set yet");
        return;
    }

    // Reconnect to MQTT broker if necessary
    if (!mqttClient.connected())
    {
        reconnect();
    }
    mqttClient.loop();

    // Read data from DHT22
    TempAndHumidity data;
    auto result{dht.read_data(data)};

    if (isnan(data.temperature) || isnan(data.humidity))
    {
        println("Failed to read from DHT sensor!");
        ESP.restart();
    }
    else
    {
        println("Temperature: " + String(data.temperature) + "°C");
        println("Humidity: " + String(data.humidity) + "%");
    }

    // Store if reading is valid
    if (result && !isnan(data.temperature) && !isnan(data.humidity))
    {
        auto const timeNowUTC{time(nullptr)};
        readings.push({timeNowUTC, data.temperature, data.humidity});
    }

    // Publish readings to MQTT broker
    while (!readings.empty())
    {
        auto const &[time, temperature, humidity]{readings.front()};
        String payload = String(time) + SEPARATOR + String(temperature) + SEPARATOR + String(humidity);
        println(payload);
        auto const result{mqttClient.publish(mqtt_pub_topic, payload.c_str())};
        println("Publishing result: " + String(result));
        if (!result)
        {
            break;
        }
        readings.pop();
    }
}
