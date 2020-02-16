#pragma once

#include <string>

class SatelliteClientImpl;

class SatelliteClient
{
public:
    ~SatelliteClient(void);
    static SatelliteClient& GetInstance(void);

    void SetServer(const std::string &ip_port);
    std::string GetNode(const std::string &service);

private:
    SatelliteClient(void);
    SatelliteClient(const SatelliteClient&) = delete;
    SatelliteClientImpl *PImpl;
    static SatelliteClient _Instance;
    friend void PullerFunction(void);
};
