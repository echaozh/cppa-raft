CXX := clang++
CC := clang++
CXXFLAGS +=	-I $(CPPA_PATH) -I. -std=c++11 -g -stdlib=libstdc++ \
				-DCPPA_DISABLE_CONTEXT_SWITCHING -pthread
LDFLAGS += -L $(CPPA_PATH)/build/lib -lcppa -stdlib=libstdc++ -pthread

# SRCS := main raft state_machine
# OBJS := $(addsuffix .o,$(SRCS))

# cppa-raft: $(OBJS)

# $(OBS): %.o: %.cpp

TESTS := follower
TEST_PROGS := $(addprefix tests/test_,$(TESTS))

.PHONY: clean
clean:
	-rm *.o tests/*.o
	-rm cppa-raft $(TEST_PROGS)

.PHONY: tests check
tests: $(TEST_PROGS)
tests: LDFLAGS += -lgtest -lgtest_main

check: tests
	$(foreach test,$(TEST_PROGS), \
		echo $(test); LD_LIBRARY_PATH=$$CPPA_PATH/build/lib $(test);)

$(TEST_PROGS): tests/%: tests/%.o raft.o

tests/test_main.o $(addsuffix .o,$(TEST_PROGS)): tests/%.o: tests/%.cpp
