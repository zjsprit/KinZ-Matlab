# KinZ for Matlab, A library for Azure Kinect
At the time of this writing, there are no official Matlab bindings for Azure Kinect.  
This library allows using Azure Kinect directly in Matlab.


## Installation:
1. Install the Azure Kinect SDK as described [here](https://docs.microsoft.com/en-us/azure/kinect-dk/sensor-sdk-download)  where it says Microsoft installer. Download the .exe and follow the steps.
2. For Body tracking functionality (optional) you need an NVIDIA GPU and install CUDA. Download from [here](https://developer.nvidia.com/cuda-downloads?/).
3. For Body tracking, install the Azure Kinect Body Tracking SDK. Download the msi installer from [here](https://docs.microsoft.com/en-us/azure/kinect-dk/body-sdk-download).
4. Set the compiler for Matlab as shown [here](https://www.mathworks.com/help/matlab/matlab_external/choose-c-or-c-compilers.html).
5. Open the compile_for_windows.m or compile_for_linux.m, set the corresponding paths and run. If the compilation was successful,
6. Add to the windows path environmental variable the bin directory containing the **k4a.dll** and optionally **k4abt.dll** (if compiling the body tracking SDK). For example add *C:\Program Files\Azure Kinect SDK v1.4.1\tools* to the path environmental variable. Follow the instructions described [here](https://www.architectryan.com/2018/03/17/add-to-the-path-on-windows-10/)


## Demos
Inside demos directory, you'll find demos showing all the features of the library.  
Currently, there are only 5 demos:
- **videoDemo**: shows how to get color, depth IR, and IMU sensors.
- **calibrationDemo**: shows how to extract camera calibration values.
- **pointcloudDemos**: shows how to get the colored pointcloud and visualize it.
- **bodyTrackingDemo**: shows how to get body tracking information and visualize it.

[![View KinZ-Matlab on File Exchange](https://www.mathworks.com/matlabcentral/images/matlab-file-exchange.svg)](https://www.mathworks.com/matlabcentral/fileexchange/81788-kinz-matlab)

![RGB, Depth, and Infrared](/demos/videodemo.png "Video Demo")