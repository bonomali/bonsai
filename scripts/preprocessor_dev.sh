#! /bin/bash

. scripts/preamble.sh

if [ "$Platform" == "Linux" ] ; then

  DEBUGGER=$1

  DEBUGFLAG="--do-debug-window"

  $DEBUGGER bin/preprocessor_dev         \
    $DEBUGFLAG                           \
    -D BONSAI_PREPROCESSOR               \
    -D BONSAI_LINUX                      \
    -D __x86_64__                        \
    src/poof/preprocessor.cpp \
    -I src                               \
    -I /usr/include                      \
    -I /usr/include/x86_64-linux-gnu     \
    -I /usr/include/clang/6.0.0/include 

    # -I /usr/include/c++/6 # NOTE(Jesse): This is where <random>, <algorithm> and <numeric> live.  Rewrite perlin.h

elif [ "$Platform" == "Windows" ] ; then

    # src/poof/preprocessor.cpp                                                                         \
    # src/bonsai_types.h                                                                                           \

    # -D BONSAI_WIN32                                                                                              \
    # -D BONSAI_LINUX                                                                                              \

  LOGLEVEL=LogLevel_Debug
  # LOGLEVEL=LogLevel_Info
  # LOGLEVEL=LogLevel_Error


#   bin/tests/preprocessor \
#     --log-level LogLevel_Error

  # DEBUGFLAG="--do-debug-window"

   bin/preprocessor_dev                                                                                            \
     --log-level $LOGLEVEL                                                                                         \
     $DEBUGFLAG                                                                                                    \
     $COLORFLAG                                                                                                    \
     -D BONSAI_PREPROCESSOR                                                                                        \
     -D BONSAI_WIN32                                                                                               \
     src/poof/preprocessor.cpp                                                                          \
     -I ./src                                                                                                      \
     -I "C:/Program Files/LLVM/lib/clang/11.0.0/include"                                                           \
     -I "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Tools/MSVC/14.28.29333/include"          \
     -I "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Tools/MSVC/14.28.29333/atlmfc/include"   \
     -I "C:/Program Files (x86)/Windows Kits/10/Include/10.0.18362.0/ucrt"                                         \
     -I "C:/Program Files (x86)/Windows Kits/10/include/10.0.18362.0/shared"                                       \
     -I "C:/Program Files (x86)/Windows Kits/10/include/10.0.18362.0/um"                                           \
     -I "C:/Program Files (x86)/Windows Kits/10/include/10.0.18362.0/winrt"

#    bin/preprocessor_dev                                                                                          \
#      --log-level $LOGLEVEL                                                                                       \
#    $DEBUGFLAG                                                                                                    \
#      $COLORFLAG                                                                                                  \
#      -D BONSAI_PREPROCESSOR                                                                                      \
#      -D BONSAI_LINUX                                                                                             \
#      src/poof/preprocessor.cpp                                                                        \
#      -I ./src                                                                                                    \
#      -I "C:/Program Files/LLVM/lib/clang/11.0.0/include"                                                         \
#      -I "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Tools/MSVC/14.28.29333/include"        \
#      -I "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Tools/MSVC/14.28.29333/atlmfc/include" \
#      -I "C:/Program Files (x86)/Windows Kits/10/Include/10.0.18362.0/ucrt"                                       \
#      -I "C:/Program Files (x86)/Windows Kits/10/include/10.0.18362.0/shared"                                     \
#      -I "C:/Program Files (x86)/Windows Kits/10/include/10.0.18362.0/um"                                         \
#      -I "C:/Program Files (x86)/Windows Kits/10/include/10.0.18362.0/winrt"


#    bin/preprocessor_dev                   \
#      --log-level $LOGLEVEL                \
#    $DEBUGFLAG                                                                                                    \
#      $COLORFLAG                           \
#      -D BONSAI_PREPROCESSOR               \
#      -D BONSAI_WIN32                      \
#      src/poof/preprocessor.cpp \
#      -I ./src

#   bin/preprocessor_dev                   \
#     --log-level $LOGLEVEL                \
#     $DEBUGFLAG                           \
#     $COLORFLAG                           \
#     -D BONSAI_PREPROCESSOR               \
#     -D BONSAI_LINUX                      \
#     src/poof/preprocessor.cpp \
#     -I ./src

#   bin/preprocessor_current               \
#     --log-level $LOGLEVEL                \
#     $DEBUGFLAG                           \
#     $COLORFLAG                           \
#     -D BONSAI_PREPROCESSOR               \
#     -D BONSAI_LINUX                      \
#     src/poof/preprocessor.cpp \
#     -I ./src


else
  echo "Unknown platform"
fi
