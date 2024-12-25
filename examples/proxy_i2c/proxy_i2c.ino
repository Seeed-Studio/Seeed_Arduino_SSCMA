#include <Arduino.h>
#include <Wire.h>

#include <Seeed_Arduino_SSCMA.h>

SSCMA AI;

void setup()
{
    // put your setup code here, to run once:
    Wire.begin();
    Serial.begin(115200);
    while (!Serial)
        delay(1000);
      
    AI.begin(&Wire, D3);
    Serial.println("Proxy start");
}

char buf[512];
void loop()
{
    int s_len = Serial.available();
    if (s_len)
    {
        int len = Serial.readBytes((char *)buf, s_len);
        AI.write(buf, len);
    }
    int t_len = AI.available();
    if (t_len)
    {
        if (t_len > 512)
        {
            t_len = 512;
        }

        AI.read(buf, t_len);
        Serial.write(buf, t_len);
    }
}
