# CXX=gcc
# CXXOPTIMIZE= -O2
# CXXFLAGS= -g -Wall -pthread $(CXXOPTIMIZE)
# USERID=205303784_405320396_405337405
# CLASSES=

# %.o: %.c $(DEPS)
# 	$(CXX) -c -o $@ $< $(CFLAGS)

# all: server client

# server: server.o
# 	$(CXX) -o $@ $^ $(CXXFLAGS)

# client: client.o
# 	$(CXX) -o $@ $^ $(CXXFLAGS)

# clean:
# 	rm -rf *.o *~ *.gch *.swp *.dSYM server client *.tar.gz

# dist: tarball
# tarball: clean
# 	tar -cvzf /tmp/$(USERID).tar.gz --exclude=./.vagrant . && mv /tmp/$(USERID).tar.gz .

CXX=g++
CXXOPTIMIZE= -O2
CXXFLAGS= -g -Wall -pthread -std=c++11 $(CXXOPTIMIZE)
USERID=205303784_405320396_405337405
CLASSES=

%.o: %.cpp $(DEPS)
	$(CXX) -c -o $@ $< $(CFLAGS)

all: server client

server: server.o
	$(CXX) -o $@ $^ $(CXXFLAGS) 

client: client.o
	$(CXX) -o $@ $^ $(CXXFLAGS) 

clean:
	rm -rf *.o *~ *.gch *.swp *.dSYM server client *.tar.gz

dist: tarball
tarball: clean
	tar -cvzf /tmp/$(USERID).tar.gz --exclude=./.vagrant . && mv /tmp/$(USERID).tar.gz .