# WilloRHI
It's just another Vulkan wrapper, nothing special going on here

Very heavily inspired by https://github.com/Ipotrick/Daxa - namely the bindless interface to shader resource access, etc. 

Okay it's not really even "inspired", I'm using a bunch of descriptor arrays for each resource type in a single descriptor set, making all resource access bindless, as each resource is indexed from shaders with a uint.\
Other projects such as Daxa, Vuk (https://github.com/martty/vuk), etc. have been used as extra resources alongside Vulkan documentation when I've gotten particularly stuck. Thanks guys!

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
