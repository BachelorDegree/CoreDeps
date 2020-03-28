#pragma once
#include <grpc/grpc.h>
#include <grpcpp/client_context.h>
#include <grpcpp/server_context.h>
#include "alohaio.pb.h"
class RequestContextHelper
{
public:
  static RequestContextHelper *GetInstance();
  static void SetInstance(RequestContextHelper *);
  alohaio::SystemCookie &GetSystemCookieInstance();
  alohaio::UserCookie &GetUserCookieInstance();
  void MakeContext(grpc::ClientContext &) const;
  void ParseFromContext(const grpc::ClientContext &);
private:
  alohaio::SystemCookie m_oSystemCookie;
  alohaio::UserCookie m_oUserCookie;
};
class ResponseContextHelper
{
  public:
    ResponseContextHelper(grpc::ServerContext &);
    int GetReturnCode() const;
    void SetReturnCode(int);
  private:
    grpc::ServerContext & m_oContext;
};