cmake_minimum_required(VERSION 3.21)
project(motiondetecter VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)
qt_standard_project_setup()
# Layouts are built in C++ to share the same widgets and support deterministic UI tests.
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC ON)
add_library(motion_ui STATIC
    core/types.cpp core/types.h
    modules/pose/SkeletonPainter.cpp modules/pose/SkeletonPainter.h
    app/AppState.h
    modules/ui/theme/Theme.cpp modules/ui/theme/Theme.h
    modules/ui/widgets/Controls.cpp modules/ui/widgets/Controls.h
    modules/ui/widgets/VideoView.cpp modules/ui/widgets/VideoView.h
    modules/ui/widgets/ChartView.cpp modules/ui/widgets/ChartView.h
    modules/ui/adapters/PoseSampleReader.cpp modules/ui/adapters/PoseSampleReader.h
    modules/ui/adapters/AnalysisM0Adapter.cpp modules/ui/adapters/AnalysisM0Adapter.h
    modules/ui/controllers/Controllers.cpp modules/ui/controllers/Controllers.h
    modules/ui/shell/Sidebar.cpp modules/ui/shell/Sidebar.h
    modules/ui/pages/RealtimePage.cpp modules/ui/pages/RealtimePage.h
    modules/ui/pages/DevicePage.cpp modules/ui/pages/DevicePage.h
    modules/ui/pages/DetectionConfigPage.cpp modules/ui/pages/DetectionConfigPage.h
    modules/ui/pages/PlaybackPage.cpp modules/ui/pages/PlaybackPage.h
    modules/ui/pages/AnalysisPage.cpp modules/ui/pages/AnalysisPage.h
    modules/ui/pages/SettingsPage.cpp modules/ui/pages/SettingsPage.h
    mainwindow.cpp mainwindow.h
    modules/ui/resources/ui.qrc)
target_include_directories(motion_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(motion_ui PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets)
qt_add_executable(motiondetecter MACOSX_BUNDLE WIN32 main.cpp)
target_link_libraries(motiondetecter PRIVATE motion_ui)
set_target_properties(motiondetecter PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME PoseLab
    MACOSX_BUNDLE_GUI_IDENTIFIER cn.motiondetecter.poselab.ui
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(ui_tests tests/ui_tests.cpp)
    target_link_libraries(ui_tests PRIVATE motion_ui Qt6::Test)
    add_test(NAME ui_contracts COMMAND ui_tests)
    set_tests_properties(ui_contracts PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
install(TARGETS motiondetecter BUNDLE DESTINATION . RUNTIME DESTINATION bin)
