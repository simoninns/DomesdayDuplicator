set(DomesdayDuplicator_SOURCES
    aboutdialog.cpp aboutdialog.ui
    advancednamingdialog.cpp advancednamingdialog.ui
    amplitudemeasurement.cpp
    automaticcapturedialog.cpp automaticcapturedialog.ui
    configuration.cpp
    configurationdialog.cpp configurationdialog.ui
    main.cpp
    mainwindow.cpp mainwindow.ui
    testdataanalysisdialog.cpp testdataanalysisdialog.ui
    playercommunication.cpp
    playercontrol.cpp
    playerremotedialog.cpp playerremotedialog.ui
    qcustomplot.cpp
    QtLogger.cpp
    UsbDeviceBase.cpp
    UsbDeviceLibUsb.cpp
    UsbDeviceWinUsb.cpp
)

qt_add_resources(DomesdayDuplicator_SOURCES resources.qrc)

add_executable(DomesdayDuplicator WIN32 MACOSX_BUNDLE
    ${DomesdayDuplicator_SOURCES})

# The deliverable goes to build/bin, not to a build-tree mirror of its source directory.
# CMake's default puts it at build/src/DomesdayDuplicator/DomesdayDuplicator, which reads as
# a copy of the sources and buries the one artefact anybody wants to run underneath two
# directories that exist only because of where the CMakeLists.txt happens to live.
# The test executables keep the default layout: they are not deliverables, and mixing them
# in with the application is what makes a bin directory stop being useful.
set_target_properties(DomesdayDuplicator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Suppress deprecation warnings for third-party QCustomPlot library
set_source_files_properties(qcustomplot.cpp PROPERTIES
    COMPILE_FLAGS "-Wno-deprecated-declarations"
)

target_compile_definitions(DomesdayDuplicator PRIVATE
    QT_DEPRECATED_WARNINGS
)

target_link_libraries(DomesdayDuplicator PRIVATE
    Qt::Core
    Qt::Gui
    Qt::Widgets
    Qt::SerialPort
    ${LibUSB_LIBRARIES}
    ddd-common
    ${FLAC_LIBRARIES}
)

target_include_directories(DomesdayDuplicator PRIVATE
    ${LibUSB_INCLUDE_DIRS}
    .
)

if(WIN32)
    target_compile_definitions(DomesdayDuplicator PRIVATE
        NOMINMAX
        QUSB_LIBRARY
    )

    target_link_libraries(DomesdayDuplicator PRIVATE
        AdvAPI32
        cfgmgr32
        onecoreuap
        winusb
    )
endif()

install(TARGETS DomesdayDuplicator
        BUNDLE DESTINATION .
        RUNTIME DESTINATION bin
)

# Desktop integration (P7-13). Installed on Linux only: macOS gets this from the .app
# bundle's Info.plist and Windows from the MSI's shortcut, so installing freedesktop files
# there would just be litter. The Flatpak build reads these from the install prefix rather
# than carrying its own copies, which is what stops the manifest and the application
# disagreeing about the application ID.
if(UNIX AND NOT APPLE)
    set(DDD_APP_ID "io.github.simoninns.DomesdayDuplicator")
    set(DDD_PACKAGING_ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/../../packaging/assets")

    install(FILES "${DDD_PACKAGING_ASSETS}/${DDD_APP_ID}.desktop"
            DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")

    install(FILES "${DDD_PACKAGING_ASSETS}/${DDD_APP_ID}.metainfo.xml"
            DESTINATION "${CMAKE_INSTALL_PREFIX}/share/metainfo")

    foreach(icon_size 16 24 32 48 64 128 256)
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/Graphics/ApplicationIcon/DomesdayDuplicator_${icon_size}x${icon_size}.png"
                DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/${icon_size}x${icon_size}/apps"
                RENAME "${DDD_APP_ID}.png")
    endforeach()
endif()
