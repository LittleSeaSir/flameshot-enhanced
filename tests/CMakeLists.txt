find_package(Qt${QT_VERSION_MAJOR} CONFIG REQUIRED Gui Widgets)

# Exercise CaptureWidget's real mouse-event path without rebuilding its large
# dependency graph as a second executable. This source and command-line entry
# point exist only when FLAMESHOT_BUILD_TESTS is enabled.
target_sources(
  flameshot
  PRIVATE capturewidget_shape_reselection_test.cpp
          capturewidget_windowsnap_test.cpp)
target_compile_definitions(flameshot PRIVATE FLAMESHOT_BUILD_TESTS=1)
add_test(
  NAME capturewidget_shape_reselection_test
  COMMAND $<TARGET_FILE:flameshot> --self-test-capture-shape-reselection)
set_tests_properties(
  capturewidget_shape_reselection_test
  PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_test(
  NAME capturewidget_windowsnap_test
  COMMAND $<TARGET_FILE:flameshot> --self-test-capture-window-snap)
set_tests_properties(
  capturewidget_windowsnap_test
  PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(
  strokesmoother_test
  strokesmoother_test.cpp
  ${CMAKE_SOURCE_DIR}/src/tools/pencil/strokesmoother.cpp)

target_include_directories(strokesmoother_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(strokesmoother_test PRIVATE Qt${QT_VERSION_MAJOR}::Gui)
target_compile_features(strokesmoother_test PRIVATE cxx_std_20)

add_test(NAME strokesmoother_test COMMAND strokesmoother_test)

add_executable(
  annotation_tools_test
  annotation_tools_test.cpp
  ${CMAKE_SOURCE_DIR}/src/tools/capturetool.h
  ${CMAKE_SOURCE_DIR}/src/tools/abstracttwopointtool.cpp
  ${CMAKE_SOURCE_DIR}/src/tools/abstractpathtool.cpp
  ${CMAKE_SOURCE_DIR}/src/tools/circle/circletool.cpp
  ${CMAKE_SOURCE_DIR}/src/tools/eraser/erasertool.cpp
  ${CMAKE_SOURCE_DIR}/src/tools/line/linetool.cpp
  ${CMAKE_SOURCE_DIR}/src/tools/pencil/penciltool.cpp
  ${CMAKE_SOURCE_DIR}/src/tools/pencil/strokesmoother.cpp
  ${CMAKE_SOURCE_DIR}/src/tools/rectangle/rectangletool.cpp
  ${CMAKE_SOURCE_DIR}/src/utils/colorutils.cpp
  ${CMAKE_SOURCE_DIR}/src/utils/pathinfo.cpp
  ${CMAKE_SOURCE_DIR}/src/widgets/capture/capturetoolobjects.cpp)
target_include_directories(annotation_tools_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(annotation_tools_test PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_compile_features(annotation_tools_test PRIVATE cxx_std_20)
target_compile_definitions(annotation_tools_test PRIVATE APP_PREFIX="${CMAKE_INSTALL_PREFIX}")
set_target_properties(annotation_tools_test PROPERTIES AUTOMOC ON)
add_test(NAME annotation_tools_test COMMAND annotation_tools_test)
set_tests_properties(annotation_tools_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(
  pineditorlayout_test
  pineditorlayout_test.cpp
  ${CMAKE_SOURCE_DIR}/src/tools/pin/pineditorlayout.cpp)
target_include_directories(pineditorlayout_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(pineditorlayout_test PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_compile_features(pineditorlayout_test PRIVATE cxx_std_20)
add_test(NAME pineditorlayout_test COMMAND pineditorlayout_test)
set_tests_properties(pineditorlayout_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(
  waylandwindowpositioner_test
  waylandwindowpositioner_test.cpp
  ${CMAKE_SOURCE_DIR}/src/utils/waylandwindowpositioner.cpp)
target_include_directories(waylandwindowpositioner_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(waylandwindowpositioner_test PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
if(UNIX AND NOT APPLE)
  target_link_libraries(waylandwindowpositioner_test PRIVATE Qt${QT_VERSION_MAJOR}::DBus)
endif()
target_compile_features(waylandwindowpositioner_test PRIVATE cxx_std_20)
add_test(NAME waylandwindowpositioner_test COMMAND waylandwindowpositioner_test)
set_tests_properties(waylandwindowpositioner_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(
  selectionwidget_lock_test
  selectionwidget_lock_test.cpp
  ${CMAKE_SOURCE_DIR}/src/widgets/capture/selectionwidget.cpp
  ${CMAKE_SOURCE_DIR}/src/widgets/capture/selectionwidget.h
  ${CMAKE_SOURCE_DIR}/src/utils/globalvalues.cpp)
target_include_directories(selectionwidget_lock_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(selectionwidget_lock_test PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_compile_features(selectionwidget_lock_test PRIVATE cxx_std_20)
target_compile_definitions(
  selectionwidget_lock_test
  PRIVATE APP_VERSION="test" FLAMESHOT_GIT_HASH="test" USE_MONOCHROME_ICON=0)
set_target_properties(selectionwidget_lock_test PROPERTIES AUTOMOC ON)
add_test(NAME selectionwidget_lock_test COMMAND selectionwidget_lock_test)
set_tests_properties(selectionwidget_lock_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(
  screenshotscale_test
  screenshotscale_test.cpp
  ${CMAKE_SOURCE_DIR}/src/utils/screenshotscale.cpp)
target_include_directories(screenshotscale_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(screenshotscale_test PRIVATE Qt${QT_VERSION_MAJOR}::Core)
target_compile_features(screenshotscale_test PRIVATE cxx_std_20)
add_test(NAME screenshotscale_test COMMAND screenshotscale_test)
