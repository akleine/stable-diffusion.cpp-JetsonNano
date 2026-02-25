## Prerequisites

There are some very useful texts out there on the net. Most of them concern _llama.cpp_, but they are very useful for us.
One of them is  https://medium.com/@anuragdogra2192/llama-cpp-on-nvidia-jetson-nano-a-complete-guide-fb178530bc35

#### Ubuntu 20.04 OS 

Get the basic Ubuntu OS image here:
https://github.com/Qengineering/Jetson-Nano-image
Many thanks for this excellent work!

#### GCC 8.5
Some steps for deployment are:
```
wget http://ftp.gnu.org/gnu/gcc/gcc-8.5.0/gcc-8.5.0.tar.gz
tar -xvzf gcc-8.5.0.tar.gz
./contrib/download_prerequisites
mkdir build && cd build
../configure --enable-languages=c,c++ --disable-multilib
make -j$(nproc) 
sudo make install
sudo update-alternatives --install /usr/bin/gcc gcc /usr/local/bin/gcc-8.5 100                                                                                                            
sudo update-alternatives --install /usr/bin/g++ g++ /usr/local/bin/g++-8.5 100                                                                                                            
sudo update-alternatives --config gcc                                                                                                                                                     
sudo update-alternatives --config g++ 
```

#### CMAKE
Some steps for deployment are:
```
wget https://github.com/Kitware/CMake/releases/download/v3.31.6/cmake-3.31.6-linux-aarch64.sh                                                                                                 
sudo sh cmake-3.31.6-linux-aarch64.sh  --prefix=/opt/cmake                                                                                                                                    
sudo ln -s /opt/cmake/cmake-3.31.6-linux-aarch64/bin/cmake /usr/local/bin/cmake  
```


***
## Some environment details
#### uname -a
```
Linux nano 4.9.253-tegra #1 SMP PREEMPT Mon Jul 26 12:13:06 PDT 2021 aarch64 aarch64 aarch64 GNU/Linux
```
#### cat os-release 
```
NAME="Ubuntu"
VERSION="20.04.6 LTS (Focal Fossa)"
ID=ubuntu
ID_LIKE=debian
PRETTY_NAME="Ubuntu 20.04.6 LTS"
VERSION_ID="20.04"
HOME_URL="https://www.ubuntu.com/"
SUPPORT_URL="https://help.ubuntu.com/"
BUG_REPORT_URL="https://bugs.launchpad.net/ubuntu/"
PRIVACY_POLICY_URL="https://www.ubuntu.com/legal/terms-and-policies/privacy-policy"
VERSION_CODENAME=focal
UBUNTU_CODENAME=focal
```
#### gcc -v
```
Using built-in specs.
COLLECT_GCC=gcc
COLLECT_LTO_WRAPPER=/usr/local/libexec/gcc/aarch64-unknown-linux-gnu/8.5.0/lto-wrapper
Target: aarch64-unknown-linux-gnu
Configured with: ../configure --enable-languages=c,c++ --disable-multilib
Thread model: posix
gcc version 8.5.0 (GCC) 
```
#### cmake --version
```
cmake version 3.31.6
CMake suite maintained and supported by Kitware (kitware.com/cmake).
```
#### nvcc --version
```
nvcc: NVIDIA (R) Cuda compiler driver
Copyright (c) 2005-2021 NVIDIA Corporation
Built on Sun_Feb_28_22:34:44_PST_2021
Cuda compilation tools, release 10.2, V10.2.300
Build cuda_10.2_r440.TC440_70.29663091_0
```

