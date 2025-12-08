# Supported Operating Systems


ld-decode and the various included tools are designed for (and tested on) Ubuntu 22.04 LTS (20.04 LTS no longer works). 

It is possible to compile and run ld-decode in other Linux environments however this is not regularly tested by the projects. But do read the [Linux Compatability Doc](https://docs.google.com/document/d/132ycIMMNvdKvrNZSzbckXVEPQVLTnH_YX0Oh3lqtkkQ/edit).

For first-time users using a self contained combined build will be the most simple, just like installing or deploying any outher piece of portable software this maintained from the [vhs-decode repository](https://github.com/oyvindln/vhs-decode/releases) witch also contains hifi-decode and a cross code developed composite decoder cvbs-decode.


### Self Contained Builds


- [Linux Builds](https://github.com/oyvindln/vhs-decode/wiki/Linux-Build)

- [Windows Builds](https://github.com/oyvindln/vhs-decode/wiki/Windows-Build)

- [MacOS Builds](https://github.com/oyvindln/vhs-decode/wiki/MacOS-Build)


# System requirements


ld-decode performs complex mathematics on huge datasets and therefore requires a fairly high-end PC for any expedient use, with AVX2 support notably helpful.  

A Haswell (or newer) i9/i7 or Ryzen with 16-64Gb of RAM and 2TB of soild state & 8TB of hard-drive storage is recommended, however the minimum requirements are a Sandy Bridge i5 with 8Gb RAM and 512Gb of hard-drive.

Blu-Ray BDXL Optical discs 100-128GB (M-Disc/DataLifePlus) and LTO5 tapes can be recommended as relatively affordable long term archival storage formats. 

Decoding in simple terms is single core bias, so faster higher speed intergrated CPUs like those found in the Apple M1 Max, and AMDs x3D line and newer are today's fastest chips, the decoders today wont be more efficient past 6 threads. (excluding the chroma-decoder and hifi-decode)


# Dedicated Install


For dedicated stations it is *highly* recommended that you install the recommended environment; if a bare-metal installation is not available, you can use tools such as virtualbox or VMware to install ld-decode in a virtual machine or container.


There is also the project combined builds found in the 


# Pre-installation


Before attempting to directly install and deploy ld-decode ensure you have a sane (preferably fresh) Ubuntu installation that is up to date.  

Use the following commands to ensure you have the latest software packages:

    sudo apt update; sudo apt upgrade

`SoX` can also be quite useful to install for use with scripts and secondary tools. 

    sudo apt install sox

[Windows SoX Install](https://sourceforge.net/projects/sox/files/sox/14.4.2/)

Install [tbc-video-export](https://github.com/JuniorIsAJitterbug/tbc-video-export) for a quicker export experience.

    pipx install tbc-video-export

To update uninstall and re-install, there is also self-contained binaries for this tool.


# Installation


## Ubuntu 22.04 LTS


To install ld-decode's associated tools enter the following commands into a command terminal in the order shown (you will need root access to the machine via the sudo command to perform the installation):

(note: previously this apt line included Python packages, but using system Python installs is now deprecated.)

    sudo apt install git qt5-qmake qtbase5-dev libqwt-qt5-dev libqt5svg5-dev libfftw3-dev python3-tk libavformat-dev libavcodec-dev libavutil-dev ffmpeg openssl pv pkg-config cmake make

Download the soruce repository

    git clone https://github.com/happycube/ld-decode ld-decode

-----

    git submodule update --init

Configure cmake for building

    cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo .`

----- 

    make

-----

    sudo make install


The `make` stage will take the most time. You can speed this up considerably by giving a `-j` argument to parallelise the build (e.g. `make -j8` to use 8 CPU cores).


## Ubuntu 25.04


First install dependencies 

    sudo apt install git qt5-qmake qtbase5-dev libqwt-qt5-dev libqt5svg5-dev libfftw3-dev python3-tk libavformat-dev libavcodec-dev libavutil-dev ffmpeg openssl pv pkg-config cmake make python3-setuptools

Go to the directory you wish to insall ld-decode into normally username/home

    git clone https://github.com/happycube/ld-decode ld-decode

-----

    git submodule update --init

-----

    cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo .`

----- 

    make

-----

    sudo make install


## Python virtualenv installation


To run ld-decode itself, you need to create a virtualenv (or similar) to run Python and ld-decode's dependancies.

Once you have a Python you want to use, run:


    python -mvenv $VIRTUALENV_DIR # ./ if a local copy`

-----

    source $VIRTUALENV_DIR/bin/activate # or windows equivalent`

-----

    install numpy numba scipy

Then go to your ld-decode directory, use source when needed, then run ld-decode normally.

## Fedora 43 installation

### Install required DNF packages
```
sudo dnf install -y \
  qt5-qtbase-devel \
  qt5-qtsvg-devel \
  qwt-qt5-devel \
  fftw-devel \
  python3-tkinter \
  ffmpeg \
  ffmpeg-free-devel \
  libavformat-free-devel \
  libavcodec-free-devel \
  libavutil-free-devel \
  openssl-devel \
  pv \
  pkgconf-pkg-config \
  cmake \
  make \
  python3-setuptools
```

### Install required Python version (3.12)
```
sudo dnf install python3.12 python3.12-devel python3.12-libs
```

### Set up a Python virtual environment

#### Set VDIR to your target directory
```
export VDIR=~/venvs/ld-decode-venv
```

#### Create a Python 3.12 venv
```
python3.12 -m venv "$VDIR"
source "$VDIR/bin/activate"
python -m pip install -U pip setuptools wheel
```

#### Clone ld-decode and install ld-decode into the venv
```
cd $VDIR
git clone https://github.com/happycube/ld-decode.git
cd ld-decode
git submodule update --init
pip install -e .
pip install numba numpy scipy
````

### Build ld-decode
```
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo .
make -j8
sudo make install
```

### Notes
To leave the venv type the following command:
```
deactivate
```

You will need to join the virtual environment before running any python-based ld-decode applications:
```
export VDIR=~/venvs/ld-decode-venv
source "$VDIR/bin/activate"
```


# Post-installation


For basic use instructions please see the [basic usage](../How-to-guides/Basic-usage-of-ld-decode.md) instructions on this wiki.  For more advanced topics please  browse the wiki contents using the navigation pane on the right of this page.


# Upgrading the existing installation


In order to upgrade your source tree to the latest code from the 'master' branch, issue the following commands:

Go into install

     cd ~/ld-decode

Pull current changes

    git pull

You can then compile and install using the same commands as above:

    cmake .

-----

    make

-----

    sudo make install

There is usually no need to explicitly delete any existing files when upgrading - `make` will work out what needs to be rebuilt automatically. If you find that ld-decode does fail to compile after an upgrade, though, you should first try the command:

    make clean

to remove the compiled files, then repeat the compilation commands above.


# Development branches


The installation above will install the 'master' github branch.  This reflects the latest version of the code that's being actively worked on with new features and bug fixes. While the development version of ld-decode usually works well for most purposes, you should be aware that it may contain new bugs as well - if you find problems with it, please let us know!

If you'd like a version of ld-decode that should definitely work - and especially if you're packaging ld-decode for a distribution - then you should use one of the numbered releases instead. We provide a new major release (with new features) about once a year, with occasional minor releases (with just bug fixes).

New features are often developed on separate development branches. If you wish to use a development branch (and you know what it is for and how to handle active software development), you can clone it with a command such as the following:

     git clone --single-branch --branch great-fix-dev https://github.com/happycube/ld-decode

This example command will clone the great-fix-dev branch onto your local machine.
