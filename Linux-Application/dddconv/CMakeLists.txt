add_executable(dddconv
    dataconversion.cpp
    main.cpp
)

target_compile_definitions(dddconv PRIVATE
    QT_DEPRECATED_WARNINGS
)

target_link_libraries(dddconv PRIVATE
    Qt::Core
)

install(TARGETS dddconv)
