#pragma once

#include <string>
#include <cstdint>
class MonitorClientImpl;

class MonitorClient
{
public:
  ~MonitorClient();

  static MonitorClient *GetInstance(void);

  void SetServer(const std::string &sIpPort);
  void SetMyID(int32_t id);
  void Start();
  static void Incr(int32_t index_id, int32_t value);
  static void SetMax(int32_t index_id, int32_t value);
  static void IncrStatusCode(bool is_client_side, const std::string &caller_service_name, const std::string &callee_service_name, int status_code);

private:
  MonitorClient(void);
  MonitorClient(const MonitorClient &) = delete;

  void _Incr(int32_t index_id, int32_t value);
  void _SetMax(int32_t index_id, int32_t value);
  void _IncrStatusCode(bool is_client_side, const std::string &caller_service_name, const std::string &callee_service_name, int status_code);

  MonitorClientImpl *pImpl;
};
