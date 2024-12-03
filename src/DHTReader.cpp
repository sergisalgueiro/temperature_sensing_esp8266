#include "DHTReader.h"

DHTReader::DHTReader() : DHTesp()
{
}

void DHTReader::set_read_interval(uint interval)
{
    read_interval_ms = interval;
}

bool DHTReader::read_data(TempAndHumidity &data)
{
    if (millis() - last_read_time < read_interval_ms)
    {
        return false;
    }
    last_read_time = millis();
    data = getTempAndHumidity();
    return true;
}
