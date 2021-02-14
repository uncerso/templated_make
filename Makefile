FLAGS:=-Werror -pedantic-errors -Wall -Wextra -Wpedantic -Wcast-align -Wcast-qual -Wconversion -Wctor-dtor-privacy -Wduplicated-branches -Wduplicated-cond -Wextra-semi -Wfloat-equal -Wlogical-op -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual -Wredundant-decls -Wsign-conversion -Wsign-promo
COMMON_FLAGS:=-std=c++20
OUT_NAME:=tmake
FILES:=src/*.cpp

bulid:
	g++ -O3 $(COMMON_FLAGS) $(FLAGS) $(FILES) -o $(OUT_NAME)
debug:
	g++ -g -no-pie $(COMMON_FLAGS) $(FILES) -o $(OUT_NAME)
