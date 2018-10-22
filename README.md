# UnityOptitrackNativePlugin
Native plugin for Unity which sends camera frames captured by OptiTrack Flex 13

# Environment Variables
* ```%opencv%```: OpenCV library path e.g. ```C:\opencv343```
* ```%NP_CAMERASDK%: Automatically added as you have installed Camera SDK e.g. ```C:\Program Files (x86)\OptiTrack\Camera SDK\```
* ```%iiiEx2018UnityProject%: Unity project directory for deploying the dll file after the building process e.g. ```C:\Users\administrator\OptitrackCameraCapture```

# Dependances
* Camera SDK 2.1 Beta 2 https://optitrack.com/downloads/developer-tools.html
* Open CV 3.4.3

# Development Environment
* Windows 10 (64 bit) + VisualStudio 2017
* Unity 2018.2.12f1

# Setup
1. Specify the Unity project directory using an environment variable named ```%iiiEx2018UnityProject%```.
2. Create a folder inside ```%iiiEx2018UnityProject%\Assets\Plugins\x86_64\```.
3. Specify the OpenCV library path using an environment variable named ```%opencv%```.
4. Download Camera SDK from https://optitrack.com/downloads/developer-tools.html and install it.
4. Build this solution.
5. Open the Unity project to try the complites.

# References
http://tips.hecomi.com/entry/2016/01/10/173437
