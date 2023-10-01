cd threads
make clean
make
cd build
source ../../activate
pintos -- -q run priority-change
# pintos --gdb -- -q run priority-change
