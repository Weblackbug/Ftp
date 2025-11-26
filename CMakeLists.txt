cmake_minimum_required(VERSION 3.10)
project(FtpUploader)

set(CMAKE_CXX_STANDARD 17)

# Add resource file
set(SOURCES
    main.cpp
    ftp_uploader.cpp
    ftp_uploader.h
    resource.rc
    miniz.c
)

add_executable(FtpUploader ${SOURCES})

# Link libraries
target_link_libraries(FtpUploader PRIVATE
    wininet
    comctl32
    user32
    gdi32
)
