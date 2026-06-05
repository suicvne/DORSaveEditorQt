cmake_minimum_required(VERSION 3.19)
project(DORSaveEditorQt LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

# libdor
set(LIBDOR_BUILD_DOCS OFF)
set(LIBDOR_BUILD_SAMPLE OFF)
add_subdirectory(../libdor build/libdor)

qt_standard_project_setup()

qt_add_executable(DORSaveEditorQt
    WIN32 MACOSX_BUNDLE
    main.cpp
    dor_save_editor_main_window.cpp
    dor_save_editor_main_window.h
    dor_save_editor_main_window.ui
    dor_chest_model.h dor_chest_model.cpp
)

target_link_libraries(DORSaveEditorQt
    PRIVATE
        Qt::Core
        Qt::Widgets
    PUBLIC
        dor
)

install(TARGETS DORSaveEditorQt
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET DORSaveEditorQt
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
