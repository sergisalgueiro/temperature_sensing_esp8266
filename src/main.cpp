#include <Arduino.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <DHTesp.h>
#include <ESP8266WiFi.h>
#include <queue>

// Set up your Wi-Fi credentials
constexpr auto ssid = "DIGIFIBRA-FuKP";
const char *password = "atic_2_b";

// Set up your MQTT information
const char *mqtt_server = "192.168.1.167"; // set the IP address of the Raspberry
constexpr auto mqtt_port = 1883;
const char *mqtt_user = "ssg";                   // set the user of the mqtt server
const char *mqtt_password = "pestevino1";        // set the password of the mqtt server
const char *mqtt_topic = "temp_hum_sensor_room"; // set the topic
// const char* mqtt_topic = "temp_hum_sensor_living";     // set the topic
// const char* mqtt_topic = "temp_hum_sensor_terrace";     // set the topic
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

void setup_wifi()
{
    delay(10);
    // Connect to Wi-Fi
    Serial.println();
    Serial.print("Connection to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
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
    Serial.begin(115200);
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
        Serial.print("Attempting MQTT connection " + String(retries) + " ...");
        if (mqttClient.connect("ESP8266Client_1", mqtt_user, mqtt_password))
        {
            Serial.println("connected");
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
            retries++;
        }
    }
    // while (!mqttClient.connected()) {
    //     Serial.print("Connecting to MQTT...");
    //     if (mqttClient.connect("ESP8266Client_1", mqtt_user, mqtt_password)) {
    //     Serial.println("connected");
    //     } else {
    //     Serial.print("error, rc=");
    //     Serial.print(mqttClient.state());
    //     Serial.println(" try again in 5 seconds");
    //     delay(5000);
    //     }
    // }
}

void loop()
{
    // Sync time
    if (timeClient.update())
    {
        time_t const epoch{timeClient.getEpochTime()};
        timeval tv{epoch, 0};
        settimeofday(&tv, nullptr);
        Serial.println("Time updated: " + String(asctime(gmtime(&epoch))));
    }

    // Read data from DHT22
    float temperature = dht.getTemperature();
    Serial.println("Temperature: " + String(temperature) + "°C");
    float humidity = dht.getHumidity();
    Serial.println("Humidity: " + String(humidity) + "%");

    // Check if the reading is valid
    if (isnan(temperature) || isnan(humidity))
    {
        Serial.println("Error reading the DHT22_1 sensor!");
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
        Serial.println(payload);
        auto const result{mqttClient.publish(mqtt_topic, payload.c_str())};
        Serial.println("Publishing result: " + String(result));
        if (!result)
        {
            break;
        }
        readings.pop();
    }

    // Please wait before reading again
    end_loop();
}
