# Common build rules.
#
# Don't invoke this file directly. It is meant to be included in other files.

BINARIES=$(BIN)play $(BIN)analyze

COMMON_HDRS=\
	$(SRC)analysis.h \
	$(SRC)codec.h \
	$(SRC)logging.h \
	$(SRC)options.h \
	$(SRC)pieces.h \
	$(SRC)pns.h \
	$(SRC)random.h \
	$(SRC)state.h \
	$(SRC)transcript.h \
	$(SRC)weights.h

COMMON_SRCS=\
	$(SRC)analysis.cc \
	$(SRC)codec.cc \
	$(SRC)random.cc \
	$(SRC)state.cc \
	$(SRC)transcript.cc \
	$(SRC)weights.cc

COMMON_OBJS=\
	$(OBJ)analysis.o \
	$(OBJ)codec.o \
	$(OBJ)options.o \
	$(OBJ)random.o \
	$(OBJ)state.o \
	$(OBJ)transcript.o \
	$(OBJ)weights.o

PLAY_OBJS=$(OBJ)play.o $(COMMON_OBJS)
ANALYZE_OBJS=$(OBJ)analyze.o $(COMMON_OBJS)

# Note that headers must be included in dependency order.
COMBINED_SRCS=\
	$(SRC)options.h $(SRC)options.cc \
	$(SRC)random.h $(SRC)random.cc \
	$(SRC)pieces.h \
	$(SRC)weights.h $(SRC)weights.cc \
	$(SRC)state.h $(SRC)state.cc \
	$(SRC)logging.h \
    $(SRC)pns.h \
	$(SRC)analysis.h $(SRC)analysis.cc \
	$(SRC)play.cc

all: $(BINARIES)

$(OBJ)analysis.o: $(SRC)analysis.cc $(SRC)analysis.h $(SRC)options.h $(SRC)pns.h $(SRC)random.h $(SRC)pieces.h $(SRC)state.h $(SRC)weights.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ)codec.o: $(SRC)codec.cc $(SRC)codec.h $(SRC)state.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ)options.o: $(SRC)options.cc $(SRC)options.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ)random.o: $(SRC)random.cc $(SRC)random.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ)state.o: $(SRC)state.cc $(SRC)state.h $(SRC)pieces.h $(SRC)random.h $(SRC)weights.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ)transcript.o: $(SRC)transcript.cc $(SRC)state.h $(SRC)codec.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ)weights.o: $(SRC)weights.cc $(SRC)weights.h $(SRC)options.h $(SRC)pieces.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ)play.o: $(SRC)play.cc $(COMMON_HDRS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ)analyze.o: $(SRC)analyze.cc $(COMMON_HDRS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BIN)play: $(PLAY_OBJS)
	$(CXX) $(CXXFLAGS) $(PLAY_OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

$(BIN)analyze: $(ANALYZE_OBJS)
	$(CXX) $(CXXFLAGS) $(ANALYZE_OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

$(OUT)combined-player.cc: $(COMBINED_SRCS) combine-sources.sh
	./combine-sources.sh $(COMBINED_SRCS) > $@

$(BIN)combined-player: $(OUT)combined-player.cc
	$(CXX) $(CXXFLAGS) -ULOCAL_BUILD -o $@ $<  $(LDFLAGS) $(LDLIBS)

play: $(BIN)play

analyze: $(BIN)analyze

combined: $(BIN)combined-player

clean:
	rm -f $(BINARIES) $(OBJ)*.o $(OUT)combined-player.cc $(BIN)combined-player

.DELETE_ON_ERROR:

.PHONY: all clean play analyze combined
