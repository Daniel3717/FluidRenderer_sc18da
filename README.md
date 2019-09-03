# FluidRenderer_sc18da

Most of the code is located within the playground folder, within playground.cpp and the various shaders. I have also modified some utilities a bit from their original state, the modified versions are in the common folder.

To run the project:

Download the repository and in the playground folder double-click on playground.exe. A white window should open and start loading. When loading is finished, the space skybox should be visible. In order to move/look around, press (and keep pressed) the Q key, and then move using WASD and the mouse.
You can also change shaders and their changes will affect how playground.exe works. However, changing playground.cpp will not affect playground.exe without building the project.




To build the project, steps:

These steps have been tested with Visual Studio 2017 installed and Windows 10.

1) Follow the "Building on Windows" steps from here:
http://www.opengl-tutorial.org/beginners-tutorials/tutorial-1-opening-a-window/#opening-a-window

2) Right-click on the playground project and select "Set as StartUp Project"

3) Copy the folders from github ("playground" and "common") into your "ogl-master" folder.

4) In Visual Studio, expand the "playground" project, right-click on common, and select Add -> Existing Item. Then, go to where you copied the "common" folder (in "ogl-master"), select all files and click "Add".

5) Click F5 (or Debug -> Start Debugging)

Warning! Do not try to Build All again, since the controls.cpp common has been slightly modified in a way which will cause build to fail for most projects except of "playground".

Note that you may have some output in your console (in the form of Compiling/Linking shader). It can be safely ignored. I have commented those out in my utilities, but I think it takes a while for Visual Studio to catch on about that change. In my case, when I ran it the 4-th time it caught on and the console printing ceased.
