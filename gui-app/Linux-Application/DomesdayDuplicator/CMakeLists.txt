set(DomesdayDuplicator_SOURCES
    aboutdialog.cpp aboutdialog.h aboutdialog.ui
    advancednamingdialog.cpp advancednamingdialog.h advancednamingdialog.ui
    automaticcapturedialog.cpp automaticcapturedialog.h automaticcapturedialog.ui
    configuration.cpp configuration.h
    configurationdialog.cpp configurationdialog.h configurationdialog.ui
    main.cpp
    mainwindow.cpp mainwindow.h mainwindow.ui
    playercommunication.cpp playercommunication.h
    playercontrol.cpp playercontrol.h
    playerremotedialog.cpp playerremotedialog.h playerremotedialog.ui
    usbcapture.cpp usbcapture.h
    usbdevice.cpp usbdevice.h
)

qt_add_resources(DomesdayDuplicator_SOURCES resources.qrc)

add_executable(DomesdayDuplicator MACOSX_BUNDLE
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

install(TARGETS DomesdayDuplicator
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
