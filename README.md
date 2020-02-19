# Aart
Love ascii-art? Me too! This utility converts images and video to beautiful and colorful ascii art.
No explanations, just look at the example. I used default 16-color palette from cmd.exe

![Image](aart/test.jpg) 
![Image](aart/ascii-art.png) 

It's simple but beautiful. You can convert even videofiles.
[This is](https://youtu.be/HAmjZi_CUzo) sample video converted with custom palette (not the best one).

Here is command-line syntax
```
Usage: Aart [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  --chr,--charmap TEXT:FILE   Path to the character map
  --clr,--colormap TEXT:FILE  Path to the color map
  -i,--input TEXT:FILE        Path to the input file
  -o,--output TEXT            Path to the output file
  --img{0},--vid{1}           Conversion mode [--img] for images, [--vid] for videos, [--img] if not specified
  --cuda,--no-cuda{false}     Use CUDA GPU acceleration (if possible). Better boost can be seen on videos, [--no-cuda] if not specified
  --cie94,--no-cie94{false}   Use more precise but more expensive algorithm, use default if not specified
```
where `charmap` is path to the main palette, `colormap` is path to the color palette, `input` and `output` are path to the input file and generated ascii-art, .

Aart includes sample palette and mediafiles that were used for testing.

# Recommendations and known problems
Default palette works best at bright and colorful images. If you want to get better results - find more suitable colors or just increase their number.
But larger palettes kills the charm of the resulting art.

When working with videofiles, there can be problems with video encoders/decoders. Aart uses OpenCV as a backend.
In my machine OpenCV writes about random errors with codecs. Nevertheless, conversion is successful.

# Build
Aart depends on OpenCV. You can provede headers and `.lib`s by yourself or use vcpkg.
If using vcpkg, then type the following commad:
```vcpkg install opencv4[ffmpeg]:x64-windows```

Aart works currently on Windows only. But you can easily port it to any platform that supports OpenCV - just create CMake project
or build VS solution with linux as target OS.
