#include <map>
#include <mutex>
#include <vector>
#include <thread>
#include <cstdint>
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
    std::map<string, std::unique_ptr<Satellite::Stub>>  ServerToStubs;
    std::map<string, ServiceDescriptor>                 ServiceToDescriptor;

    SatelliteClientImpl(void):
        VersionNum(0) { }
};

SatelliteClient SatelliteClient::_Instance;

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
    return SatelliteClient::_Instance;
}

void PullerFunction(void)
{
    auto self = &SatelliteClient::GetInstance();
    auto pimpl = self->PImpl;
    int64_t newestVersion;
    string satelliteServer;
    do
    {
        // check version
        for (auto &i : pimpl->ServerToStubs)
        {
            grpc::ClientContext ctx;
            google::protobuf::Empty req;
            GetCurrentVersionResponse rsp;
            auto ret = i.second->GetCurrentVersion(&ctx, req, &rsp);
            if (!ret.ok())
            {
                spdlog::warn("Satellite.GetCurrentVersion failed, server: {}, errcode: {}, errmsg: {}", 
                    i.first, ret.error_code(), ret.error_message());
                continue;
            }
            if (rsp.timestamp() <= pimpl->VersionNum)
                return;
            newestVersion = rsp.timestamp();
            satelliteServer = i.first;
            break;
        }
    } while (false);
    if (satelliteServer.empty())
    {
        spdlog::error("All satellite servers no response");
        return;
    }
    // get new configurations
    for (auto &sd : pimpl->ServiceToDescriptor)
    {
        grpc::ClientContext ctx;
        GetServiceNodesRequest req;
        GetServiceNodesResponse rsp;
        req.set_service_name(sd.first);
        auto ret = pimpl->ServerToStubs[satelliteServer]->GetServiceNodes(&ctx, req, &rsp);
        if (!ret.ok())
        {
            spdlog::warn("Satellite.GetServiceNodes failed, server: {}, errcode: {}, errmsg: {}", 
                satelliteServer, ret.error_code(), ret.error_message());
            continue;
        }
        lock_guard guard(sd.second.Mutex);
        sd.second.Nodes.clear();
        for (auto i = 0; i < rsp.nodes_size(); ++i)
        {
            const auto &ref = rsp.nodes(i);
            sd.second.Nodes.emplace_back((ServiceNode){ ref.server_ip_port(), ref.weight() });
        }
    }
    pimpl->VersionNum = newestVersion;
}

void SatelliteClient::SetServer(const std::string &ip_port)
{
    auto channel = grpc::CreateChannel(ip_port, grpc::InsecureChannelCredentials());
    auto stub = Satellite::NewStub(channel);
    PImpl->ServerToStubs[ip_port] = std::move(stub);
    // First satellite server added, then spwan a thread to pull
    if (PImpl->ServerToStubs.size() == 1)
    {
        std::thread puller(PullerFunction);
        puller.detach();
    }
}

std::string SatelliteClient::GetNode(const std::string &service)
{
    auto f = PImpl->ServiceToDescriptor.find(service);
    if (f == PImpl->ServiceToDescriptor.end())
    {
        bool loadOK = false;
        for (auto &i : PImpl->ServerToStubs)
        {
            grpc::ClientContext ctx;
            GetServiceNodesRequest req;
            GetServiceNodesResponse rsp;
            req.set_service_name(service);
            auto ret = i.second->GetServiceNodes(&ctx, req, &rsp);
            if (!ret.ok())
            {
                spdlog::warn("Satellite.GetServiceNodes failed, server: {}, errcode: {}, errmsg: {}", 
                    i.first, ret.error_code(), ret.error_message());
                continue;
            }
            auto &sd = PImpl->ServiceToDescriptor[service];
            lock_guard guard(sd.Mutex);
            for (auto i = 0; i < rsp.nodes_size(); ++i)
                sd.Nodes.emplace_back((ServiceNode){ rsp.nodes(i).server_ip_port(), rsp.nodes(i).weight() });
            f = PImpl->ServiceToDescriptor.find(service);
            loadOK = true;
            break;
        }
        if (!loadOK)
            return string();
    }
    auto &sd = f->second;
    lock_guard guard(sd.Mutex);
    if (sd.QueuePtr >= sd.Nodes.size())
        sd.QueuePtr = 0;
    return sd.Nodes[sd.QueuePtr++].IpPort;
}
