#!/bin/bash
if [ -z $1 ]; then
    echo "USAGE: $0 <proto_name>"
else
    protoc --cpp_out=. $1.proto
    protoc --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` $1.proto
    mv *.h ../include
    mv *.cc ../ProtoSources
fi
