#include <Seeed_Arduino_SSCMA.h>

SSCMA AI;
JsonDocument info;


// Helper function to decode a Base64 character
inline unsigned char decode_base64_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

// Decode Base64 string
String base64_decode(const String& input) {
    // Check if the input length is valid
    if (input.length() % 4 != 0) {
        throw std::invalid_argument("Invalid Base64 input length");
    }

    String output;
    std::vector<char> temp(3);

    size_t padding = 0;
    if (input.length() >= 2) {
        if (input[input.length() - 1] == '=') padding++;
        if (input[input.length() - 2] == '=') padding++;
    }

    for (size_t i = 0; i < input.length(); i += 4) {
        unsigned char b1 = decode_base64_char(input[i]);
        unsigned char b2 = decode_base64_char(input[i + 1]);
        unsigned char b3 = input[i + 2] == '=' ? 0 : decode_base64_char(input[i + 2]);
        unsigned char b4 = input[i + 3] == '=' ? 0 : decode_base64_char(input[i + 3]);

        temp[0] = (b1 << 2) | (b2 >> 4);
        temp[1] = ((b2 & 0x0F) << 4) | (b3 >> 2);
        temp[2] = ((b3 & 0x03) << 6) | b4;

        output += temp[0];
        if (input[i + 2] != '=') output += temp[1];
        if (input[i + 3] != '=') output += temp[2];
    }

    return output;
}


void setup()
{
    AI.begin();
    Serial.begin(9600);

    DeserializationError error = deserializeJson(info, base64_decode(AI.info()));

    if(error){
      Serial.println("Can not fetch Model Information");
      while(1){
        delay(100);
      }
    }


}

void loop()
{
    if (!AI.invoke())
    {
        Serial.println("invoke success");
        Serial.print("perf: prepocess=");
        Serial.print(AI.perf().prepocess);
        Serial.print(", inference=");
        Serial.print(AI.perf().inference);
        Serial.print(", postpocess=");
        Serial.println(AI.perf().postprocess);

        for (int i = 0; i < AI.boxes().size(); i++)
        {
            Serial.print("Box[");
            Serial.print(i);
            Serial.print("] target=");
            Serial.print(info["classes"][AI.boxes()[i].target].as<String>());
            Serial.print(", score=");
            Serial.print(AI.boxes()[i].score);
            Serial.print(", x=");
            Serial.print(AI.boxes()[i].x);
            Serial.print(", y=");
            Serial.print(AI.boxes()[i].y);
            Serial.print(", w=");
            Serial.print(AI.boxes()[i].w);
            Serial.print(", h=");
            Serial.println(AI.boxes()[i].h);
        }
        for (int i = 0; i < AI.classes().size(); i++)
        {
            Serial.print("Class[");
            Serial.print(i);
            Serial.print("] target=");
            Serial.print(info["classes"][AI.classes()[i].target].as<String>());
            Serial.print(", score=");
            Serial.println(AI.classes()[i].score);
        }
        for (int i = 0; i < AI.points().size(); i++)
        {
            Serial.print("Point[");
            Serial.print(i);
            Serial.print("]: target=");
            Serial.print(info["classes"][AI.points()[i].target].as<String>());
            Serial.print(", score=");
            Serial.print(AI.points()[i].score);
            Serial.print(", x=");
            Serial.print(AI.points()[i].x);
            Serial.print(", y=");
            Serial.println(AI.points()[i].y);
        }
        for (int i = 0; i < AI.keypoints().size(); i++)
        {
            Serial.print("keypoint[");
            Serial.print(i);
            Serial.print("] target=");
            Serial.print(info["classes"][AI.keypoints()[i].box.target].as<String>());
            Serial.print(", score=");
            Serial.print(AI.keypoints()[i].box.score);
            Serial.print(", box:[x=");
            Serial.print(AI.keypoints()[i].box.x);
            Serial.print(", y=");
            Serial.print(AI.keypoints()[i].box.y);
            Serial.print(", w=");
            Serial.print(AI.keypoints()[i].box.w);
            Serial.print(", h=");
            Serial.print(AI.keypoints()[i].box.h);
            Serial.print("], points:[");
            for (int j = 0; j < AI.keypoints()[i].points.size(); j++)
            {
                Serial.print("[");
                Serial.print(AI.keypoints()[i].points[j].x);
                Serial.print(",");
                Serial.print(AI.keypoints()[i].points[j].y);
                Serial.print("],");
            }
            Serial.println("]");
        }
    }
}