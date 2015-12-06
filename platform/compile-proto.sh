echo "build galaxy.proto"
protoc -I ../src/proto/  --python_out=src/galaxy/ ../src/proto/galaxy.proto
echo "build master.proto"
protoc -I ../src/proto/  --python_out=src/galaxy/ ../src/proto/master.proto
echo "build lumia agent.proto"

protoc -I ../src/proto/  --python_out=src/galaxy/ ../src/proto/log.proto
protoc -I ../src/proto/  --python_out=src/galaxy/ ../src/proto/initd.proto
