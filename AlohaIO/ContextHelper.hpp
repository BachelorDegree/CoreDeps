#ifndef _CONTEXT_HEADER_HPP_
#define _CONTEXT_HEADER_HPP_
#include <grpc/grpc.h>
#include <grpcpp/client_context.h>
#include <grpcpp/server_context.h>
#include <string>
#include "alohaio.pb.h"
class ServerContextHelper
{
public:
  static ServerContextHelper *GetInstance();
  static void SetInstance(ServerContextHelper *);
  void SetCalleeInterfaceName(const std::string &);
  void SetCallerInterfaceName(const std::string &);
  const std::string &GetCalleeInterfaceName() const;
  const std::string &GetCallerInterfaceName() const;
  alohaio::SystemCookie &GetSystemCookieInstance();
  alohaio::UserCookie &GetUserCookieInstance();
  void MakeClientContext(grpc::ClientContext &) const;
  void BindContext(grpc::ServerContext &);
  int GetReturnCode() const;
  void SetReturnCode(int);

private:
  alohaio::SystemCookie m_oSystemCookie;
  alohaio::UserCookie m_oUserCookie;
  grpc::ServerContext *m_pServerContext;
  std::string m_strCallerInterfaceName;
  std::string m_strCalleeInterfaceName;
  int m_iReturnCode;
};
class ClientContextHelper
{
public:
  ClientContextHelper(grpc::ClientContext &);
  int GetReturnCode() const;

private:
  grpc::ClientContext &m_oContext;
};
#endif