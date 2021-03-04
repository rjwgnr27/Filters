# Filters
KDE application for interactive file filtering

# Building
## Prerequisites
You need Qt5, KDE Frameworks 5, and CMake 2.8.11 or higher

## Getting the source
"SOURCE_BASE" is the base directory for your project builds, i.e. "~/source":

```shell
cd SOURCE_BASE
git clone git@github.com:rjwgnr27/Filters.git
cd Filters
```

Create the out-of-source build directory:
```shell
mkdir build
cd build
```

Configure and make:

Where:

	BUILD_TYPE := Debug|RelWithDebInfo|Release
	INSTALL := install base path, i.e. "/usr" | "/usr/local"
	CMAKE-OPTIONS:
		-DCMAKE_VERBOSE_MAKEFILE:BOOL=ON

```shell
cmake [CMAKE-OPTIONS] -DCMAKE_BUILD_TYPE=BUILD_TYPE -DCMAKE_INSTALL_PREFIX=INSTALL ../
make
```

To run local build:
```shell
./filters
```

-- NOTE --
To run locally, the application UI .rc file must be copied into the kxmlgui5 path, i.e.:

```shell
sudo cp ../src/APPui.rc /usr/local/share/kxmlgui5/filters/
```

To install in user directory:
```shell
make install/fast
```

To install in system directory:
```shell
sudo make install/fast
```
