COMMON_SRC=../src/ognconv.cpp ../src/format.cpp ../src/intmath.cpp ../src/nmea.cpp ../src/bitcount.cpp
CXXFLAGS=-std=c++11 -Wall -Wno-misleading-indentation -O2 -ffast-math -I../src

all: lookout_sim lookout_case

lookout_sim: lookout_sim.cc
	g++ $(CXXFLAGS) -o lookout_sim lookout_sim.cc $(COMMON_SRC)

lookout_case: lookout_case.cc
	g++ $(CXXFLAGS) -o lookout_case lookout_case.cc $(COMMON_SRC)
