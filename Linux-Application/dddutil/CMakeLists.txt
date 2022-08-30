add_executable(dddutil WIN32 MACOSX_BUNDLE
    about.cpp about.h about.ui
    analysetestdata.cpp analysetestdata.h
    fileconverter.cpp fileconverter.h
    inputsample.cpp inputsample.h
    main.cpp
    mainwindow.cpp mainwindow.h mainwindow.ui
    progressdialog.cpp progressdialog.h progressdialog.ui
    sampledetails.cpp sampledetails.h
)

target_compile_definitions(dddutil PRIVATE
    QT_DEPRECATED_WARNINGS
)

target_link_libraries(dddutil PRIVATE
    Qt::Core
    Qt::Gui
    Qt::Widgets
)

install(TARGETS dddutil
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
