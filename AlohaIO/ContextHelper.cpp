
#include <colib/co_routine.h>
#include <colib/co_routine_specific.h>
#include "ContextHelper.hpp"
struct RequestContextHelperWrapper
{
  RequestContextHelper *pRequestContextHelper;
};
CO_ROUTINE_SPECIFIC(RequestContextHelperWrapper, co_pRequestContextHelper);
RequestContextHelper *RequestContextHelper::GetInstance()
{
  return co_pRequestContextHelper->pRequestContextHelper;
}
void RequestContextHelper::SetInstance(RequestContextHelper *pRequestContextHelper)
{
  co_pRequestContextHelper->pRequestContextHelper = pRequestContextHelper;
}
alohaio::SystemCookie &RequestContextHelper::GetSystemCookieInstance()
{
  return m_oSystemCookie;
}
alohaio::UserCookie &RequestContextHelper::GetUserCookieInstance()
{
  return m_oUserCookie;
}
void RequestContextHelper::MakeContext(grpc::ClientContext &) const
{
}
void RequestContextHelper::ParseFromContext(const grpc::ClientContext &)
{
}
ResponseContextHelper::ResponseContextHelper(grpc::ServerContext &oContext) : m_oContext(oContext)
{
  
}
int ResponseContextHelper::GetReturnCode() const
{
  return 0;
}
void ResponseContextHelper::SetReturnCode(int)
{
}