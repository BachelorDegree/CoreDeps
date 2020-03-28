#pragma once
#include <grpc/grpc.h>
#include <grpcpp/client_context.h>
#include <grpcpp/server_context.h>
#include "alohaio.pb.h"
class ServerContextHelper
{
public:
  static ServerContextHelper *GetInstance();
  static void SetInstance(ServerContextHelper *);
  alohaio::SystemCookie &GetSystemCookieInstance();
  alohaio::UserCookie &GetUserCookieInstance();
  void MakeClientContext(grpc::ClientContext &) const;
  void BindContext(grpc::ServerContext &);
  int GetReturnCode() const;
  void SetReturnCode(int);
private:
  alohaio::SystemCookie m_oSystemCookie;
  alohaio::UserCookie m_oUserCookie;
  grpc::ServerContext * m_pServerContext;
  int m_iReturnCode;
};
class ClientContextHelper
{
  public:
    ClientContextHelper(grpc::ClientContext &);
    int GetReturnCode() const;
  private:
    grpc::ClientContext & m_oContext;
};