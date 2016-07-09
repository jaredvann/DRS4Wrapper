
# WARNING:

# if problems finding 'pyconfig.h' run following command before compilation:

# export CPLUS_INCLUDE_PATH="$CPLUS_INCLUDE_PATH:/usr/include/python2.7/"



CC = gcc
CXX = g++

CFLAGS = -Wall -fPIC -Idrs -DHAVE_LIBUSB -DHAVE_LIBUSB10 -DOS_DARWIN -L/usr/lib -L/usr/local/lib -lusb -lpython2.7 -lboost_python

CXXFLAGS = -std=c++11 -Wc++11-extensions

CPP_OBJ       = drs.o averager.o
OBJECTS       = musbstd.o mxml.o strlcpy.o


all: $(OBJECTS) $(CPP_OBJ) DRS4Wrapper.so


$(CPP_OBJ): %.o: drs/%.cpp
	$(CXX) $(CFLAGS) $(CXXFLAGS) -c $<

$(OBJECTS): %.o: drs/%.c
	$(CC) $(CFLAGS) -c $<

DRS4Wrapper.so: DRS4Wrapper.o
	$(CXX) $(CFLAGS) $(CXXFLAGS) $(OBJECTS) $(CPP_OBJ) DRS4Wrapper.o -o DRS4Wrapper.so -shared


