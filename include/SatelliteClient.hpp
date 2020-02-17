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
    void RegisterLocalService(const std::string &service, const std::string &interface, const std::string &port);

private:
    SatelliteClient(void);
    SatelliteClient(const SatelliteClient&) = delete;
    SatelliteClientImpl *PImpl;
    static SatelliteClient _Instance;
    void PullerFunction(void);
};
