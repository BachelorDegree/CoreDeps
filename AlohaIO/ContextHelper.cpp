#include <string>
#include <colib/co_routine.h>
#include <colib/co_routine_specific.h>
#include "MonitorClient.hpp"
#include "ContextHelper.hpp"
#define RETURN_CODE_NAME "return-code"
#define CALLER_INTERFACE_NAME "caller-interface-name"
struct ServerContextHelperWrapper
{
  ServerContextHelper *pServerContextHelper;
};
CO_ROUTINE_SPECIFIC(ServerContextHelperWrapper, co_pServerContextHelper);
ServerContextHelper *ServerContextHelper::GetInstance()
{
  return co_pServerContextHelper->pServerContextHelper;
}
void ServerContextHelper::SetInstance(ServerContextHelper *pServerContextHelper)
{
  co_pServerContextHelper->pServerContextHelper = pServerContextHelper;
}
alohaio::SystemCookie &ServerContextHelper::GetSystemCookieInstance()
{
  return m_oSystemCookie;
}
alohaio::UserCookie &ServerContextHelper::GetUserCookieInstance()
{
  return m_oUserCookie;
}
void ServerContextHelper::MakeClientContext(grpc::ClientContext &oContext) const
{
  oContext.AddMetadata(CALLER_INTERFACE_NAME, this->GetCallerInterfaceName());
}
void ServerContextHelper::SetCalleeInterfaceName(const std::string &str)
{
  this->m_strCalleeInterfaceName = str;
}
void ServerContextHelper::SetCallerInterfaceName(const std::string &str)
{
  this->m_strCallerInterfaceName = str;
}
const std::string &ServerContextHelper::GetCalleeInterfaceName() const
{
  return m_strCalleeInterfaceName;
}
const std::string &ServerContextHelper::GetCallerInterfaceName() const
{
  return m_strCallerInterfaceName;
}
void ServerContextHelper::BindContext(grpc::ServerContext &oContext)
{
  m_pServerContext = &oContext;
  for (auto &oPair : m_pServerContext->client_metadata())
  {
    if (oPair.first == CALLER_INTERFACE_NAME)
    {
      this->SetCallerInterfaceName(oPair.second.data());
    }
  }
}
int ServerContextHelper::GetReturnCode() const
{
  return m_iReturnCode;
}
void ServerContextHelper::SetReturnCode(int iRet)
{
  m_iReturnCode = iRet;
  m_pServerContext->AddTrailingMetadata(RETURN_CODE_NAME, std::to_string(m_iReturnCode));
  MonitorClient::IncrStatusCode(false, this->GetCallerInterfaceName(), this->GetCalleeInterfaceName(), iRet);
}

ClientContextHelper::ClientContextHelper(grpc::ClientContext &oContext) : m_oContext(oContext)
{
}
int ClientContextHelper::GetReturnCode() const
{
  if (m_oContext.GetServerTrailingMetadata().count(RETURN_CODE_NAME) > 0)
  {
    m_oContext.GetServerTrailingMetadata().find(RETURN_CODE_NAME)->second;
    auto _str = m_oContext.GetServerTrailingMetadata().find(RETURN_CODE_NAME)->second;
    std::string str{_str.data(), _str.length()};
    str.push_back('\0');
    int iRet;
    if (1 != sscanf(str.data(), "%d", &iRet))
    {
      return -2;
    }
    return iRet;
  }
  return -1;
}