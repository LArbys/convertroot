#CXX = clang++
CXX = g++
CXXFLAGS = -g -fPIC `root-config --cflags`
LDFLAGS = 
LDLIBS = -L.

USE_GPU = 1

# These should be defined 
#CAFFE_INCDIR = /Users/twongjirad/software/caffe/include
#CAFFE_LIBDIR = /Users/twongjirad/software/caffe/build/lib

#LMDB_LIBDIR = /usr/lib/x86_64-linux-gnu
#LMDB_INCDIR = /usr/include
#LMDB_LIBDIR = /opt/local/lib
#LMDB_INCDIR = /opt/local/include

#PROTOBUF_LIBDIR = /opt/local/lib
#PROTOBUF_INCDIR = /opt/local/include

#OPENCV_INCDIR = /opt/local/include
#OPENCV_LIBDIR = /opt/local/lib
OPENCV_LIBS = -lopencv_core
OPENCV_LIBS = $(wildcard ${OPENCV_LIBDIR}/libopencv_*)
CXXFLAGS += -DUSE_OPENCV

ROOTLIBS = `root-config --libs`

#BOOST_LIBDIR = /opt/local/lib

CXXFLAGS += -I./include -I$(CAFFE_INCDIR) -I${CAFFE_INCDIR}/../.build_release/src -I$(LMDB_INCDIR) -I$(PROTOBUF_INCDIR) -I$(OPENCV_INCDIR)
ifeq (${USE_GPU},1)
  CXXFLAGS += -I$(CUDA_INCDIR)
else
  CXXFLAGS += -DCPU_ONLY
endif

ifeq (`uname`,'Darwin')
  LDLIBS += -L$(BOOST_LIBDIR) -lboost_system-mt
  LDLIBS += -Wl,-rpath,$(CAFFE_LIBDIR) 
  LDLIBS += -Wl,-rpath,$(PWD)
  LIBFLAGS += -shared -install_name '@rpath/libconvertroot.so'
else
  LDLIBS += -L$(BOOST_LIBDIR) -lboost_system
  LIBFLAGS += -shared
endif
LDLIBS += -L$(CAFFE_LIBDIR)  -lcaffe
LDLIBS += $(ROOTLIBS) -L$(LMDB_LIBDIR) -llmdb -lleveldb -L$(PROTOBUF_LIBDIR) -lprotobuf -lglog $(OPENCV_LIBS)

CCSRC = $(wildcard src/*.cc)
COBJS = $(addprefix .obj/, $(notdir $(CCSRC:.cc=.o)))
#EXESRC = $(addprefix exesrc/, $(nodir $(EXES)))
EXESRC = $(wildcard exesrc/*.cc)
EXEOBJ = $(addprefix .obj/,$(notdir $(EXESRC:.cc=.o)))
EXEBIN = $(addprefix bin/,$(notdir $(EXESRC:.cc=)))

all: libconvertroot.so ${EXEBIN}

caffe.pb.o:
	@rm -f src/caffe.pb.cc include/caffe.pb.h
	protoc --proto_path=${CAFFE_INCDIR}/../src/caffe/proto --cpp_out=. ${CAFFE_INCDIR}/../src/caffe/proto/caffe.proto
	@mv caffe.pb.cc src/
	@mv caffe.pb.h include/
	$(CXX) $(CXXFLAGS) -c src/caffe.pb.cc -o ./obj/caffe.pb.o

.obj/%.o: src/%.cc
	@mkdir -p .obj
	$(CXX) -c $(CXXFLAGS) -o $@ $^

libconvertroot.so: $(COBJS)
	$(CXX) $(LIBFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

#.obj/root2lmdb.o: exesrc/root2lmdb.cc
#	$(CXX) $(CXXFLAGS) -c exesrc/root2lmdb.cc -o .obj/root2lmdb.o
#
#bin/root2lmdb: .obj/root2lmdb.o libconvertroot.so
#	@mkdir -p bin
#	$(CXX) $(LDFLAGS) -o bin/root2lmdb $^ $(LDLIBS)
#
#.obj/bnb_root2lmdb.o: exesrc/bnb_root2lmdb.cc
#	$(CXX) $(CXXFLAGS) -c exesrc/bnb_root2lmdb.cc -o .obj/bnb_root2lmdb.o
#
#bin/bnb_root2lmdb: .obj/bnb_root2lmdb.o libconvertroot.so
#	@mkdir -p bin
#	$(CXX) $(LDFLAGS) -o bin/bnb_root2lmdb $^ $(LDLIBS)
#
#.obj/data_root2lmdb.o: exesrc/data_root2lmdb.cc
#	$(CXX) $(CXXFLAGS) -c exesrc/data_root2lmdb.cc -o .obj/data_root2lmdb.o
#
#bin/data_root2lmdb: .obj/data_root2lmdb.o libconvertroot.so
#	@mkdir -p bin
#	$(CXX) $(LDFLAGS) -o bin/data_root2lmdb $^ $(LDLIBS)
#
#.obj/lmdb2jpg.o: exesrc/lmdb2jpg.cc
#	$(CXX) $(CXXFLAGS) -c $^ -o $@
#
#bin/lmdb2jpg: .obj/lmdb2jpg.o libconvertroot.so
#	@mkdir -p bin
#	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

#.obj/tridata_lmdb2jpg.o: exesrc/tridata_lmdb2jpg.cc
#	$(CXX) $(CXXFLAGS) -c $^ -o $@

#bin/tridata_lmdb2jpg: .obj/tridata_lmdb2jpg.o libconvertroot.so
#	@mkdir -p bin
#	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)
#	install_name_tool -change libcaffe.so.1.0.0-rc3 $(CAFFE_LIBDIR)/libcaffe.so.1.0.0-rc3 $@

.obj/%.o: exesrc/%.cc
	$(CXX) $(CXXFLAGS) -c $^ -o $@

bin/%: .obj/%.o libconvertroot.so
	@mkdir -p bin
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

clean:
	@rm bin/* .obj/*
