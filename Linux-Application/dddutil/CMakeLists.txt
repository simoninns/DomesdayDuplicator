add_executable(dddutil WIN32 MACOSX_BUNDLE
    about.cpp about.ui
    analysetestdata.cpp
    fileconverter.cpp
    inputsample.cpp
    main.cpp
    mainwindow.cpp mainwindow.ui
    progressdialog.cpp progressdialog.ui
    sampledetails.cpp
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
    RUNTIME DESTINATION bin
)
