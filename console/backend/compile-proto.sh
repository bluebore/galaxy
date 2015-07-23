#!/usr/bin/env sh
export PATH=../../../../../third-64/protobuf/bin:$PATH
echo "build task proto"
protoc -I ../../src/proto/  --python_out=src/galaxy/ ../../src/proto/task.proto
echo "build master proto"
protoc -I ../../src/proto/  --python_out=src/galaxy/ ../../src/proto/master.proto
echo "build agent proto"
protoc -I ../../src/proto/  --python_out=src/galaxy/ ../../src/proto/agent.proto
protoc -I ../../src/proto/  --python_out=src/galaxy/ ../../src/proto/monitor.proto
