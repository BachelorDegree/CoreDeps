#include <deque>
#include <atomic>
#include <mutex>
#include <vector>
#include <sstream>
#include <stdlib.h>
#include <spdlog/spdlog.h>
#include <curl/curl.h>
#include <boost/algorithm/string.hpp>
#include <thread>
#include "MonitorClient.hpp"
int PostHttpJson(const std::string &strIp, const std::string &strJson)
{
  CURL *curl;
  CURLcode res;

  /* In windows, this will init the winsock stuff */
  curl_global_init(CURL_GLOBAL_ALL);

  /* get a curl handle */
  curl = curl_easy_init();
  if (curl)
  {
    /* First set the URL that is about to receive our POST. This URL can
       just as well be a https:// URL if that is what should receive the
       data. */
    curl_easy_setopt(curl, CURLOPT_URL, strIp.c_str());
    /* Now specify the POST data */
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strJson.c_str());
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "charset: utf-8");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    /* Perform the request, res will get the return code */
    res = curl_easy_perform(curl);
    /* Check for errors */
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    if (res != CURLE_OK)
    {
      return res;
    }
    else
    {
      return 0;
    }
  }
  else
  {
    return -1;
  }
}
std::string exec(std::string command)
{
  char buffer[128];
  std::string result = "";

  // Open pipe to file
  FILE *pipe = popen(command.c_str(), "r");
  if (!pipe)
  {
    return "popen failed!";
  }

  // read till end of process:
  while (!feof(pipe))
  {

    // use buffer to read and add to result
    if (fgets(buffer, 128, pipe) != NULL)
      result += buffer;
  }

  pclose(pipe);
  return result;
}
template <class KEY_T, class HASHFUNC_T>
class HashDequeCounter
{
private:
  HASHFUNC_T m_HashFunc;
  struct Node
  {
    KEY_T key;
    std::atomic<int32_t> value;
  };

public:
  HashDequeCounter(size_t size)
  {
    m_size = size;
    m_szDeque = new std::deque<Node>[size];
  }
  ~HashDequeCounter()
  {
    if (m_szDeque)
    {
      delete[] m_szDeque;
      m_szDeque = nullptr;
    }
  }
  void GetValueAndSetZero(std::vector<std::pair<KEY_T, int32_t>> &vecResult)
  {
    for (size_t i = 0; i < m_size; i++)
    {
      auto &deque = m_szDeque[i];
      auto size = deque.size();
      for (size_t pos = 0; pos < size; pos++)
      {
        if (deque[pos].value > 0)
        {
          const auto &key = deque[pos].key;
          auto value = GetAndSet(key, 0);
          vecResult.push_back(std::make_pair(key, value));
        }
      }
    }
  }
  Node *GetNode(const KEY_T &key)
  {
    auto &deque = m_szDeque[GetRouteKey(key)];
    Node *pNode = nullptr;
    auto deque_size = deque.size();
    for (size_t i = 0; i < deque_size; i++)
    {
      if (deque[i].key == key)
      {
        pNode = &deque[i];
        break;
      }
    }
    if (pNode == nullptr)
    {
      std::lock_guard<std::mutex> oGuard{m_mutex};
      for (size_t i = 0; i < deque.size(); i++)
      {
        if (deque[i].key == key)
        {
          pNode = &deque[i];
          break;
        }
      }
      if (pNode == nullptr)
      {
        deque.emplace_back();
        pNode = &(*deque.rbegin());
        pNode->key = key;
        pNode->value = 0;
      }
    }
    return pNode;
  }
  void SetMax(const KEY_T &key, int32_t value)
  {
    auto pNode = GetNode(key);
    int32_t cur;
    do
    {
      cur = pNode->value.load();
      if (cur >= value)
      {
        break;
      }

    } while (std::atomic_compare_exchange_weak(&(pNode->value), &cur, value) == false);
  }
  int32_t GetAndSet(const KEY_T &key, int32_t value)
  {
    auto pNode = GetNode(key);
    int32_t cur;
    do
    {
      cur = pNode->value.load();
    } while (std::atomic_compare_exchange_weak(&(pNode->value), &cur, value) == false);
    return cur;
  }
  void Increase(const KEY_T &key, int32_t value)
  {
    auto pNode = GetNode(key);
    int32_t cur;
    int32_t nxt;
    do
    {
      cur = pNode->value.load();
      nxt = cur + value;
    } while (std::atomic_compare_exchange_weak(&(pNode->value), &cur, nxt) == false);
  }

private:
  size_t GetRouteKey(const KEY_T &key) const
  {
    return m_HashFunc(key) % m_size;
  }
  size_t m_size;
  std::deque<Node> *m_szDeque;
  std::mutex m_mutex;
};
class MonitorClientImpl
{
public:
  MonitorClientImpl() : m_service_index_counter(1007), m_client_status_code_counter(1007), m_server_status_code_counter(1007)
  {
  }
  void Start()
  {
    auto thread = std::thread([&]() -> void {
      while (true)
      {
        this->ReportServerInfo();
        this->ReportServiceIndex();
        this->ReportStatusCode();
        sleep(10);
      }
    });
    thread.detach();
  }
  void SetServer(const std::string &sIpPort)
  {
    m_strIP = sIpPort;
  }
  void SetMyID(int32_t id)
  {
    m_server_id = id;
  }
  void Incr(int32_t index_id, int32_t value)
  {
    m_service_index_counter.Increase(index_id, value);
  }
  void SetMax(int32_t index_id, int32_t value)
  {
    m_service_index_counter.SetMax(index_id, value);
  }
  void IncrStatusCode(bool is_client_side, const std::string &caller_service_name, const std::string &callee_service_name, int status_code)
  {
    auto oTuple = std::forward_as_tuple(caller_service_name, callee_service_name, status_code);
    if (is_client_side)
    {
      m_client_status_code_counter.Increase(oTuple, 1);
    }
    else
    {
      m_server_status_code_counter.Increase(oTuple, 1);
    }
  }
  void ReportStatusCode()
  {
    std::vector<std::pair<std::tuple<std::string, std::string, int>, int32_t>> vecValuesClient;
    std::vector<std::pair<std::tuple<std::string, std::string, int>, int32_t>> vecValuesServer;
    m_client_status_code_counter.GetValueAndSetZero(vecValuesClient);
    m_server_status_code_counter.GetValueAndSetZero(vecValuesServer);
    std::stringstream ss;
    ss << "[";
    bool bFirst = false;
    for (auto oPair : vecValuesClient)
    {
      if (bFirst == false)
      {
        bFirst = true;
      }
      else
      {
        ss << ",";
      }
      ss << "{\"report_machine_id\": " << m_server_id;
      ss << ", \"caller_service_name\": \"" << std::get<0>(oPair.first) << '"';
      ss << ", \"callee_service_name\": \"" << std::get<1>(oPair.first) << '"';
      ss << ", \"status_code\": " << std::get<2>(oPair.first);
      ss << ", \"count\": " << oPair.second;
      ss << ", \"is_client_report\": true";
      ss << "}";
    }
    for (auto oPair : vecValuesServer)
    {
      if (bFirst == false)
      {
        bFirst = true;
      }
      else
      {
        ss << ",";
      }
      ss << "{\"report_machine_id\": " << m_server_id;
      ss << ", \"caller_service_name\": \"" << std::get<0>(oPair.first) << '"';
      ss << ", \"callee_service_name\": \"" << std::get<1>(oPair.first) << '"';
      ss << ", \"status_code\": " << std::get<2>(oPair.first);
      ss << ", \"count\": " << oPair.second;
      ss << ", \"is_client_report\": false";
      ss << "}";
    }
    ss << "]";
    std::string strJson = ss.str();
    ss.clear();
    SPDLOG_DEBUG("Req: {}", strJson);
    int iRet = PostHttpJson(this->m_strIP + "/report/status_code/", strJson);
    if (iRet != 0)
    {
      SPDLOG_ERROR("ReportStatusCode Failed. Ret: {}", iRet);
    }
  }
  void ReportServiceIndex()
  {
    std::vector<std::pair<int32_t, int32_t>> vecValues;
    m_service_index_counter.GetValueAndSetZero(vecValues);
    std::stringstream ss;
    ss << "[";
    bool bFirst = false;
    for (auto oPair : vecValues)
    {
      if (bFirst == false)
      {
        bFirst = true;
      }
      else
      {
        ss << ",";
      }
      ss << "{\"machine_id\": " << m_server_id;
      ss << ", \"service_index_id\": " << oPair.first;
      ss << ", \"value\": " << oPair.second;
      ss << "}";
    }
    ss << "]";
    std::string strJson = ss.str();
    ss.clear();
    SPDLOG_DEBUG("Req: {}", strJson);
    int iRet = PostHttpJson(this->m_strIP + "/report/service_index/", strJson);
    if (iRet != 0)
    {
      SPDLOG_ERROR("ReportServiceIndex Failed. Ret: {}", iRet);
    }
  }
  void ReportServerInfo()
  {
    std::vector<std::pair<std::string, std::string>> vecInfo;
    vecInfo.emplace_back("avg_load", exec("cat /proc/loadavg"));
    vecInfo.emplace_back("mem_usage", exec("cat /proc/meminfo"));
    vecInfo.emplace_back("io_stat", exec("iostat"));
    vecInfo.emplace_back("process_info", exec("ps aux"));
    vecInfo.emplace_back("disk_info", exec("df"));
    std::stringstream ss;
    ss << "{ \"machine_id\": " << this->m_server_id;
    for (auto oPair : vecInfo)
    {
      ss << ",";
      boost::algorithm::replace_all(oPair.second, "\\", "\\\\");
      boost::algorithm::replace_all(oPair.second, "\n", "\\n");
      boost::algorithm::replace_all(oPair.second, "\t", "\\t");
      ss << "\"" << oPair.first << "\": "
         << "\"" << oPair.second << "\"";
    }
    ss << "}";
    std::string strJson = ss.str();
    ss.clear();
    SPDLOG_DEBUG("Req: {}", strJson);
    int iRet = PostHttpJson(this->m_strIP + "/report/machine/", strJson);
    if (iRet != 0)
    {
      SPDLOG_ERROR("ReportServiceIndex Failed. Ret: {}", iRet);
    }
  }

private:
  std::string m_strIP;
  int32_t m_server_id;
  class StatusCodeHashFunction
  {
  public:
    int32_t operator()(const std::tuple<std::string, std::string, int> &data) const
    {
      int32_t a = 0;
      for (char b : std::get<0>(data))
      {
        a = a * 1007 + b;
      }
      for (char b : std::get<1>(data))
      {
        a = a * 1007 + b;
      }
      a = a * 1007 + std::get<2>(data);
      return a;
    }
  };
  class Uint32HashFunction
  {
  public:
    int32_t operator()(int32_t a) const
    {
      return a;
    }
  };
  HashDequeCounter<int32_t, Uint32HashFunction> m_service_index_counter;
  HashDequeCounter<std::tuple<std::string, std::string, int>, StatusCodeHashFunction> m_client_status_code_counter;
  HashDequeCounter<std::tuple<std::string, std::string, int>, StatusCodeHashFunction> m_server_status_code_counter;
};
MonitorClient::~MonitorClient()
{
  if (pImpl)
  {
    delete pImpl;
    pImpl = nullptr;
  }
}

MonitorClient *MonitorClient::GetInstance(void)
{
  static MonitorClient oClient;
  return &oClient;
}
void MonitorClient::Start()
{
  pImpl->Start();
}
void MonitorClient::SetServer(const std::string &sIpPort)
{
  pImpl->SetServer(sIpPort);
}
void MonitorClient::SetMyID(int32_t id)
{
  pImpl->SetMyID(id);
}
void MonitorClient::Incr(int32_t index_id, int32_t value)
{
  GetInstance()->_Incr(index_id, value);
}
void MonitorClient::SetMax(int32_t index_id, int32_t value)
{
  GetInstance()->_SetMax(index_id, value);
}

MonitorClient::MonitorClient(void)
{
  this->pImpl = new MonitorClientImpl;
}

void MonitorClient::_Incr(int32_t index_id, int32_t value)
{
  this->pImpl->Incr(index_id, value);
}
void MonitorClient::_SetMax(int32_t index_id, int32_t value)

{
  this->pImpl->SetMax(index_id, value);
}
void MonitorClient::_IncrStatusCode(bool is_client_side, const std::string &caller_service_name, const std::string &callee_service_name, int status_code)
{
  this->pImpl->IncrStatusCode(is_client_side, caller_service_name, callee_service_name, status_code);
}

void MonitorClient::IncrStatusCode(bool is_client_side, const std::string &caller_service_name, const std::string &callee_service_name, int status_code)
{
  GetInstance()->_IncrStatusCode(is_client_side, caller_service_name, callee_service_name, status_code);
}