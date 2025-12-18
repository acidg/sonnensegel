#ifndef TIME_PROVIDER_H
#define TIME_PROVIDER_H

// Platform-independent time abstraction for testability
class ITimeProvider {
public:
    virtual ~ITimeProvider() = default;
    virtual unsigned long millis() const = 0;
};

#endif // TIME_PROVIDER_H
