#include <Seeed_Arduino_SSCMA.h>

SSCMA AI;

void setup()
{
    AI.begin();
    Serial.begin(9600);
}

void loop()
{
    if (!AI.invoke())
    {
        Serial.println("invoke success");
        Serial.printf("perf: prepocess=%d, inference=%d, postprocess=%d\n",
                      AI.perf().prepocess, AI.perf().inference,
                      AI.perf().postprocess);
        Serial.print("perf: postprocess=");
        Serial.print(AI.perf().postprocess);
        Serial.print(", inference=");
        Serial.print(AI.perf().inference);
        Serial.println(", prepocess=");

        for (int i = 0; i < AI.boxes().size(); i++)
        {
            Serial.printf(
                "box %d: x=%d, y=%d, w=%d, h=%d, score=%d, target=%d\n", i,
                AI.boxes()[i].x, AI.boxes()[i].y, AI.boxes()[i].w,
                AI.boxes()[i].h, AI.boxes()[i].score, AI.boxes()[i].target);
            Serial.print("Box[");
            Serial.print(i);
            Serial.print("] target=");
            Serial.print(AI.boxes()[i].target);
            Serial.print(", score=");
            Serial.print(AI.boxes()[i].score);
            Serial.print(", x=");
            Serial.print(AI.boxes()[i].x);
            Serial.print(", y=");
            Serial.print(AI.boxes()[i].y);
            Serial.print(", w=");
            Serial.print(AI.boxes()[i].w);
            Serial.print(", h=");
            Serial.print(AI.boxes()[i].h);
        }
        for (int i = 0; i < AI.classes().size(); i++)
        {
            Serial.print("Class[");
            Serial.print(i);
            Serial.print("] target=");
            Serial.print(AI.classes()[i].target);
            Serial.print(", score=");
            Serial.print(AI.classes()[i].score);
        }
        for (int i = 0; i < AI.points().size(); i++)
        {
            Serial.print("Point[");
            Serial.print(i);
            Serial.print("] target=");
            Serial.print(AI.points()[i].target);
            Serial.print(", score=");
            Serial.print(AI.points()[i].score);
            Serial.print(", x=");
            Serial.print(AI.points()[i].x);
            Serial.print(", y=");
            Serial.print(AI.points()[i].y);
        }
    }
}