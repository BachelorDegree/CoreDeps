#pragma once

#include <string>

class SatelliteClientImpl;

class SatelliteClient
{
public:
    ~SatelliteClient(void);

    static SatelliteClient& GetInstance(void);

    void SetServer(const std::string &sIpPort);
    std::string GetNode(const std::string &sService);
    void RegisterLocalService(const std::string &sService, const std::string &sInterface, const std::string &sPort);

private:
    SatelliteClient(void);
    SatelliteClient(const SatelliteClient&) = delete;
    SatelliteClientImpl *PImpl;
    void Heartbeat(void);
    void PullerFunction(void);
};
