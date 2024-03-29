# Filters - An Application for Interactive File Filtering
This application allows the the filtering of a text file, by the sequential 
application of regular expressions.

The application can be run in interactive mode, presenting a UI with a list 
of regular expressions, and the final results of the applied filters. It can
also be run in non-interactive "batch" mode, applying a filter set saved from
a previous interactive session to an input file.

## GUI mode
When started, the program will
present a screen, with two main divisions. On the top is a grid of expressions 
to be applied to the input file. On the bottom is the resultant text output
from the application of the filters to the input.

For each (enabled) RE string in the filter list, the RE is applied to the input,
and the result passed to the next RE. The expressions are evaluated in order,
from top to bottom. Each step is cached, so changing a later expression only
evaluates from that point down. 

Each expression line has three check boxes, and an expression string. The check
boxes are labeled "En", "Ex", and "IC".

* "En" is the enable option. Toggling this check mark will re-run the evaluation
sequence from that point down. Any disabled expression (where "En" check is 
cleared), will pass through the input of that step unchanged to the next step.

* "Ex" is the "exclude" flag. By default, this flag is cleared, and only inputs
to this step which *match* the RE will pass through to the next step. When this
flag is set, the expression is an "exclusion"; any input for which the RE 
matches will be *excluded* from the output.

* "IC" uses case insensitive matching in the regular expression for that step

* The "Regular Expression" field is the RE string for the step. The syntax is 
checked on entry, according to the "Dialect" option under the "Filters" menu. 
Currently, only QRegularExpression (PCRE) dialect is supported.

**NOTE** Since application of the regular expressions on very large files can be
very time consuming, "Auto Run" is disabled by default. After adding, deleting,
or changing an expression, the "Run" command must be issued. As a best practice,
you will want to put the most exclusive expressions at the top. This will make
subsequent expressions faster. If the "Auto Run" option is enabled, any changes
in the expression list are evaluated immediately.

### Subject files
##### From file
The "File"->"Open" and "File"->"Open Recent" commands will load (replace) the
subject source with the contents of a local or remote file.

##### From clipboard
"File"->"Load from clipboard" will replace load (replace) the subject source 
with the contents of the system clipboard, if the clipboard contents are text,
or convertible to text. If the clipboard does not contain any text, an 
informational dialog will pop-up, and the original subject will remain
unchanged.

## Batch Mode
The application can be run in batch mode, to run a saved RE file against a
subject file from a command line or script. It is invoked by using the
"--batch" option. When started in batch mode, the GUI is not opened.

When starting in batch mode, "--refile" and "--subject" parameters must also
be specified. The RE file(s) will be loaded and verified. The subject file
will then be loaded, the filters applied, and the result printed to stdout.

# Building
#### Prerequisites
You need Qt5, KDE Frameworks 5, and CMake 2.8.11 or higher

#### Getting the source
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

#### Configure and make:

The application can be configured to install to the user's bin path (i.e. 
`~/bin/`), or system wide (`/usr/local/…` or `/usr/…` by defining the
`-DCMAKE_INSTALL_PREFIX=…` parameter. If not specified, it will use the install prefix
as determined by the GNUInstallDirs package.

**NOTE** Commonly, locally built and installed apps will go into 
`/opt/…` or `/usr/local/…`; with `/usr/…` reserved for applications managed by the
 system package manager.

	BUILD_TYPE := Debug|RelWithDebInfo|Release
	INSTALL := install base path, i.e. "/usr" | "/usr/local" | "~/"
	CMAKE-OPTIONS:
		-DCMAKE_VERBOSE_MAKEFILE:BOOL=ON
                -DCMAKE_UNITY_BUILD:BOOL=ON

    BUILDOPTIONS:
        --clean-first
        --parallel N        : Use N concurrent threads
        --preset PRESET
        --target TARGET     : build TARGET (may be multiple) instead of default
        --verbose

Application specific options can be enabled using the -DOPT=TRUE option, where OPT is:

| OPT | Description |
| --- | ----------- |
| WITH_LTO | Enable *link-time optimization*, aka, *inter-procedural optimization* |
| WITH_ASAN | Enable *address sanitizer* |
| WITH_UBSAN | Enable *undefined behavior sanitizer* |

For example, to enable LTO and ASAN: `cmake .. -DWITH_ASAN=TRUE -DWITH_LTO=TRUE`.

Note: Using ASAN may require pre-loading the libasan.so library:
  ex: LD_PRELOAD=/usr/lib64/libasan.so.8 ./filters

Note: with ASAN: At runtime set environment variable
  "ASAN_OPTIONS=detect_stack_use_after_return=1"

You can override the default make file generator with the ```-G``` GENERATOR, where
GENERATOR is i.e. 'Unix Makefiles' | 'Ninja' | ...

To use an alternate tool chain from the default, specify a tool chain file on with the
`--toolchain=TOOL_FILE` option. Several tool-chain files are located in
`./cmake/tools_XXX.cmake`.

```shell
cmake [CMAKE-OPTIONS] [--toolchain=TOOL_FILE] -DCMAKE_BUILD_TYPE=BUILD_TYPE [-DCMAKE_INSTALL_PREFIX=INSTALL] [-G GENERATOR] ../
cmake --build . [BUILDOPTIONS]
```

ex:
```shell
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local -G ninja
```

#### Run in build directory:
The application can be run without installing, for instance, for debugging:

```shell
./filters
```
*NOTE*
To run from the build directory, the application UI .rc file must be copied 
into the kxmlgui5 path, i.e.:

```shell
sudo cp ../src/APPui.rc /usr/local/share/kxmlgui5/filters/
```

#### To install in your private directory:

```shell
cmake --install .
```

#### To install in system directory:
Use this option to make the application available to all users in the system.

**NOTE** Commonly, locally built and installed apps will go into 
`/usr/local/…`; with `/usr/…` reserved for applications managed by the
 system package manager.


```shell
sudo cmake --install .
```

## Todo
* move the filter chain out of the event loop.
