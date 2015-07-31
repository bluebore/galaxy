# OPT ?= -O2 -DNDEBUG     # (A) Production use (optimized mode)
OPT ?= -g2 -Wall -Werror  # (B) Debug mode, w/ full line-level debugging symbols
# OPT ?= -O2 -g2 -DNDEBUG # (C) Profiling mode: opt, but w/debugging symbols

# Thirdparty
SNAPPY_PATH=./thirdparty/snappy/
PROTOBUF_PATH=./thirdparty/protobuf/
PROTOC_PATH=
PROTOC=$(PROTOC_PATH)protoc
PBRPC_PATH=./thirdparty/sofa-pbrpc/output/
BOOST_PATH=../boost/
PREFIX=./output
INCLUDE_PATH = -I./ -I./src -I$(PROTOBUF_PATH)/include \
               -I$(PBRPC_PATH)/include \
               -I$(SNAPPY_PATH)/include \
               -I$(BOOST_PATH)/include

LDFLAGS = -L$(PROTOBUF_PATH)/lib -lprotobuf \
          -L$(PBRPC_PATH)/lib -lsofa-pbrpc \
          -L$(SNAPPY_PATH)/lib -lsnappy \
          -Lcommon/ -lcommon \
          -lpthread -lz -lgflags

CXXFLAGS += $(OPT)

PROTO_FILE = $(wildcard src/proto/*.proto)
PROTO_SRC = $(patsubst %.proto,%.pb.cc,$(PROTO_FILE))
PROTO_HEADER = $(patsubst %.proto,%.pb.h,$(PROTO_FILE))
PROTO_OBJ = $(patsubst %.proto,%.pb.o,$(PROTO_FILE))

MASTER_SRC = $(wildcard src/master/*.cc)
MASTER_OBJ = $(patsubst %.cc, %.o, $(MASTER_SRC))
MASTER_HEADER = $(wildcard src/master/*.h)

SCHEDULER_SRC = $(wildcard src/scheduler/*.cc)
SCHEDULER_OBJ = $(patsubst %.cc, %.o, $(SCHEDULER_SRC))
SCHEDULER_HEADER = $(wildcard src/scheduler/*.h)

AGENT_SRC = $(wildcard src/agent/*.cc)
AGENT_OBJ = $(patsubst %.cc, %.o, $(AGENT_SRC))
AGENT_HEADER = $(wildcard src/agent/*.h)

SDK_SRC = $(wildcard src/sdk/*.cc)
SDK_OBJ = $(patsubst %.cc, %.o, $(SDK_SRC))
SDK_HEADER = $(wildcard src/sdk/*.h)

CLIENT_OBJ = $(patsubst %.cc, %.o, $(wildcard src/client/*.cc))

FLAGS_OBJ = $(patsubst %.cc, %.o, $(wildcard src/*.cc))
OBJS = $(FLAGS_OBJ) $(PROTO_OBJ)

LIBS = libgalaxy.a
BIN = master agent scheduler

all: $(BIN)

# Depends
$(MASTER_OBJ) $(AGENT_OBJ) $(PROTO_OBJ) $(SDK_OBJ): $(PROTO_HEADER)
$(MASTER_OBJ): $(MASTER_HEADER)
$(AGENT_OBJ): $(AGENT_HEADER)
$(SDK_OBJ): $(SDK_HEADER)

# Targets
master: $(MASTER_OBJ) $(OBJS)
	$(CXX) $(MASTER_OBJ) $(OBJS) -o $@ $(LDFLAGS)

scheduler: $(SCHEDULER_OBJ) $(OBJS)
	$(CXX) $(SCHEDULER_OBJ) $(OBJS) -o $@ $(LDFLAGS)

agent: $(AGENT_OBJ) $(OBJS)
	$(CXX) $(AGENT_OBJ) $(OBJS) -o $@ $(LDFLAGS)

libgalaxy.a: $(SDK_OBJ) $(OBJS) $(PROTO_HEADER)
	$(AR) -rs $@ $(SDK_OBJ) $(OBJS)

galaxy_client: $(CLIENT_OBJ) $(LIBS)
	$(CXX) $(CLIENT_OBJ) $(LIBS) -o $@ $(LDFLAGS)

%.o: %.cc
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATH) -c $< -o $@

%.pb.h %.pb.cc: %.proto
	$(PROTOC) --proto_path=./src/proto/ --proto_path=/usr/local/include --cpp_out=./src/proto/ $<

clean:
	rm -rf $(BIN)
	rm -rf $(MASTER_OBJ) $(SCHEDULER_OBJ) $(AGENT_OBJ) $(SDK_OBJ) $(CLIENT_OBJ) $(OBJS)
	rm -rf $(PROTO_SRC) $(PROTO_HEADER)
	rm -rf $(PREFIX)
	rm -rf $(LIBS) 

install: $(BIN) $(LIBS)
	mkdir -p $(PREFIX)/bin
	mkdir -p $(PREFIX)/lib
	mkdir -p $(PREFIX)/include/sdk
	cp $(BIN) $(PREFIX)/bin
	cp $(LIBS) $(PREFIX)/lib
	cp src/sdk/*.h $(PREFIX)/include/sdk

.PHONY: test
test:
	echo done
