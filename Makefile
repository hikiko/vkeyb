src = $(wildcard src/*.cc)
obj = $(src:.cc=.o)
bin = vkeyb


dbg = -g
opt = -O3

CXX = g++
CXXFLAGS = -pedantic -Wall $(dbg) $(opt)
LDFLAGS = -lGL -lGLU -lX11 -limago -lglut \
		  -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_video

$(bin): $(obj)
	$(CXX) -o $@ $(obj) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) $(bin)
