
for /r %%i in (*.slang) do slangc %%i -profile glsl_460 -target spirv -o %%i.spv -entry main -warnings-disable 39001
