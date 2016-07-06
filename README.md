# DRS4Wrapper
Python (plus C++) wrapper for the DRS4 Digitiser by PSI


### INSTALLATION NOTES

	- tested on OSX10.11
	- recommended to install boost and libusb via homebrew (http://brew.sh/)
	- uses default OSX python 2.7 install
	- unlikely to work with other python versions/installs in other locations


### RECOMMENDED INSTALLATION STEPS

	- brew install --with-c++11 --build-from-source boost
	- brew install python-boost libusb libusb-compat


### COMPILATION STEPS
	- export CPLUS_INCLUDE_PATH="$CPLUS_INCLUDE_PATH:/usr/include/python2.7/"
	- make
	- python test.py
