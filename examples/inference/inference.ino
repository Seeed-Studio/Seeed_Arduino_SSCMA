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
        for (int i = 0; i < AI.boxes().size(); i++)
        {
            Serial.printf(
                "box %d: x=%d, y=%d, w=%d, h=%d, score=%d, target=%d\n", i,
                AI.boxes()[i].x, AI.boxes()[i].y, AI.boxes()[i].w,
                AI.boxes()[i].h, AI.boxes()[i].score, AI.boxes()[i].target);
        }
        for (int i = 0; i < AI.classes().size(); i++)
        {
            Serial.printf("class %d: target=%d, score=%d\n", i,
                          AI.classes()[i].target, AI.classes()[i].score);
        }
        for (int i = 0; i < AI.points().size(); i++)
        {
            Serial.printf("point %d: x=%d, y=%d, z=%d, score=%d, target=%d\n",
                          i, AI.points()[i].x, AI.points()[i].y,
                          AI.points()[i].z, AI.points()[i].score,
                          AI.points()[i].target);
        }
    }
}