# Seeed_Arduino_SSCMA Library

The Seeed_Arduino_SSCMA library is designed to enable Arduino hosts to communicate with sensors equipped with the SSCMA-Micro firmware. This library provides the necessary drivers to facilitate seamless interaction between the Arduino platform and sensors supporting the SSCMA-Micro firmware.

## Installation

1. Download the latest version of the Seeed_Arduino_SSCMA library from the [GitHub repository](https://github.com/seeed-studio/Seeed_Arduino_SSCMA).
2. Add the library to your Arduino IDE by selecting **Sketch > Include Library > Add .ZIP Library** and choosing the downloaded file.

## Usage

1. Include the library in your Arduino sketch:  
   ```c++
   #include <Seeed_Arduino_SSCMA.h>
   ```

2. Create an instance of the SSCMA object:
   ```c++
   SSCMA mySensor;
   ```

3. Initialize the sensor in your `setup()` function:
   ```c++
   void setup() {
       mySensor.begin();
   }
   ```

4. Read sensor data using appropriate methods provided by the library.

## Example

```c++
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
```

## Methods

- `begin()`: Initializes the sensor.
- `invoke()`: Invokes the sensor to perform inference.
- `perf()`: Returns the performance metrics of the sensor.
- `boxes()`: Returns the bounding boxes of the sensor.
- `classes()`: Returns the classification results of the sensor.
- `points()`: Returns the point cloud data of the sensor.

## Compatibility

This library is compatible with Arduino boards and sensors that support the SSCMA-Micro firmware.

## License

This library is released under the [MIT License](./LICENSE).

## Contributions

Contributions and improvements to the library are welcomed. If you encounter any issues or have suggestions for enhancements, please create an issue or pull request in the [GitHub repository](https://github.com/your_repository_link).

For more detailed information, including specific API references, please refer to the header files and the provided examples included in the library.

Thank you for using the Seeed_Arduino_SSCMA library! We hope it enhances your experience in working with SSCMA-Micro firmware supported sensors on the Arduino platform.