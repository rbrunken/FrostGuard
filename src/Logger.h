#ifndef LOGGER_H
#define LOGGER_H
#include <string.h>
#include <EspMQTTClient.h>

using namespace std;

#define ERROR_TOPIC_PREFIX "Error/"
#define MESSAGE_TOPIC_PREFIX "Info/"

class Logger
{
private:
    EspMQTTClient *_mqttClient = NULL;
    string _errorTopic = ERROR_TOPIC_PREFIX;
    string _messageTopic = MESSAGE_TOPIC_PREFIX;

    void messageToMqtt(char *buff, size_t len);
    void errorToMqtt(char *buff, size_t len);
    void toHex(uint8_t n, char* buf);
public:
    void begin(){_mqttClient = NULL;}
    void begin(char *myName, EspMQTTClient *mqttClient);
    size_t printError(const char *format, ...);
    size_t printMessage(const char *format, ...);
    size_t printHex(uint8_t* n, size_t size);
};

#endif