
#include <DHTesp.h>

/**
 * Class to read DHT sensor data at a fixed interval.
 *
 */
class DHTReader : public DHTesp
{
public:
    DHTReader();

    void set_read_interval(uint interval_ms);

    bool read_data(TempAndHumidity &data);

private:
    uint read_interval_ms;
    unsigned long last_read_time;
};
