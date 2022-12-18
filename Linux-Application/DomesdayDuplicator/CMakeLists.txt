set(DomesdayDuplicator_SOURCES
    aboutdialog.cpp aboutdialog.ui
    advancednamingdialog.cpp advancednamingdialog.ui
    amplitudemeasurement.cpp
    automaticcapturedialog.cpp automaticcapturedialog.ui
    configuration.cpp
    configurationdialog.cpp configurationdialog.ui
    main.cpp
    mainwindow.cpp mainwindow.ui
    playercommunication.cpp
    playercontrol.cpp
    playerremotedialog.cpp playerremotedialog.ui
    qcustomplot.cpp
    usbcapture.cpp
    usbdevice.cpp
)

qt_add_resources(DomesdayDuplicator_SOURCES resources.qrc)

add_executable(DomesdayDuplicator WIN32 MACOSX_BUNDLE
    ${DomesdayDuplicator_SOURCES})

target_compile_definitions(DomesdayDuplicator PRIVATE
    QT_DEPRECATED_WARNINGS
)

target_link_libraries(DomesdayDuplicator PRIVATE
    Qt::Core
    Qt::Gui
    Qt::Widgets
    Qt::SerialPort
    ${LibUSB_LIBRARIES}
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
        # TODO: Does this depend on compiler?
        AdvAPI32
    )
endif()

install(TARGETS DomesdayDuplicator)
