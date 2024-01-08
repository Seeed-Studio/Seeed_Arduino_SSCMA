#include <Seeed_Arduino_SSCMA.h>
SSCMA Infer;

void stop_rotate(void)
{
    digitalWrite(D0, HIGH);
    delayMicroseconds(1500);
    digitalWrite(D0, LOW);
}

void servo_rotate(int rtime, bool revers = false)
{
    uint32_t sleep;
    if (revers)
    {
        sleep = 2500;
    }
    else
    {
        sleep = 500;
    }
    digitalWrite(D0, HIGH);

    delayMicroseconds(sleep);
    digitalWrite(D0, LOW);
    delay(30 * rtime);
    stop_rotate();
}

void setup()
{
    Infer.begin();
    Serial.begin(9600);
    pinMode(D0, OUTPUT);
}

void loop()
{
    if (!Infer.invoke())
    {
        if (Infer.boxes().size() > 0)
        {
            if (Infer.boxes()[0].x < 80)
            {
                servo_rotate(1, true);
            }
            else if (Infer.boxes()[0].x > 140)
            {
                servo_rotate(1, false);
            }
            Serial.print("X==>>");
            Serial.print(Infer.boxes()[0].x);
            Serial.print("Y==>>");
            Serial.println(Infer.boxes()[0].y);
        }
    }
}
