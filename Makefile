# OPT ?= -O2 -DNDEBUG     # (A) Production use (optimized mode)
OPT ?= -g2 -Wall -Werror  # (B) Debug mode, w/ full line-level debugging symbols
# OPT ?= -O2 -g2 -DNDEBUG # (C) Profiling mode: opt, but w/debugging symbols

# Thirdparty
include depends.mk

CC = gcc
CXX = g++

SHARED_CFLAGS = -fPIC
SHARED_LDFLAGS = -shared -Wl,-soname -Wl,

INCPATH += -I./src -I./include $(DEPS_INCPATH) 
CFLAGS += -std=c99 $(OPT) $(SHARED_CFLAGS) $(INCPATH)
CXXFLAGS += $(OPT) $(SHARED_CFLAGS) $(INCPATH)
LDFLAGS += -rdynamic $(DEPS_LDPATH) $(DEPS_LDFLAGS) -lpthread -lrt -lz -ldl


PREFIX=./output


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

AGENT_SRC = $(wildcard src/agent/agent*.cc) src/agent/pod_manager.cc src/agent/task_manager.cc src/agent/utils.cc src/agent/persistence_handler.cc src/agent/cgroups.cc src/agent/resource_collector.cc 
AGENT_OBJ = $(patsubst %.cc, %.o, $(AGENT_SRC))
AGENT_HEADER = $(wildcard src/agent/*.h) 
TEST_AGENT_SRC = src/agent/test_agent.cc src/agent/resource_collector.cc src/agent/cgroups.cc src/agent/utils.cc 
TEST_AGENT_OBJ = $(patsubst %.cc, %.o, $(TEST_AGENT_SRC))

INITD_SRC = src/gce/initd_impl.cc src/gce/initd_main.cc src/agent/utils.cc src/flags.cc  src/agent/cgroups.cc
INITD_OBJ = $(patsubst %.cc, %.o, $(INITD_SRC))
INITD_HEADER = $(wildcard src/gce/*.h) src/agent/utils.h 

INITD_CLI_SRC = src/gce/initd_cli.cc 
INITD_CLI_OBJ = $(patsubst %.cc, %.o, $(INITD_CLI_SRC))

INITD_SERVER_SRC = src/gce/initd_cli_server.cc
INITD_SERVER_OBJ = $(patsubst %.cc, %.o, $(INITD_CLI_SRC))


TEST_INITD_SRC = src/gce/test_initd.cc
TEST_INITD_OBJ = $(patsubst %.cc, %.o, $(TEST_INITD_SRC))

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
BIN = master agent scheduler galaxy initd initd_cli initd_cli_server 
TEST_BIN = test_agent test_initd
all: $(BIN) $(LIBS) $(TEST_BIN)

# Depends
$(MASTER_OBJ) $(AGENT_OBJ) $(PROTO_OBJ) $(SDK_OBJ) $(SCHED_TEST_OBJ): $(PROTO_HEADER)
$(MASTER_OBJ): $(MASTER_HEADER) $(UTILS_HEADER)
$(AGENT_OBJ): $(AGENT_HEADER)
$(SDK_OBJ): $(SDK_HEADER)
$(SCHEDULER_OBJ): $(UTILS_HEADER)

# Targets
master: $(MASTER_OBJ) $(UTILS_OBJ) $(OBJS)
	$(CXX) $(MASTER_OBJ) $(UTILS_OBJ) $(OBJS) -o $@  $(LDFLAGS)

scheduler: $(SCHEDULER_OBJ) $(OBJS) $(WATCHER_OBJ) $(UTILS_OBJ) 
	$(CXX) $(SCHEDULER_OBJ) $(UTILS_OBJ) $(WATCHER_OBJ) $(OBJS) -o $@  $(LDFLAGS)

agent: $(AGENT_OBJ) $(WATCHER_OBJ) $(OBJS)
	$(CXX) $(UTILS_OBJ) $(AGENT_OBJ) $(WATCHER_OBJ) $(OBJS) -o $@ $(LDFLAGS)

libgalaxy.a: $(SDK_OBJ) $(OBJS) $(PROTO_HEADER)
	$(AR) -rs $@ $(SDK_OBJ) $(OBJS)

galaxy: $(CLIENT_OBJ) $(LIBS)
	$(CXX) $(CLIENT_OBJ) $(LIBS) -o $@ $(LDFLAGS)

initd: $(INITD_OBJ) $(LIBS) $(OBJS)
	$(CXX) $(INITD_OBJ) $(LIBS) -o $@ $(LDFLAGS)

initd_cli: $(INITD_CLI_OBJ) $(LIBS) $(OBJS)
	$(CXX) $(INITD_CLI_OBJ) $(LIBS) -o $@ $(LDFLAGS)

initd_cli_server: $(INITD_SERVER_OBJ) $(LIBS) $(OBJS)
	$(CXX) $(INITD_SERVER_OBJ) $(LIBS) -o $@ $(LDFLAGS)

test_main: $(TEST_TRACE_OBJ) 
	$(CXX) $(TEST_TRACE_OBJ) -o $@ $(FTRACE_LIBDIR)/libftrace.a $(LDFLAGS)

test_agent: $(TEST_AGENT_OBJ) $(LIBS) $(OBJS)
	$(CXX) $(TEST_AGENT_OBJ) $(UTILS_OBJ) $(LIBS) -o $@ $(LDFLAGS)

test_initd: $(TEST_INITD_OBJ) $(LIBS) $(OBJS)
	$(CXX) $(TEST_INITD_OBJ) $(LIBS) -o $@ $(LDFLAGS)

test_sched: $(SCHED_TEST_OBJ) $(LIBS) $(OBJS) 
	$(CXX) $(SCHED_TEST_OBJ) $(LIBS) -o $@ $(LDFLAGS)

%.o: %.cc
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

%.pb.h %.pb.cc: %.proto
	$(PROTOC) --proto_path=./src/proto/ --proto_path=/usr/local/include --cpp_out=./src/proto/ $<

clean:
	rm -rf $(BIN)
	rm -rf $(TEST_BIN)
	rm -rf $(MASTER_OBJ) $(SCHEDULER_OBJ) $(AGENT_OBJ) $(SDK_OBJ) $(CLIENT_OBJ) $(SCHED_TEST_OBJ) $(UTILS_OBJ) $(OBJS)
	rm -rf $(PROTO_SRC) $(PROTO_HEADER)
	rm -rf $(PREFIX)
	rm -rf $(LIBS) 

install: $(BIN) $(LIBS)
	mkdir -p $(PREFIX)/bin
	mkdir -p $(PREFIX)/test_bin
	mkdir -p $(PREFIX)/lib
	mkdir -p $(PREFIX)/include/sdk
	cp $(BIN) $(PREFIX)/bin
	cp $(TEST_BIN) $(PREFIX)/test_bin
	cp $(LIBS) $(PREFIX)/lib
	cp src/sdk/*.h $(PREFIX)/include/sdk

.PHONY: test
test:
	echo done

