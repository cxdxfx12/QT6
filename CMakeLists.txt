cmake_minimum_required(VERSION 3.16)
project(WukongFreight VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Concurrent Network)

# QXlsx 子模块
set(QXLSX_LIB_TYPE STATIC)
add_subdirectory(QXlsx/QXlsx)

# 源文件
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/DataModel/OrderData.cpp
    src/DataModel/Customer.cpp
    src/DataModel/PriceTable.cpp
    src/DataModel/CalculationRule.cpp
    src/Excel/ExcelEngine.cpp
    src/Excel/ExcelImporter.cpp
    src/Excel/ExcelExporter.cpp
    src/Calculation/FreightCalculator.cpp
    src/Calculation/SimdCalculator.cpp
    src/Calculation/RuleManager.cpp
    src/Calculation/ThreadPool.cpp
    src/UI/CustomerDialog.cpp
    src/UI/PriceTableDialog.cpp
    src/UI/ProgressWidget.cpp
    src/UI/QuickTestWidget.cpp
    src/UI/GlobalRulesDialog.cpp
    src/UI/RuleManagerDialog.cpp
    src/Utils/Logger.cpp
    src/Utils/ConfigManager.cpp
    src/Auth/LoginDialog.cpp
    src/Auth/LicenseManager.cpp
)

set(HEADERS
    src/MainWindow.h
    src/DataModel/OrderData.h
    src/DataModel/Customer.h
    src/DataModel/PriceTable.h
    src/DataModel/CalculationRule.h
    src/Excel/ExcelEngine.h
    src/Excel/ExcelImporter.h
    src/Excel/ExcelExporter.h
    src/Calculation/FreightCalculator.h
    src/Calculation/SimdCalculator.h
    src/Calculation/RuleManager.h
    src/Calculation/ThreadPool.h
    src/UI/CustomerDialog.h
    src/UI/PriceTableDialog.h
    src/UI/ProgressWidget.h
    src/UI/QuickTestWidget.h
    src/UI/GlobalRulesDialog.h
    src/UI/RuleManagerDialog.h
    src/Utils/Logger.h
    src/Utils/ConfigManager.h
    src/Auth/LoginDialog.h
    src/Auth/LicenseManager.h
)

set(UI_FILES
    src/MainWindow.ui
    src/UI/RuleManagerDialog.ui
    src/Auth/LoginDialog.ui
)

# 资源文件
set(RESOURCES
    resources/resources.qrc
)

# Windows 图标资源
if(WIN32)
    set(WIN_RC resources/app.rc)
    # 设置 RC 文件的包含路径，确保 windres 能找到 app_icon.ico
    set_source_files_properties(resources/app.rc PROPERTIES
        COMPILE_FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/resources"
    )
else()
    set(WIN_RC "")
endif()

# 添加可执行文件
if(APPLE)
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${SOURCES} ${HEADERS} ${UI_FILES} ${RESOURCES} ${WIN_RC})
elseif(WIN32)
    add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS} ${UI_FILES} ${RESOURCES} ${WIN_RC})
else()
    add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${UI_FILES} ${RESOURCES} ${WIN_RC})
endif()

# 链接库
target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Network
    QXlsx::QXlsx
)

# 包含目录
target_include_directories(${PROJECT_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Windows 隐藏控制台 + 设置输出名称
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE ON
        OUTPUT_NAME "悟空结算"
    )
endif()
