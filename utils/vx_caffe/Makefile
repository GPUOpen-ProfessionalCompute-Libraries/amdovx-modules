default: out/vx_caffe

proto/caffe.proto:
	@mkdir -p proto
	@mkdir -p out
	@mkdir -p out/weights
	@mkdir -p out/bias
	wget -q -O proto/caffe.proto https://raw.githubusercontent.com/BVLC/caffe/master/src/caffe/proto/caffe.proto

proto/caffe.pb.cc proto/caffe.pb.h: proto/caffe.proto
	protoc --cpp_out=. proto/caffe.proto

out/vx_caffe: proto/caffe.pb.cc proto/caffe.pb.h
	$(CXX) $(FLAGS) --std=c++11 -I. -Iout src/vx_caffe.cpp proto/caffe.pb.cc `pkg-config --cflags --libs protobuf` -o out/vx_caffe
	$(CXX) $(FLAGS) --std=c++11 -I. -Iout src/bin2proto.cpp proto/caffe.pb.cc `pkg-config --cflags --libs protobuf` -o out/bin2proto
	$(CXX) $(FLAGS) --std=c++11 -I. -Iout src/proto2bin.cpp proto/caffe.pb.cc `pkg-config --cflags --libs protobuf` -o out/proto2bin

clean:
	$(RM) -r out
	$(RM) -r proto
