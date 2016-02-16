CXX = clang++
CXXFLAGS = -g -fPIC `root-config --cflags`
LDFLAGS = 
LDLIBS = -L.

USE_GPU = 0

#CAFFE_INCDIR = /home/taritree/software/caffe/include
CAFFE_INCDIR = /Users/twongjirad/software/caffe/include
CAFFE_LIBDIR = /Users/twongjirad/software/caffe/build/lib
CXXFLAGS += -DCPU_ONLY

#CUDA_INCDIR = /usr/local/cuda-7.5/targets/x86_64-linux/include
CUDA_INCDIR = 

#LMDB_LIBDIR = /usr/lib/x86_64-linux-gnu
#LMDB_INCDIR = /usr/include
LMDB_LIBDIR = /opt/local/lib
LMDB_INCDIR = /opt/local/include

PROTOBUF_LIBDIR = /opt/local/lib
PROTOBUF_INCDIR = /opt/local/include

OPENCV_INCDIR = /opt/local/include
OPENCV_LIBDIR = /opt/local/lib
OPENCV_LIBS = -lopencv_core
OPENCV_LIBS = $(wildcard ${OPENCV_LIBDIR}/libopencv_*)
CXXFLAGS += -DUSE_OPENCV

ROOTLIBS = `root-config --libs`

BOOST_LIBDIR = /opt/local/lib

CXXFLAGS += -I./include -I$(CAFFE_INCDIR) -I${CAFFE_INCDIR}/../.build_release/src -I$(LMDB_INCDIR) -I$(PROTOBUF_INCDIR) -I$(OPENCV_INCDIR)
ifeq (${USE_GPU},1)
  CXXFLAGS += -I$(CUDA_INCDIR)
endif
LDLIBS += $(ROOTLIBS) -L$(LMDB_LIBDIR) -llmdb -L$(PROTOBUF_LIBDIR) -lprotobuf -lglog $(OPENCV_LIBS) -L$(BOOST_LIBDIR) -lboost_system-mt
LDLIBS += -L$(CAFFE_LIBDIR)  -lcaffe
LDFLAGS = -Wl,-rpath,$(CAFFE_LIBDIR)


CCSRC = $(wildcard src/*.cc)
COBJS = $(addprefix .obj/, $(notdir $(CCSRC:.cc=.o)))
EXESRC = $(addprefix exesrc/, $(addsuffix $(EXES),.cc))
EXEOBJ = $(addprefix .obj/,$(notdir $(EXESRC:.cc=.o)))
EXEBIN = $(addprefix bin/,$(EXES))

all: libconvertroot.so bin/root2lmdb bin/lmdb2jpg

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
	$(CXX) -shared $(LDFLAGS) -o $@ $^ $(LDLIBS)
	install_name_tool -change libcaffe.dylib.1.0.0-rc3 $(CAFFE_LIBDIR)/libcaffe.dylib.1.0.0-rc3 libconvertroot.so

.obj/root2lmdb.o: exesrc/root2lmdb.cc
	$(CXX) $(CXXFLAGS) -c exesrc/root2lmdb.cc -o .obj/root2lmdb.o

bin/root2lmdb: .obj/root2lmdb.o libconvertroot.so
	@mkdir -p bin
	$(CXX) $(LDFLAGS) -o bin/root2lmdb $^ $(LDLIBS)
	install_name_tool -change libcaffe.dylib.1.0.0-rc3 $(CAFFE_LIBDIR)/libcaffe.dylib.1.0.0-rc3 bin/root2lmdb 

.obj/lmdb2jpg.o: exesrc/lmdb2jpg.cc
	$(CXX) $(CXXFLAGS) -c $^ -o $@

bin/lmdb2jpg: .obj/lmdb2jpg.o libconvertroot.so
	@mkdir -p bin
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	install_name_tool -change libcaffe.dylib.1.0.0-rc3 $(CAFFE_LIBDIR)/libcaffe.dylib.1.0.0-rc3 bin/lmdb2jpg

clean:
	@rm bin/* .obj/* src/caffe.pb.cc include/caffe.pb.h
