# WilloRHI
It's just another Vulkan wrapper, nothing special going on here

Very heavily inspired by https://github.com/Ipotrick/Daxa - namely the bindless interface to shader resource access, etc.

I'm mainly doing this to try elevate my knowledge of Vulkan and general graphics programming concepts.

## Building
You will need to have the Vulkan SDK and CMake installed, every other dependency will be downloaded automagically.
```
git clone https://github.com/Willoguns01/WilloRHI.git
cd WilloRHI
mkdir build
cd build
cmake ..
cmake --build . --config Release
```
