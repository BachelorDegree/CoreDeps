#include <map>
#include <mutex>
#include <vector>
#include <thread>
#include <cstdint>
#include <net/if.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <grpcpp/grpcpp.h>
#include <spdlog/spdlog.h>
#include "satellite.grpc.pb.h"
#include "SatelliteClient.hpp"

using std::string;
using lock_guard = std::lock_guard<std::mutex>;

struct ServiceNode
{
    std::string IpPort;
    int32_t     Weight;
};

struct LocalService
{
    std::string IpPort;
    std::string ServiceName;
};

struct ServiceDescriptor
{
    std::mutex                  Mutex;
    std::vector<ServiceNode>    Nodes;
    uint64_t                    QueuePtr;
    ServiceDescriptor(void):
        QueuePtr(0) { }
};

class SatelliteClientImpl
{
public:
    int64_t VersionNum;
    std::vector<std::string>            UpstreamServers;
    std::vector<LocalService>           LocalServices;
    std::map<string, ServiceDescriptor> ServiceToDescriptor;

    SatelliteClientImpl(void):
        VersionNum(0) { }
};

SatelliteClient::SatelliteClient(void)
{
    PImpl = new SatelliteClientImpl;
}

SatelliteClient::~SatelliteClient(void)
{
    delete PImpl;
}

SatelliteClient& SatelliteClient::GetInstance(void)
{
    static SatelliteClient oInstance;
    return oInstance;
}

void SatelliteClient::Heartbeat(void)
{
    auto iTimestamp = time(nullptr);
    do
    {
        for (const auto &i : PImpl->LocalServices)
        {
            for (const auto &sServerIpPort : PImpl->UpstreamServers)
            {
                auto pChannel = grpc::CreateChannel(sServerIpPort, grpc::InsecureChannelCredentials());
                auto pStub = Satellite::NewStub(pChannel);
                grpc::ClientContext oContext;
                HeartbeatRequest oReq;
                GeneralStatus oRsp;
                oReq.set_timestamp(iTimestamp);
                oReq.mutable_service_info()->set_weight(100);
                oReq.mutable_service_info()->set_server_ip_port(i.IpPort);
                oReq.mutable_service_info()->set_service_name(i.ServiceName);
                auto oRet = pStub->Heartbeat(&oContext, oReq, &oRsp);
                if (!oRet.ok())
                {
                    spdlog::warn("Satellite.Heartbeat rpc failed, server: {}, errcode: {}, errmsg: {}", 
                        sServerIpPort, oRet.error_code(), oRet.error_message());
                    continue;
                }
                if (oRsp.code() != 0)
                {
                    spdlog::error("Satellite.Heartbeat logic failed, server: {}, code: {}, msg: {}", 
                        sServerIpPort, oRsp.code(), oRsp.message());
                }
            }
        }
    } while (false);
}

void SatelliteClient::PullerFunction(void)
{
    // Heartbeat
    this->Heartbeat();
    
    // Route table
    int64_t iNewestVersion;
    std::unique_ptr<Satellite::Stub> pStub = nullptr;
    do
    {
        // check version
        for (const auto &sServerIpPort : this->PImpl->UpstreamServers)
        {
            auto pChannel = grpc::CreateChannel(sServerIpPort, grpc::InsecureChannelCredentials());
            pStub = Satellite::NewStub(pChannel);
            grpc::ClientContext oContext;
            google::protobuf::Empty oReq;
            GetCurrentVersionResponse oRsp;
            auto oRet = pStub->GetCurrentVersion(&oContext, oReq, &oRsp);
            if (!oRet.ok())
            {
                spdlog::warn("Satellite.GetCurrentVersion rpc failed, server: {}, errcode: {}, errmsg: {}", 
                    sServerIpPort, oRet.error_code(), oRet.error_message());
                continue;
            }
            if (oRsp.timestamp() <= this->PImpl->VersionNum)
                goto RETURN;
            iNewestVersion = oRsp.timestamp();
            break;
        }
    } while (false);
    if (pStub == nullptr)
    {
        spdlog::error("All satellite servers no response");
        goto RETURN;
    }
    // get new configurations
    for (auto &oDescriptor : this->PImpl->ServiceToDescriptor)
    {
        grpc::ClientContext oContext;
        GetServiceNodesRequest oReq;
        GetServiceNodesResponse oRsp;
        oReq.set_service_name(oDescriptor.first);
        auto oRet = pStub->GetServiceNodes(&oContext, oReq, &oRsp);
        if (!oRet.ok())
        {
            spdlog::warn("Satellite.GetServiceNodes rpc failed, errcode: {}, errmsg: {}", 
                oRet.error_code(), oRet.error_message());
            continue;
        }
        lock_guard oGuard(oDescriptor.second.Mutex);
        oDescriptor.second.Nodes.clear();
        for (auto i = 0; i < oRsp.nodes_size(); ++i)
        {
            const auto &oServerInfo = oRsp.nodes(i);
            oDescriptor.second.Nodes.emplace_back((ServiceNode){ oServerInfo.server_ip_port(), oServerInfo.weight() });
        }
    }
    this->PImpl->VersionNum = iNewestVersion;
RETURN:
    std::this_thread::sleep_for(std::chrono::seconds(15));
}

void SatelliteClient::SetServer(const std::string &sIpPort)
{
    spdlog::info("Satellite upstream server added: {}", sIpPort);
    PImpl->UpstreamServers.push_back(sIpPort);
    // First satellite server added, then spwan a thread to pull
    if (PImpl->UpstreamServers.size() == 1)
    {
        std::thread thPuller([this](void)
        {
            for ( ; ;)
                this->PullerFunction();
        });
        thPuller.detach();
    }
}

std::string SatelliteClient::GetNode(const std::string &sService)
{
    auto itSD = PImpl->ServiceToDescriptor.find(sService);
    if (itSD == PImpl->ServiceToDescriptor.end())
    {
        bool bLoadOk = false;
        for (const auto &sServerIpPort : PImpl->UpstreamServers)
        {
            auto pChannel = grpc::CreateChannel(sServerIpPort, grpc::InsecureChannelCredentials());
            auto pStub = Satellite::NewStub(pChannel);
            grpc::ClientContext oContext;
            GetServiceNodesRequest oReq;
            GetServiceNodesResponse oRsp;
            oReq.set_service_name(sService);
            auto oRet = pStub->GetServiceNodes(&oContext, oReq, &oRsp);
            if (!oRet.ok())
            {
                spdlog::warn("Satellite.GetServiceNodes rpc failed, server: {}, errcode: {}, errmsg: {}", 
                    sServerIpPort, oRet.error_code(), oRet.error_message());
                continue;
            }
            auto &oDescriptor = PImpl->ServiceToDescriptor[sService];
            lock_guard oGuard(oDescriptor.Mutex);
            for (auto i = 0; i < oRsp.nodes_size(); ++i)
                oDescriptor.Nodes.emplace_back((ServiceNode){ oRsp.nodes(i).server_ip_port(), oRsp.nodes(i).weight() });
            itSD = PImpl->ServiceToDescriptor.find(sService);
            bLoadOk = true;
            break;
        }
        if (!bLoadOk)
            return string();
    }
    auto &oDescriptor = itSD->second;
    lock_guard oGuard(oDescriptor.Mutex);
    if (oDescriptor.Nodes.size() == 0)
        return string();
    if (oDescriptor.QueuePtr >= oDescriptor.Nodes.size())
        oDescriptor.QueuePtr = 0;
    return oDescriptor.Nodes[oDescriptor.QueuePtr++].IpPort;
}

void SatelliteClient::RegisterLocalService(const std::string &sService, const std::string &sInterface, const std::string &sPort)
{
    ifreq ifr;
    int iInetSock = socket(AF_INET, SOCK_DGRAM, 0);
    strcpy(ifr.ifr_ifrn.ifrn_name, sInterface.c_str());
    ioctl(iInetSock, SIOCGIFADDR, &ifr);
    string sIp = inet_ntoa(((sockaddr_in*)&ifr.ifr_addr)->sin_addr);
    PImpl->LocalServices.emplace_back((LocalService) { sIp.append(":").append(sPort), sService });
    this->Heartbeat();
}
