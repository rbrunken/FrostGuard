#include <stdio.h>
#include "Logger.h"

void Logger::begin(char *myName, EspMQTTClient *mqttClient){
    if(myName == NULL || mqttClient == NULL){
        return;
    }

    _mqttClient = mqttClient;
    _errorTopic += myName;
    _messageTopic += myName;
}

size_t Logger::printError(const char *format, ...){
    char loc_buf[100];
    char * temp = loc_buf;
    va_list arg;
    va_start(arg, format);
    int len = vsnprintf(loc_buf, sizeof(loc_buf), format, arg);
    va_end(arg);
    if(len < 0 || len >= sizeof(loc_buf)) {
        return 0;
    };

    Serial.write((uint8_t*)temp, len);
    Serial.println();
    errorToMqtt(temp, len);
    return len;
}

size_t Logger::printMessage(const char *format, ...){
    char loc_buf[100];
    char * temp = loc_buf;
    va_list arg;
    va_start(arg, format);
    int len = vsnprintf(loc_buf, sizeof(loc_buf), format, arg);
    va_end(arg);
    if(len < 0 || len >= sizeof(loc_buf)) {
        return 0;
    };

    Serial.write((uint8_t*)temp, len);
    Serial.println();
    messageToMqtt(temp, len);
    return len;
}

size_t Logger::printHex(uint8_t* n, size_t size)
{
    char str[size * 2 + 1];
    int32_t j = 0;
    for (size_t i = 0; i < size; i++)
    {
        sprintf(&(str[j]),"%x", n[i]);
        j += 2;
    }
    
    str[j] = '\0';

    Serial.write((uint8_t*)str, j);
    Serial.println();
    messageToMqtt(str, j);
    return j;
}

void Logger::toHex(uint8_t n, char* buf)
{
#define HEX 16

    // buf has a length of 3 byte
    char *str = &buf[sizeof(buf) - 1];
    uint8_t base = HEX;

    *str = '\0';

    do {
        unsigned long m = n;
        n /= base;
        char c = m - base * n;
        *--str = c < 10 ? c + '0' : c + 'A' - 10;
    } while(n);
}

void Logger::messageToMqtt(char *buff, size_t len){
    if(_mqttClient != NULL && _mqttClient->isConnected()){
        _mqttClient->publish(_messageTopic.c_str(), buff);
    }
}

void Logger::errorToMqtt(char *buff, size_t len){
    if(_mqttClient != NULL && _mqttClient->isConnected()){
        _mqttClient->publish(_errorTopic.c_str(), buff);
    }
}