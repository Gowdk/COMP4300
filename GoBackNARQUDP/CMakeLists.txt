# Compile instructions for the ns-3 executable.
build_exec(
    EXECNAME main
    SOURCE_FILES
        main.cc
        gbnSender.cc
        gbnReceiver.cc
        slidingWindow.cc
        packetEncoding.cc
    HEADER_FILES
        gbnSender.h
        gbnReceiver.h
        slidingWindow.h
        packetEncoding.h
    LIBRARIES_TO_LINK
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libpoint-to-point}
        ${libapplications}
    EXECUTABLE_DIRECTORY_PATH
        ${CMAKE_OUTPUT_DIRECTORY}/scratch/assignment2
)
