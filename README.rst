OBS Studio with webrtc custom

Build steps:

git clone --recursive https://github.com/wwllww/obs-studio.git

- Windows: (x64 Relase for example) 

1. down load the dependencies2017 https://obsproject.com/downloads/dependencies2017.zip

2. mkdir webrtc-core in /dependencies2017/win64/include/ and copy webrtc_core_interface.h in it.

3. mkdir Release in /dependencies2017/win64/bin and copy webrtc-core.lib in it.

4. copy srtp2.lib ssleay32.lib libeay32.lib to /dependencies2017/win64/bin

5. run cmake-gui

	add DepsPath /dependencies2017/win64

	add QTDIR   /Qt/Qt5.13.1/5.13.1/msvc2017_64


- MacOS:

1. mkdir /tmp/obsdeps/include/webrtc-core and copy webrtc_core_interface.h to it.

2. mkidr /tmp/obsdeps/lib and copy libwebrtc-core.a libsrtp2.a libssl.a libcrypto.a to it.

3. https://github.com/obsproject/obs-deps
