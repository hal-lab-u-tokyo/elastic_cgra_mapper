
if [ ! -d "build" ]; then
    mkdir build
fi
cd build
cmake -GNinja ..
ninja

cd ..
