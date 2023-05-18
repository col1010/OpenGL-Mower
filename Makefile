
all: 
	g++ -I imgui imgui/*.o tiny_obj_loader.cc main.cpp -lGL -lglfw -lGLEW -lSOIL
debug:
	g++ -I imgui imgui/*.o tiny_obj_loader.cc main.cpp -lGL -lglfw -lGLEW -lSOIL -g
clean:
	rm a.out

test:	all
	./a.out
