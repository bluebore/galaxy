# OPT ?= -O2 -DNDEBUG     # (A) Production use (optimized mode)
OPT ?= -g2 -Wall -Werror  # (B) Debug mode, w/ full line-level debugging symbols
# OPT ?= -O2 -g2 -DNDEBUG # (C) Profiling mode: opt, but w/debugging symbols

# Thirdparty
include depends.mk

PREFIX=./output
INCLUDE_PATH = -I./ -I./src -I$(PROTOBUF_PATH)/include \
               -I$(PBRPC_PATH)/include \
               -I$(SNAPPY_PATH)/include \
               -I$(BOOST_PATH)/include \
               -Icommon/include \
               -I$(INS_PATH)/include \
               -I$(GFLAGS_PATH)/include \

LDFLAGS = -L$(PROTOBUF_PATH)/lib \
          -L$(PBRPC_PATH)/lib -lins_sdk -lsofa-pbrpc -lprotobuf \
          -L$(SNAPPY_PATH)/lib -lsnappy \
          -L$(GFLAGS_PATH)/lib  \
          -Lcommon/ -lcommon \
          -L$(INS_PATH)/lib \
          -lgflags -lpthread -lz

CXXFLAGS += $(OPT)

PROTO_FILE = $(wildcard src/proto/*.proto)
PROTO_SRC = $(patsubst %.proto,%.pb.cc,$(PROTO_FILE))
PROTO_HEADER = $(patsubst %.proto,%.pb.h,$(PROTO_FILE))
PROTO_OBJ = $(patsubst %.proto,%.pb.o,$(PROTO_FILE))

MASTER_SRC = $(wildcard src/master/*.cc)
MASTER_OBJ = $(patsubst %.cc, %.o, $(MASTER_SRC))
MASTER_HEADER = $(wildcard src/master/*.h)

SCHEDULER_SRC = $(wildcard src/scheduler/scheduler*.cc)
SCHEDULER_OBJ = $(patsubst %.cc, %.o, $(SCHEDULER_SRC))
SCHEDULER_HEADER = $(wildcard src/scheduler/*.h)

AGENT_SRC = $(wildcard src/agent/agent*.cc) src/agent/pod_manager.cc src/agent/utils.cc src/agent/cgroups.cc src/agent/task_manager.cc
AGENT_OBJ = $(patsubst %.cc, %.o, $(AGENT_SRC))
AGENT_HEADER = $(wildcard src/agent/*.h) src/agent/pod_manager.h src/agent/utils.h src/agent/cgroups.h src/agent/task_manager.h

TEST_AGENT_SRC = src/agent/test_agent.cc
TEST_AGENT_OBJ = $(patsubst %.cc, %.o, $(TEST_AGENT_SRC))

INITD_SRC = $(wildcard src/gce/initd*.cc) src/agent/utils.cc src/flags.cc 
INITD_OBJ = $(patsubst %.cc, %.o, $(INITD_SRC))
INITD_HEADER = $(wildcard src/gce/*.h) src/agent/utils.h 

TEST_INITD_SRC = src/gce/test_initd.cc
TEST_INITD_OBJ = $(patsubst %.cc, %.o, $(TEST_INITD_SRC))
#TEST_INITD_HEADER = $(wildcard src/gce/*.h) src/gce/utils.h

SDK_SRC = $(wildcard src/sdk/*.cc)
SDK_OBJ = $(patsubst %.cc, %.o, $(SDK_SRC))
SDK_HEADER = $(wildcard src/sdk/*.h)

SCHED_TEST_SRC = $(wildcard src/scheduler/test*.cc)
SCHED_TEST_OBJ = $(patsubst %.cc, %.o, $(SCHED_TEST_SRC))

UTILS_SRC = $(wildcard src/utils/*.cc)
UTILS_HEADER = $(wildcard src/utils/*.h)
UTILS_OBJ = $(patsubst %.cc, %.o, $(UTILS_SRC))

WATCHER_SRC = $(wildcard src/master/master_watcher.cc)
WATCHER_OBJ = $(patsubst %.cc, %.o, $(WATCHER_SRC))
WATCHER_HEADER = $(wildcard src/master/master_watcher.h)


CLIENT_OBJ = $(patsubst %.cc, %.o, $(wildcard src/client/*.cc))

FLAGS_OBJ = $(patsubst %.cc, %.o, $(wildcard src/*.cc))
OBJS = $(FLAGS_OBJ) $(PROTO_OBJ)

LIBS = libgalaxy.a
BIN = master agent scheduler galaxy initd 

all: $(BIN) $(LIBS)

# Depends
$(MASTER_OBJ) $(AGENT_OBJ) $(PROTO_OBJ) $(SDK_OBJ) $(SCHED_TEST_OBJ): $(PROTO_HEADER)
$(MASTER_OBJ): $(MASTER_HEADER) $(UTILS_HEADER)
$(AGENT_OBJ): $(AGENT_HEADER)
$(SDK_OBJ): $(SDK_HEADER)
$(SCHEDULER_OBJ): $(UTILS_HEADER)

# Targets
master: $(MASTER_OBJ) $(UTILS_OBJ) $(OBJS)
	$(CXX) $(MASTER_OBJ) $(UTILS_OBJ) $(OBJS) -o $@ $(LDFLAGS)

scheduler: $(SCHEDULER_OBJ) $(OBJS) $(WATCHER_OBJ) $(UTILS_OBJ) 
	$(CXX) $(SCHEDULER_OBJ) $(UTILS_OBJ) $(WATCHER_OBJ) $(OBJS) -o $@ $(LDFLAGS)

agent: $(AGENT_OBJ) $(WATCHER_OBJ) $(OBJS)
	$(CXX) $(AGENT_OBJ) $(WATCHER_OBJ) $(OBJS) -o $@ $(LDFLAGS)

test_agent: $(TEST_AGENT_OBJ) $(LIBS) $(OBJS)
	$(CXX) $(TEST_AGENT_OBJ) $(LIBS) -o $@ $(LDFLAGS)

libgalaxy.a: $(SDK_OBJ) $(OBJS) $(PROTO_HEADER)
	$(AR) -rs $@ $(SDK_OBJ) $(OBJS)

galaxy: $(CLIENT_OBJ) $(LIBS)
	$(CXX) $(CLIENT_OBJ) $(LIBS) -o $@ $(LDFLAGS)

initd: $(INITD_OBJ) $(LIBS) $(OBJS)
	$(CXX) $(INITD_OBJ) $(LIBS) -o $@ $(LDFLAGS)

test_initd: $(TEST_INITD_OBJ) $(LIBS) $(OBJS)
	$(CXX) $(TEST_INITD_OBJ) $(LIBS) -o $@ $(LDFLAGS)

test_sched: $(SCHED_TEST_OBJ) $(LIBS) $(OBJS) 
	$(CXX) $(SCHED_TEST_OBJ) $(LIBS) -o $@ $(LDFLAGS)

%.o: %.cc
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATH) -c $< -o $@

%.pb.h %.pb.cc: %.proto
	$(PROTOC) --proto_path=./src/proto/ --proto_path=/usr/local/include --cpp_out=./src/proto/ $<

clean:
	rm -rf $(BIN)
	rm -rf $(MASTER_OBJ) $(SCHEDULER_OBJ) $(AGENT_OBJ) $(SDK_OBJ) $(CLIENT_OBJ) $(SCHED_TEST_OBJ) $(UTILS_OBJ) $(OBJS)
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
