cmake_minimum_required(VERSION 3.24)
project(Mlash VERSION 1.17.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
find_package(Qt6 6.10.2 EXACT REQUIRED COMPONENTS Core Widgets Network)
if(WIN32)
    find_package(Qt6 6.10.2 EXACT REQUIRED COMPONENTS ZlibPrivate)
    set(MLASH_ZLIB_TARGET Qt6::ZlibPrivate)
else()
    find_package(ZLIB REQUIRED)
    set(MLASH_ZLIB_TARGET ZLIB::ZLIB)
endif()
include(FetchContent)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(yaml_cpp GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG f7320141120f720aecc4c32be25586e7da9eb978 GIT_SHALLOW FALSE)
FetchContent_MakeAvailable(yaml_cpp)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)
set(ZSTD_MULTITHREAD_SUPPORT OFF CACHE BOOL "" FORCE)
FetchContent_Declare(zstd
    URL https://codeload.github.com/facebook/zstd/tar.gz/ac66b19e6bd6b83238bf008eecc1298105298532
    URL_HASH SHA256=566ea800f5df33b0b76e3fb2b93f95ffb58e82449074341b11ff2882b10cbc64
    SOURCE_SUBDIR build/cmake DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(zstd)
FetchContent_Declare(qrcodegen
    URL https://codeload.github.com/nayuki/QR-Code-generator/tar.gz/720f62bddb7226106071d4728c292cb1df519ceb
    URL_HASH SHA256=d80795686f3e58448c7b76830bb5c40847b7eaae1996c28f0422a029b66c2bb0)
FetchContent_MakeAvailable(qrcodegen)
add_library(mlash_qrcode STATIC "${qrcodegen_SOURCE_DIR}/cpp/qrcodegen.cpp")
target_include_directories(mlash_qrcode PUBLIC "${qrcodegen_SOURCE_DIR}/cpp")

add_library(mlash_engine STATIC
    src/app/Types.h src/app/Async.h src/app/AppController.h src/app/AppController.cpp src/app/AppControllerBackup.cpp src/app/AppControllerCoreUpdate.cpp src/app/AppControllerProfiles.cpp src/app/AppControllerTransaction.cpp
    src/app/ProfileMaintenance.h src/app/ProfileMaintenance.cpp
    src/app/ConnectionColumns.h src/app/ConnectionSession.h src/app/ConnectionSession.cpp
    src/app/HomeMetrics.h src/app/HomeMetrics.cpp
    src/app/HomeNodeProbe.h src/app/HomeNodeProbe.cpp
    src/app/ProtocolLink.h src/app/ProtocolLink.cpp src/app/ProtocolInbox.h src/app/ProtocolInbox.cpp
    src/app/OutboundDiagnostics.h src/app/OutboundDiagnostics.cpp
    src/app/ServiceDiagnostics.h src/app/ServiceDiagnostics.cpp
    src/app/WebsiteTests.h src/app/WebsiteTests.cpp
    src/core/MihomoClient.h src/core/MihomoClient.cpp src/core/CoreManager.h src/core/CoreManager.cpp
    src/core/CoreUpdate.h src/core/CoreUpdate.cpp src/core/OfficialCoreSource.h src/core/OfficialCoreSource.cpp
    src/core/TunPolicy.h src/core/TunPolicy.cpp src/core/TunSession.h src/core/TunSession.cpp src/core/TunApiGate.h src/core/TunApiGate.cpp
    src/profile/ConfigBuilder.h src/profile/ConfigBuilder.cpp
    src/profile/ProxyEdits.h src/profile/ProxyEdits.cpp
    src/profile/ProxyImpact.h src/profile/ProxyImpact.cpp
    src/profile/MrsSnapshot.h src/profile/MrsSnapshot.cpp
    src/profile/GeoData.h src/profile/GeoData.cpp
    src/profile/ProfileStore.h src/profile/ProfileStore.cpp
    src/profile/VersionChanges.h src/profile/VersionChanges.cpp
    src/profile/RuleDraft.h src/profile/RuleDraft.cpp
    src/profile/NodeLinks.h src/profile/NodeLinks.cpp
    src/profile/ProviderEdits.h src/profile/YamlFields.cpp src/profile/ProviderEdits.cpp src/profile/ProviderPolicy.cpp
    src/profile/SubscriptionDownloader.h src/profile/SubscriptionDownloader.cpp
    src/profile/SubscriptionInfo.h src/profile/SubscriptionInfo.cpp
    src/profile/BackupArchive.h src/profile/BackupArchive.cpp src/profile/LocalBackup.h src/profile/LocalBackup.cpp
    src/profile/RemoteBackup.h src/profile/RemoteBackup.cpp src/profile/WebDavClient.h src/profile/WebDavClient.cpp
    src/storage/Storage.h src/storage/Storage.cpp
    src/platform/PlatformServices.h src/platform/ProxyCoordinator.h src/platform/ProxyCoordinator.cpp
    src/platform/GlobalHotkeys.h src/platform/GlobalHotkeys.cpp
    src/diagnostics/Diagnostics.h src/diagnostics/Diagnostics.cpp src/diagnostics/ProcessMetrics.h)
target_include_directories(mlash_engine PUBLIC src)
target_link_libraries(mlash_engine PUBLIC Qt6::Core Qt6::Network yaml-cpp::yaml-cpp libzstd_static ${MLASH_ZLIB_TARGET})
target_compile_definitions(mlash_engine PUBLIC MLASH_VERSION="${PROJECT_VERSION}")
if(WIN32)
    target_sources(mlash_engine PRIVATE src/platform/windows/TunNative.h src/platform/windows/TunNative.cpp)
    target_sources(mlash_engine PRIVATE src/platform/windows/ProtocolRegistry.cpp)
    target_sources(mlash_engine PRIVATE src/platform/windows/WindowsServices.cpp src/platform/windows/ProcessMetrics.cpp src/platform/windows/PortableCrypto.cpp src/platform/windows/GlobalHotkeys.cpp)
    target_link_libraries(mlash_engine PRIVATE wininet crypt32 advapi32 iphlpapi ws2_32 psapi bcrypt user32 shell32 ole32)
    target_compile_definitions(mlash_engine PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
else()
    target_sources(mlash_engine PRIVATE src/platform/UnsupportedServices.cpp src/platform/UnsupportedMetrics.cpp src/platform/UnsupportedCrypto.cpp src/platform/UnsupportedHotkeys.cpp)
endif()
if(MSVC)
    target_compile_options(mlash_engine PUBLIC /utf-8 /W4 /permissive-)
else()
    target_compile_options(mlash_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(mlash_ui STATIC src/ui/ContextNote.h src/ui/SettingsSearch.h src/ui/SettingsSearch.cpp src/ui/HomeSummaryDialog.h src/ui/HomeSummaryDialog.cpp src/ui/MainWindow.h src/ui/MainWindow.cpp src/ui/MainWindowHome.cpp src/ui/MainWindowSettings.cpp src/ui/MainWindowProxy.cpp src/ui/MainWindowProfiles.cpp src/ui/MainWindowParts.h src/ui/NodeFilter.cpp
    src/ui/ProviderDefinitionDialog.h src/ui/YamlFormEditor.h src/ui/YamlFormEditor.cpp
    src/ui/ConnectionDetailsDialog.h src/ui/ConnectionDetailsDialog.cpp src/ui/ProviderDefinitionDialog.cpp src/ui/ProviderPolicyDialog.cpp
    src/ui/DialogDraftGuard.h src/ui/DialogDraftGuard.cpp
    src/ui/ProfileChangesDialog.h src/ui/ProfileChangesDialog.cpp
    src/ui/TunDialog.h src/ui/TunDialog.cpp
    src/ui/BackupDialog.h src/ui/BackupDialog.cpp src/ui/LocalBackupDialog.h src/ui/LocalBackupDialog.cpp
    src/ui/GuiHost.h src/ui/GuiHost.cpp
    src/ui/WebsiteTestsPage.h src/ui/WebsiteTestsPage.cpp
    src/ui/WebsiteTargetsDialog.h src/ui/WebsiteTargetsDialog.cpp
    src/ui/RuleOverlayDialog.h src/ui/RuleOverlayDialog.cpp
    src/ui/DnsDialog.h src/ui/DnsDialog.cpp
    src/ui/ProxyEditorDialog.h src/ui/ProxyEditorDialog.cpp
    src/ui/NodeLinksEditor.h src/ui/NodeLinksEditor.cpp
    src/ui/ProxyChainDialog.h src/ui/ProxyChainDialog.cpp
    src/ui/ProtocolImportQueue.h src/ui/ProtocolImportQueue.cpp
    src/ui/ProtocolSettingsDialog.h src/ui/ProtocolSettingsDialog.cpp
    src/ui/ProxyBypassDialog.h src/ui/ProxyBypassDialog.cpp
    src/ui/CoreUpdateDialog.h src/ui/CoreUpdateDialog.cpp
    src/ui/RemoteBackupDialog.h src/ui/RemoteBackupDialog.cpp
    src/ui/GeoDataDialog.h src/ui/GeoDataDialog.cpp
    src/ui/HomeMetricsView.h src/ui/HomeMetricsView.cpp
    src/ui/OutboundDialog.h src/ui/OutboundDialog.cpp
    src/ui/HotkeysDialog.h src/ui/HotkeysDialog.cpp
    src/ui/ConnectionOptions.h src/ui/ConnectionOptions.cpp
    src/ui/ActivityTableModel.h src/ui/ActivityTableModel.cpp resources/brand.qrc resources/flags.qrc
    src/ui/ImportDialog.h src/ui/ImportDialog.cpp src/ui/ProxyTableModel.h src/ui/ProxyTableModel.cpp
    src/ui/NodePresentation.h src/ui/NodePresentation.cpp
    src/ui/WidgetStyle.h src/ui/WidgetStyle.cpp
    src/ui/UiTheme.h src/ui/UiTheme.cpp
    src/ui/UiIcons.h src/ui/UiIcons.cpp resources/services.qrc
    src/ui/TrayController.h src/ui/TrayController.cpp)
target_link_libraries(mlash_ui PUBLIC mlash_engine Qt6::Widgets mlash_qrcode)
add_executable(Mlash WIN32 src/main.cpp src/diagnostics/QualificationRun.h src/diagnostics/QualificationRun.cpp)
target_link_libraries(Mlash PRIVATE mlash_ui)
if(WIN32)
    configure_file(resources/mlash_version.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/mlash_version.h" @ONLY)
    target_sources(Mlash PRIVATE resources/mlash.rc)
    set_source_files_properties(resources/mlash.rc PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/mlash.ico;${CMAKE_CURRENT_BINARY_DIR}/generated/mlash_version.h")
    target_include_directories(Mlash PRIVATE resources "${CMAKE_CURRENT_BINARY_DIR}/generated")
endif()
install(TARGETS Mlash RUNTIME DESTINATION .)
if(WIN32)
    add_executable(MlashTunService src/platform/windows/TunServiceMain.cpp resources/mlash.rc)
    target_link_libraries(MlashTunService PRIVATE mlash_engine)
    target_compile_definitions(MlashTunService PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_include_directories(MlashTunService PRIVATE resources "${CMAKE_CURRENT_BINARY_DIR}/generated")
    install(TARGETS MlashTunService RUNTIME DESTINATION .)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.10.2 EXACT REQUIRED COMPONENTS Test)
    add_executable(mlash_tun_tests tests/TunTests.cpp tests/MockHttp.cpp)
    target_link_libraries(mlash_tun_tests PRIVATE mlash_ui Qt6::Test)
    target_compile_definitions(mlash_tun_tests PRIVATE MLASH_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_dependencies(mlash_tun_tests fake_core)
    add_test(NAME tun_tests COMMAND mlash_tun_tests -o "${CMAKE_BINARY_DIR}/tun-tests.xml,xml")
    set_tests_properties(tun_tests PROPERTIES TIMEOUT 75 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(mlash_tests tests/EngineTests.cpp tests/MockHttp.h tests/MockHttp.cpp src/ui/ProxyTableModel.h src/ui/ProxyTableModel.cpp)
    target_link_libraries(mlash_tests PRIVATE mlash_engine Qt6::Test Qt6::Gui)
    target_compile_definitions(mlash_tests PRIVATE MLASH_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_executable(fake_core tests/FakeCore.cpp tests/MockHttp.cpp)
    target_link_libraries(fake_core PRIVATE Qt6::Core Qt6::Network yaml-cpp::yaml-cpp)
    add_executable(fake_core_update tests/FakeCore.cpp tests/MockHttp.cpp)
    target_compile_definitions(fake_core_update PRIVATE MLASH_UPDATE_FIXTURE)
    target_link_libraries(fake_core_update PRIVATE Qt6::Core Qt6::Network yaml-cpp::yaml-cpp)
    add_executable(mlash_readiness_tests tests/CoreReadinessTests.cpp)
    target_link_libraries(mlash_readiness_tests PRIVATE mlash_engine Qt6::Test)
    target_compile_definitions(mlash_readiness_tests PRIVATE MLASH_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_dependencies(mlash_readiness_tests fake_core)
    add_test(NAME core_readiness_tests COMMAND mlash_readiness_tests -o "${CMAKE_BINARY_DIR}/core-readiness-tests.xml,xml")
    set_tests_properties(core_readiness_tests PROPERTIES TIMEOUT 30)
    add_test(NAME engine COMMAND mlash_tests -o "${CMAKE_BINARY_DIR}/engine-tests.xml,xml")
    set_tests_properties(engine PROPERTIES TIMEOUT 90 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME gui_smoke COMMAND Mlash --smoke-test --data-dir "${CMAKE_BINARY_DIR}/smoke-data")
    set_tests_properties(gui_smoke PROPERTIES TIMEOUT 15 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(mlash_ui_tests tests/UiTests.cpp tests/MockHttp.cpp)
    add_executable(mlash_workflow_depth_tests tests/WorkflowDepthTests.cpp tests/MockHttp.cpp)
    target_link_libraries(mlash_workflow_depth_tests PRIVATE mlash_ui Qt6::Test)
    add_test(NAME workflow_depth_tests COMMAND mlash_workflow_depth_tests -o "${CMAKE_BINARY_DIR}/workflow-depth-tests.xml,xml")
    set_tests_properties(workflow_depth_tests PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    target_link_libraries(mlash_ui_tests PRIVATE mlash_ui Qt6::Test)
    target_compile_definitions(mlash_ui_tests PRIVATE MLASH_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    # Separate the long end-to-end fixture from independent components. Keep
    # each existing 30s budget; no runtime or identity checks are relaxed.
    add_test(NAME ui_interactions COMMAND mlash_ui_tests currentGroupSelectionAndNativePreview -o "${CMAKE_BINARY_DIR}/ui-tests.xml,xml")
    set_tests_properties(ui_interactions PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME ui_components COMMAND mlash_ui_tests
        popupSurfacesWithoutMainWindow desktopPreferencesAndEnvironmentCopy responsivePageGeometry
        historyReadingAndDraftFeedback logPauseClearAndDiagnosticPrivacy profilePolicyDialogPersistsOnlyOnSave
        providerAndCompatibilityDialogsRespectVersions
        trayGroupDwellCacheAndSelection trayAppearancePriorityAndResources lightweightLifecycleAndQuickReopen
        importValidationAndSecretVisibility failedImportRetainsDraftUntilExplicitRetry unchangedSnapshotsPreserveRowsAndChoices refinedTaskGeometryAndSemantics refinedContextFitsAutomaticGroupsAndLargeText emptyStateCannotIssueInvalidCommands fileProfileOffersSourceSyncAndReplacement
        coherentActionsAndBilingualRuleTokens nodeLocationHints dashboardTabsAndSettingsFocus quietMouseFocusAndGroupedSettings
        -o "${CMAKE_BINARY_DIR}/ui-components.xml,xml")
    set_tests_properties(ui_components PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME status_bar_tests COMMAND mlash_ui_tests persistentStatusBarDoesNotMovePages -o "${CMAKE_BINARY_DIR}/status-bar-tests.xml,xml")
    set_tests_properties(status_bar_tests PROPERTIES TIMEOUT 20 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(mlash_website_tests tests/WebsiteTests.cpp tests/MockHttp.cpp)
    add_executable(mlash_backup_tests tests/BackupTests.cpp)
    add_executable(mlash_official_core_tests tests/OfficialCoreTests.cpp)
    target_link_libraries(mlash_official_core_tests PRIVATE mlash_ui Qt6::Test)
    target_compile_definitions(mlash_official_core_tests PRIVATE MLASH_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_dependencies(mlash_official_core_tests fake_core_update)
    add_test(NAME official_core_tests COMMAND mlash_official_core_tests -o "${CMAKE_BINARY_DIR}/official-core-tests.xml,xml")
    set_tests_properties(official_core_tests PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(mlash_yaml_forms_tests tests/YamlFormsTests.cpp)
    add_executable(mlash_rules_tests tests/RulesTests.cpp)
    add_executable(mlash_node_links_tests tests/NodeLinksTests.cpp)
    target_link_libraries(mlash_node_links_tests PRIVATE mlash_ui Qt6::Test)
    target_compile_definitions(mlash_node_links_tests PRIVATE MLASH_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME node_links_tests COMMAND mlash_node_links_tests -o "${CMAKE_BINARY_DIR}/node-links-tests.xml,xml")
    set_tests_properties(node_links_tests PROPERTIES TIMEOUT 75 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(mlash_provider_edits_tests tests/ProviderEditsTests.cpp)
    add_executable(mlash_dns_tests tests/DnsTests.cpp)
    add_executable(mlash_mrs_tests tests/MrsTests.cpp)
    add_executable(mlash_geo_tests tests/GeoTests.cpp tests/MockHttp.cpp)
    add_executable(mlash_home_tests tests/HomeMetricsTests.cpp tests/MockHttp.cpp)
    add_executable(mlash_outbound_tests tests/OutboundTests.cpp tests/MockHttp.cpp)
    add_executable(mlash_service_tests tests/ServiceTests.cpp tests/MockHttp.cpp)
    add_executable(mlash_hotkey_tests tests/HotkeyTests.cpp tests/MockHttp.cpp)
    target_compile_definitions(mlash_hotkey_tests PRIVATE MLASH_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_executable(mlash_connection_view_tests tests/ConnectionViewTests.cpp tests/MockHttp.cpp)
    add_executable(mlash_appearance_prefs_tests tests/AppearancePrefsTests.cpp tests/MockHttp.cpp)
    add_executable(mlash_proxy_edits_tests tests/ProxyEditsTests.cpp)
    add_executable(mlash_proxy_chain_tests tests/ProxyChainTests.cpp tests/MockHttp.cpp)
    add_executable(mlash_home_probe_tests tests/HomeProbeTests.cpp tests/MockHttp.cpp)
    add_executable(mlash_protocol_tests tests/ProtocolTests.cpp)
    add_executable(mlash_proxy_bypass_tests tests/ProxyBypassTests.cpp)
    add_executable(mlash_core_update_tests tests/CoreUpdateTests.cpp)
    add_executable(mlash_remote_backup_tests tests/RemoteBackupTests.cpp tests/MockHttp.cpp)
    target_link_libraries(mlash_remote_backup_tests PRIVATE mlash_ui Qt6::Test)
    target_compile_definitions(mlash_remote_backup_tests PRIVATE MLASH_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME remote_backup_tests COMMAND mlash_remote_backup_tests -o "${CMAKE_BINARY_DIR}/remote-backup-tests.xml,xml")
    set_tests_properties(remote_backup_tests PROPERTIES TIMEOUT 45 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    target_link_libraries(mlash_core_update_tests PRIVATE mlash_ui Qt6::Test)
    target_compile_definitions(mlash_core_update_tests PRIVATE MLASH_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_dependencies(mlash_core_update_tests fake_core fake_core_update)
    add_test(NAME core_update_tests COMMAND mlash_core_update_tests -o "${CMAKE_BINARY_DIR}/core-update-tests.xml,xml")
    set_tests_properties(core_update_tests PROPERTIES TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    target_link_libraries(mlash_proxy_bypass_tests PRIVATE mlash_ui Qt6::Test)
    add_test(NAME proxy_bypass_tests COMMAND mlash_proxy_bypass_tests -o "${CMAKE_BINARY_DIR}/proxy-bypass-tests.xml,xml")
    set_tests_properties(proxy_bypass_tests PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    if(WIN32)
        target_sources(mlash_protocol_tests PRIVATE src/platform/windows/ProtocolTestSupport.cpp)
        target_compile_definitions(mlash_protocol_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    endif()
    target_link_libraries(mlash_protocol_tests PRIVATE mlash_ui Qt6::Test)
    add_test(NAME protocol_tests COMMAND mlash_protocol_tests -o "${CMAKE_BINARY_DIR}/protocol-tests.xml,xml")
    set_tests_properties(protocol_tests PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    target_link_libraries(mlash_home_probe_tests PRIVATE mlash_ui Qt6::Test)
    target_compile_definitions(mlash_home_probe_tests PRIVATE MLASH_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME home_probe_tests COMMAND mlash_home_probe_tests -o "${CMAKE_BINARY_DIR}/home-probe-tests.xml,xml")
    set_tests_properties(home_probe_tests PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    target_link_libraries(mlash_proxy_chain_tests PRIVATE mlash_ui Qt6::Test)
    target_compile_definitions(mlash_proxy_chain_tests PRIVATE MLASH_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME proxy_chain_tests COMMAND mlash_proxy_chain_tests -o "${CMAKE_BINARY_DIR}/proxy-chain-tests.xml,xml")
    set_tests_properties(proxy_chain_tests PROPERTIES TIMEOUT 45 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    target_link_libraries(mlash_proxy_edits_tests PRIVATE mlash_ui Qt6::Test)
    target_compile_definitions(mlash_proxy_edits_tests PRIVATE MLASH_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME proxy_edits_tests COMMAND mlash_proxy_edits_tests -o "${CMAKE_BINARY_DIR}/proxy-edits-tests.xml,xml")
    set_tests_properties(proxy_edits_tests PROPERTIES TIMEOUT 45 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    target_link_libraries(mlash_appearance_prefs_tests PRIVATE mlash_ui Qt6::Test)
    target_compile_definitions(mlash_appearance_prefs_tests PRIVATE MLASH_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME appearance_prefs_tests COMMAND mlash_appearance_prefs_tests -o "${CMAKE_BINARY_DIR}/appearance-prefs-tests.xml,xml")
    set_tests_properties(appearance_prefs_tests PROPERTIES TIMEOUT 45 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    target_link_libraries(mlash_connection_view_tests PRIVATE mlash_ui Qt6::Test)
    add_test(NAME connection_view_tests COMMAND mlash_connection_view_tests -o "${CMAKE_BINARY_DIR}/connection-view-tests.xml,xml")
    set_tests_properties(connection_view_tests PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    target_link_libraries(mlash_hotkey_tests PRIVATE mlash_ui Qt6::Test)
    if(WIN32)
        target_sources(mlash_hotkey_tests PRIVATE src/platform/windows/HotkeyTestSupport.cpp)
        target_compile_definitions(mlash_hotkey_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    endif()
    add_test(NAME hotkey_tests COMMAND mlash_hotkey_tests -o "${CMAKE_BINARY_DIR}/hotkey-tests.xml,xml")
    set_tests_properties(hotkey_tests PROPERTIES TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    target_link_libraries(mlash_service_tests PRIVATE mlash_engine Qt6::Test)
    add_test(NAME service_tests COMMAND mlash_service_tests -o "${CMAKE_BINARY_DIR}/service-tests.xml,xml")
    set_tests_properties(service_tests PROPERTIES TIMEOUT 30)
    target_link_libraries(mlash_outbound_tests PRIVATE mlash_ui Qt6::Test)
    add_test(NAME outbound_tests COMMAND mlash_outbound_tests -o "${CMAKE_BINARY_DIR}/outbound-tests.xml,xml")
    set_tests_properties(outbound_tests PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    target_link_libraries(mlash_home_tests PRIVATE mlash_ui Qt6::Test)
    add_test(NAME home_metrics_tests COMMAND mlash_home_tests -o "${CMAKE_BINARY_DIR}/home-metrics-tests.xml,xml")
    set_tests_properties(home_metrics_tests PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    target_link_libraries(mlash_geo_tests PRIVATE mlash_ui Qt6::Test)
    target_compile_definitions(mlash_geo_tests PRIVATE MLASH_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME geo_tests COMMAND mlash_geo_tests -o "${CMAKE_BINARY_DIR}/geo-tests.xml,xml")
    set_tests_properties(geo_tests PROPERTIES TIMEOUT 45 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    target_link_libraries(mlash_mrs_tests PRIVATE mlash_engine Qt6::Test)
    target_compile_definitions(mlash_mrs_tests PRIVATE MLASH_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME mrs_tests COMMAND mlash_mrs_tests -o "${CMAKE_BINARY_DIR}/mrs-tests.xml,xml")
    set_tests_properties(mrs_tests PROPERTIES TIMEOUT 30)
    target_link_libraries(mlash_dns_tests PRIVATE mlash_ui Qt6::Test)
    target_compile_definitions(mlash_dns_tests PRIVATE MLASH_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME dns_tests COMMAND mlash_dns_tests -o "${CMAKE_BINARY_DIR}/dns-tests.xml,xml")
    set_tests_properties(dns_tests PROPERTIES TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    target_link_libraries(mlash_provider_edits_tests PRIVATE mlash_ui Qt6::Test)
    target_compile_definitions(mlash_provider_edits_tests PRIVATE MLASH_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME provider_edits_tests COMMAND mlash_provider_edits_tests -o "${CMAKE_BINARY_DIR}/provider-edits-tests.xml,xml")
    set_tests_properties(provider_edits_tests PROPERTIES TIMEOUT 75 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    target_link_libraries(mlash_yaml_forms_tests PRIVATE mlash_ui Qt6::Test)
    target_compile_definitions(mlash_yaml_forms_tests PRIVATE MLASH_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME yaml_forms_tests COMMAND mlash_yaml_forms_tests -o "${CMAKE_BINARY_DIR}/yaml-forms-tests.xml,xml")
    set_tests_properties(yaml_forms_tests PROPERTIES TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    target_link_libraries(mlash_rules_tests PRIVATE mlash_ui Qt6::Test)
    target_compile_definitions(mlash_rules_tests PRIVATE MLASH_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME rules_tests COMMAND mlash_rules_tests -o "${CMAKE_BINARY_DIR}/rules-tests.xml,xml")
    set_tests_properties(rules_tests PROPERTIES TIMEOUT 45 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    target_link_libraries(mlash_backup_tests PRIVATE mlash_ui Qt6::Test)
    target_compile_definitions(mlash_backup_tests PRIVATE MLASH_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME backup_tests COMMAND mlash_backup_tests -o "${CMAKE_BINARY_DIR}/backup-tests.xml,xml")
    set_tests_properties(backup_tests PROPERTIES TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    target_link_libraries(mlash_website_tests PRIVATE mlash_ui Qt6::Test)
    target_compile_definitions(mlash_website_tests PRIVATE MLASH_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME website_tests COMMAND mlash_website_tests -o "${CMAKE_BINARY_DIR}/website-tests.xml,xml")
    set_tests_properties(website_tests PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(mlash_appearance_tests tests/AppearanceTests.cpp tests/MockHttp.cpp)
    target_link_libraries(mlash_appearance_tests PRIVATE mlash_ui Qt6::Test)
    target_compile_definitions(mlash_appearance_tests PRIVATE MLASH_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME appearance_tests COMMAND mlash_appearance_tests -o "${CMAKE_BINARY_DIR}/appearance-tests.xml,xml")
    set_tests_properties(appearance_tests PROPERTIES TIMEOUT 45 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    if(NOT WIN32)
        add_test(NAME ui_refinement_tests COMMAND mlash_appearance_tests
            semanticContrastAndArtwork themeChangesPreserveDraftAndPopupFirstFrame
            allPagesAndDialogsAcrossAppearances groupedSettingsAndPopulatedPagesRemainUsable
            toolWindowsKeepActionsAndDraftsAccessible quietDefaultsKeepErrorsDraftsAndHelp
            taskEntryPointsAndSettingsStayCoherent ruleListAndHomeSelectionRequireExplicitApply
            -o "${CMAKE_BINARY_DIR}/ui-refinement-tests.xml,xml")
        set_tests_properties(ui_refinement_tests PROPERTIES TIMEOUT 45 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
    if(WIN32)
        add_executable(mlash_system_tests tests/WindowsReleaseTests.cpp src/platform/windows/WindowsReleaseSupport.cpp)
        target_link_libraries(mlash_system_tests PRIVATE mlash_engine Qt6::Test)
        target_compile_definitions(mlash_system_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
        add_test(NAME system_test_guard COMMAND mlash_system_tests)
        set_tests_properties(system_test_guard PROPERTIES TIMEOUT 10 SKIP_RETURN_CODE 77)
    endif()
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME release_tools COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/test_release_tools.py")
        set_tests_properties(release_tools PROPERTIES TIMEOUT 20)
    endif()
endif()
