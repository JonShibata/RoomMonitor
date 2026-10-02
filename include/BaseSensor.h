#ifndef BASE_SENSOR_H
#define BASE_SENSOR_H

class BaseSensor {
public:
    virtual ~BaseSensor() {}
    virtual void requestReading() = 0;
    virtual bool isReadingReady() const = 0;
    virtual void update() = 0;
};

#endif
