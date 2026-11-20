cmake_minimum_required(VERSION 3.24)
project(Mlash VERSION 1.22.0 LANGUAGES C CXX)
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

add_library(mlash_domain STATIC
    src/domain/Types.h src/domain/ConnectionColumns.h
    src/domain/WebsiteTargets.h src/domain/WebsiteTargets.cpp
    src/domain/TunOptions.h src/domain/TunOptions.cpp
    src/domain/HotkeyBindings.h src/domain/HotkeyBindings.cpp)
target_include_directories(mlash_domain PUBLIC src)
target_link_libraries(mlash_domain PUBLIC Qt6::Core Qt6::Network)

add_library(mlash_engine STATIC
    src/app/Async.h src/app/AppUpdateSource.h src/app/AppUpdateSource.cpp src/app/ConnectivityProbe.h src/app/ConnectivityProbe.cpp src/app/AppController.h src/app/AppController.cpp src/app/AppControllerLiveViews.cpp src/app/AppControllerRuntime.cpp src/app/AppControllerPreferences.cpp src/app/AppControllerDiagnostics.cpp src/app/AppControllerLifecycle.cpp src/app/AppControllerBackup.cpp src/app/AppControllerCoreUpdate.cpp src/app/AppControllerAppUpdate.cpp src/app/AppControllerProfiles.cpp src/app/AppControllerTransaction.cpp src/app/AppControllerProxySwitch.cpp
    src/app/ProfileMaintenance.h src/app/ProfileMaintenance.cpp
    src/app/ConnectionSession.h src/app/ConnectionSession.cpp
    src/app/HomeMetrics.h src/app/HomeMetrics.cpp
    src/app/HomeNodeProbe.h src/app/HomeNodeProbe.cpp
    src/app/ProtocolLink.h src/app/ProtocolLink.cpp src/app/ProtocolInbox.h src/app/ProtocolInbox.cpp
    src/app/OutboundDiagnostics.h src/app/OutboundDiagnostics.cpp
    src/app/ServiceDiagnostics.h src/app/ServiceDiagnostics.cpp
    src/app/WebsiteTests.h src/app/WebsiteTests.cpp src/app/GroupDelayTests.h src/app/GroupDelayTests.cpp
    src/core/CoreDtos.h src/core/CoreDtos.cpp src/core/CoreApi.h src/core/MihomoClient.h src/core/MihomoClient.cpp src/core/MihomoEndpoints.cpp src/core/CoreManager.h src/core/CoreManager.cpp
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
    src/platform/GlobalHotkeys.h
    src/diagnostics/Diagnostics.h src/diagnostics/Diagnostics.cpp src/diagnostics/ProcessMetrics.h)
target_include_directories(mlash_engine PUBLIC src)
target_link_libraries(mlash_engine PUBLIC mlash_domain Qt6::Core Qt6::Network yaml-cpp::yaml-cpp libzstd_static ${MLASH_ZLIB_TARGET})
target_compile_definitions(mlash_engine PUBLIC MLASH_VERSION="${PROJECT_VERSION}")
if(WIN32)
    target_sources(mlash_engine PRIVATE src/platform/windows/TunNative.h src/platform/windows/TunNative.cpp src/platform/windows/WindowChromeNative.h src/platform/windows/WindowChromeNative.cpp)
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

add_library(mlash_ui STATIC src/ui/Route.h src/ui/Route.cpp src/ui/ViewIds.h src/ui/ViewIds.cpp src/ui/StatusSentence.h src/ui/StatusSentence.cpp src/ui/FeedbackSync.h src/ui/FeedbackSync.cpp src/ui/PageLifecycle.h src/ui/PageLifecycle.cpp src/ui/ToolWindowRegistry.h src/ui/ToolWindowRegistry.cpp src/ui/StatusShape.h src/ui/StatusShape.cpp src/ui/SkeletonBlock.h src/ui/SkeletonBlock.cpp src/ui/UsageDelegate.h src/ui/UsageDelegate.cpp src/ui/MenuSection.h src/ui/ContextNote.h src/ui/SettingsSearch.h src/ui/SettingsSearch.cpp src/ui/CompactSpinBox.h src/ui/MainWindow.h src/ui/MainWindow.cpp src/ui/MainWindowChrome.cpp src/ui/MainWindowData.cpp src/ui/MainWindowFeedback.cpp src/ui/MainWindowState.cpp src/ui/MainWindowNavigation.cpp src/ui/MainWindowHome.cpp src/ui/MainWindowSettings.cpp src/ui/MainWindowProxy.cpp src/ui/MainWindowProfiles.cpp src/ui/DashboardCardButton.h src/ui/ChevronToolButton.h src/ui/MenuPushButton.h src/ui/WindowUiFactories.h src/ui/SettingRow.h src/ui/ProfileRowDelegate.h src/ui/ProfileCardWidget.h src/ui/ProfileCardWidget.cpp src/ui/GroupCardDelegate.h src/ui/NodeNameDelegate.h src/ui/NodeCardDelegate.h src/ui/NodeCardView.h src/ui/ProxySwitch.h src/ui/DetailsDrawer.h src/ui/NodeRowModel.h src/ui/NodeRowDelegate.h src/ui/NodeRowView.h src/ui/WindowChrome.h src/ui/NodeFilter.cpp
    src/ui/ProviderDefinitionDialog.h src/ui/YamlFormEditor.h src/ui/YamlFormEditor.cpp
    src/ui/ConnectionDetailsDialog.h src/ui/ConnectionDetailsDialog.cpp src/ui/ProviderDefinitionDialog.cpp src/ui/ProviderPolicyDialog.cpp
    src/ui/TaskWorkspace.h src/ui/TaskWorkspace.cpp
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
    src/ui/AppUpdateDialog.h src/ui/AppUpdateDialog.cpp
    src/ui/RemoteBackupDialog.h src/ui/RemoteBackupDialog.cpp
    src/ui/GeoDataDialog.h src/ui/GeoDataDialog.cpp
    src/ui/HomeMetricsView.h src/ui/HomeMetricsView.cpp
    src/ui/OutboundDialog.h src/ui/OutboundDialog.cpp
    src/ui/HotkeysDialog.h src/ui/HotkeysDialog.cpp
    src/ui/ConnectionOptions.h src/ui/ConnectionOptions.cpp src/ui/ConnectionColumnsPopup.h src/ui/ConnectionColumnsPopup.cpp
    src/ui/ActivityTableModel.h src/ui/ActivityTableModel.cpp resources/brand.qrc resources/flags.qrc
    src/ui/ImportDialog.h src/ui/ImportDialog.cpp src/ui/ProxyTableModel.h src/ui/ProxyTableModel.cpp
    src/ui/NodePresentation.h src/ui/NodePresentation.cpp
    src/ui/WidgetStyle.h src/ui/WidgetStyle.cpp
    src/ui/UiTheme.h src/ui/UiTheme.cpp
    src/ui/design/UiDesignTokens.h src/ui/design/ShadowrocketStyle.h
    src/ui/design/ShadowrocketStyleSheet.h src/ui/design/ShadowrocketStyleSheet.cpp resources/fonts.qrc
    src/ui/UiIcons.h src/ui/UiIcons.cpp resources/services.qrc
    src/ui/TrayController.h src/ui/TrayController.cpp)
target_link_libraries(mlash_ui PUBLIC mlash_engine Qt6::Widgets mlash_qrcode)
add_executable(Mlash WIN32 src/main.cpp src/diagnostics/QualificationRun.h src/diagnostics/QualificationRun.cpp)
target_link_libraries(Mlash PRIVATE mlash_ui)
if(WIN32)
    configure_file(resources/mlash_version.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/mlash_version.h" @ONLY)
    target_sources(Mlash PRIVATE resources/mlash.rc)
    set_source_files_properties(resources/mlash.rc PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/docs/design/u32-assets/mlash-app-icon.ico;${CMAKE_CURRENT_BINARY_DIR}/generated/mlash_version.h")
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
    add_executable(mlash_domain_contract_tests tests/DomainContractTests.cpp)
    target_link_libraries(mlash_domain_contract_tests PRIVATE mlash_domain Qt6::Test)
    add_test(NAME domain_contract_tests COMMAND mlash_domain_contract_tests -o "${CMAKE_BINARY_DIR}/domain-contract-tests.xml,xml")
    set_tests_properties(domain_contract_tests PROPERTIES TIMEOUT 10 LABELS "unit")
    add_executable(mlash_core_dto_tests tests/CoreDtoTests.cpp)
    target_link_libraries(mlash_core_dto_tests PRIVATE mlash_engine Qt6::Test)
    add_test(NAME core_dto_tests COMMAND mlash_core_dto_tests -o "${CMAKE_BINARY_DIR}/core-dto-tests.xml,xml")
    set_tests_properties(core_dto_tests PROPERTIES TIMEOUT 10 LABELS "unit;integration")
    add_executable(mlash_page_router_tests tests/PageRouterTests.cpp src/ui/PageLifecycle.h src/ui/PageLifecycle.cpp)
    target_include_directories(mlash_page_router_tests PRIVATE src)
    target_link_libraries(mlash_page_router_tests PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME page_router_tests COMMAND mlash_page_router_tests -o "${CMAKE_BINARY_DIR}/page-router-tests.xml,xml")
    set_tests_properties(page_router_tests PROPERTIES TIMEOUT 10 LABELS "unit;ui")
    add_executable(mlash_tun_tests tests/TunTests.cpp tests/MockHttp.cpp)
    target_link_libraries(mlash_tun_tests PRIVATE mlash_ui Qt6::Test)
    target_compile_definitions(mlash_tun_tests PRIVATE MLASH_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_dependencies(mlash_tun_tests fake_core)
    add_test(NAME tun_tests COMMAND mlash_tun_tests -o "${CMAKE_BINARY_DIR}/tun-tests.xml,xml")
    set_tests_properties(tun_tests PROPERTIES TIMEOUT 75 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(mlash_tests tests/EngineTests.cpp tests/MockHttp.h tests/MockHttp.cpp)
    target_link_libraries(mlash_tests PRIVATE mlash_engine Qt6::Test)
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
    add_executable(mlash_ui_model_tests tests/UiModelTests.cpp src/ui/ProxyTableModel.h src/ui/ProxyTableModel.cpp)
    target_link_libraries(mlash_ui_model_tests PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
    target_include_directories(mlash_ui_model_tests PRIVATE src)
    add_test(NAME ui_model_tests COMMAND mlash_ui_model_tests -o "${CMAKE_BINARY_DIR}/ui-model-tests.xml,xml")
    set_tests_properties(ui_model_tests PROPERTIES TIMEOUT 15 LABELS "unit;ui")
    add_test(NAME gui_smoke COMMAND Mlash --smoke-test --data-dir "${CMAKE_BINARY_DIR}/smoke-data")
    set_tests_properties(gui_smoke PROPERTIES TIMEOUT 15 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(mlash_flat_workspace_tests tests/FlatWorkspaceTests.cpp)
    target_link_libraries(mlash_flat_workspace_tests PRIVATE mlash_ui Qt6::Test)
    add_test(NAME flat_workspace_tests COMMAND mlash_flat_workspace_tests -o "${CMAKE_BINARY_DIR}/flat-workspace-tests.xml,xml")
    set_tests_properties(flat_workspace_tests PROPERTIES TIMEOUT 45 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(mlash_ui_tests tests/UiTests.cpp tests/MockHttp.cpp)
    add_executable(mlash_workflow_depth_tests tests/WorkflowDepthTests.cpp tests/MockHttp.cpp)
    target_link_libraries(mlash_workflow_depth_tests PRIVATE mlash_ui Qt6::Test)
    add_test(NAME workflow_depth_tests COMMAND mlash_workflow_depth_tests -o "${CMAKE_BINARY_DIR}/workflow-depth-tests.xml,xml")
    set_tests_properties(workflow_depth_tests PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    target_link_libraries(mlash_ui_tests PRIVATE mlash_ui Qt6::Test)
    target_compile_definitions(mlash_ui_tests PRIVATE MLASH_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    # Separate the long end-to-end fixture from independent components. Keep
    # each existing 30s budget; no runtime or identity checks are relaxed.
    add_test(NAME prototype_ui_tests COMMAND mlash_ui_tests prototypeSystemPagesAndResponsiveInspector -o "${CMAKE_BINARY_DIR}/prototype-ui-tests.xml,xml")
    set_tests_properties(prototype_ui_tests PROPERTIES TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME navigation_contract_tests COMMAND mlash_ui_tests
        routesCarryHomeAnchorAndSettingsCategory titleSlotIsSingleAndWindowTitleFollowsRoute sidebarStatusIsPermanentAndSharesTrayWording
        -o "${CMAKE_BINARY_DIR}/navigation-contract-tests.xml,xml")
    set_tests_properties(navigation_contract_tests PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME ui_interactions COMMAND mlash_ui_tests currentGroupSelectionAndNativePreview -o "${CMAKE_BINARY_DIR}/ui-tests.xml,xml")
    set_tests_properties(ui_interactions PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME profile_rename_ui COMMAND mlash_ui_tests subscriptionCardEditingAndReturnPreserveObject -o "${CMAKE_BINARY_DIR}/profile-rename-ui.xml,xml")
    add_test(NAME subscription_cards_ui COMMAND mlash_ui_tests quickImportCreatesProfileAndOffersAdvancedWindow subscriptionCardsRouteActionsWithoutChangingUse subscriptionOrderKeepsSelectedObject subscriptionRollbackRequiresExplicitConfirmation -o "${CMAKE_BINARY_DIR}/subscription-cards-ui.xml,xml")
    set_tests_properties(subscription_cards_ui PROPERTIES TIMEOUT 20 ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "ui")
    set_tests_properties(profile_rename_ui PROPERTIES TIMEOUT 20 ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "ui")
    add_test(NAME profile_navigation_ui COMMAND mlash_ui_tests subscriptionCardReturnRestoresGridPosition changingSectionRestoresNetworkSettingsDraft settingsSearchDoesNotRearrangeDirtyNetworkTask changingSectionRestoresEmbeddedTaskDraft settingsSearchKeepsDirtyTaskVisible -o "${CMAKE_BINARY_DIR}/profile-navigation-ui.xml,xml")
    set_tests_properties(profile_navigation_ui PROPERTIES TIMEOUT 25 ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "ui")
    add_test(NAME node_view_navigation_ui COMMAND mlash_ui_tests nodeViewSwitchAndBookmarkUseVisibleSelection -o "${CMAKE_BINARY_DIR}/node-view-navigation-ui.xml,xml")
    set_tests_properties(node_view_navigation_ui PROPERTIES TIMEOUT 15 ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "ui")
    add_test(NAME profile_import_ui COMMAND mlash_ui_tests fileProfileOffersSourceSyncAndReplacement -o "${CMAKE_BINARY_DIR}/profile-import-ui.xml,xml")
    set_tests_properties(profile_import_ui PROPERTIES TIMEOUT 25 ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "ui")
    add_test(NAME settings_inline_ui COMMAND mlash_ui_tests desktopPreferencesAndEnvironmentCopy settingsRowsShowCurrentValue -o "${CMAKE_BINARY_DIR}/settings-inline-ui.xml,xml")
    set_tests_properties(settings_inline_ui PROPERTIES TIMEOUT 25 ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "ui")
    add_test(NAME tray_quick_menu_ui COMMAND mlash_ui_tests trayQuickSubscriptionsKeepCurrentAndBoundMenu -o "${CMAKE_BINARY_DIR}/tray-quick-menu-ui.xml,xml")
    set_tests_properties(tray_quick_menu_ui PROPERTIES TIMEOUT 20 ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "ui")
    add_test(NAME log_pause_clear_ui COMMAND mlash_ui_tests logPauseClearAndDiagnosticPrivacy -o "${CMAKE_BINARY_DIR}/log-pause-clear-ui.xml,xml")
    set_tests_properties(log_pause_clear_ui PROPERTIES TIMEOUT 20 ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "ui" RESOURCE_LOCK system_clipboard)
    add_test(NAME ui_components COMMAND mlash_ui_tests
        compactNodeDelayDoesNotSelectAndProfileBrowseDoesNotApply popupSurfacesWithoutMainWindow groupPickerSearchKeepsStablePopupAndSelection desktopPreferencesAndEnvironmentCopy responsivePageGeometry
        historyReadingAndDraftFeedback logPauseClearAndDiagnosticPrivacy profilePolicyDialogPersistsOnlyOnSave
        providerAndCompatibilityDialogsRespectVersions
        trayGroupDwellCacheAndSelection trayAppearancePriorityAndResources lightweightLifecycleAndQuickReopen
        importValidationAndSecretVisibility failedImportRetainsDraftUntilExplicitRetry unchangedSnapshotsPreserveRowsAndChoices refinedTaskGeometryAndSemantics refinedContextFitsAutomaticGroupsAndLargeText emptyStateCannotIssueInvalidCommands fileProfileOffersSourceSyncAndReplacement
        coherentActionsAndBilingualRuleTokens nodeLocationHints dashboardTabsAndSettingsFocus quietMouseFocusAndGroupedSettings emptyStatesOfferFocusableNextStep
        -o "${CMAKE_BINARY_DIR}/ui-components.xml,xml")
    # 该系统剪贴板是进程外共享资源：并行 ctest 时与其他用例互相覆盖。
    # 用 RESOURCE_LOCK 串行化，而不是放宽断言。
    set_tests_properties(ui_components PROPERTIES TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen" RESOURCE_LOCK system_clipboard)
    add_test(NAME status_bar_tests COMMAND mlash_ui_tests persistentStatusBarDoesNotMovePages -o "${CMAKE_BINARY_DIR}/status-bar-tests.xml,xml")
    # P4.1 反馈路由：页面级结果不进状态栏、告警与最近操作两段并存、归属表逐条固定。
    add_test(NAME feedback_routing_tests COMMAND mlash_ui_tests operationResultsGoToTheirOwnPage feedbackTargetsStayExplicit -o "${CMAKE_BINARY_DIR}/feedback-routing-tests.xml,xml")
    set_tests_properties(feedback_routing_tests PROPERTIES TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    # S1 页面生命周期：进入/离开各触发一次，视图 2 归一化不留双份副作用。
    add_test(NAME page_lifecycle_tests COMMAND mlash_ui_tests viewSwitchingKeepsControllerSideEffects -o "${CMAKE_BINARY_DIR}/page-lifecycle-tests.xml,xml")
    set_tests_properties(page_lifecycle_tests PROPERTIES TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    # S2 工具窗口：同键复用、对象删除关闭、失效指针不复用。
    add_test(NAME tool_window_tests COMMAND mlash_ui_tests toolWindowsAreReusedPerObject -o "${CMAKE_BINARY_DIR}/tool-window-tests.xml,xml")
    set_tests_properties(tool_window_tests PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    # V1 状态形状编码与骨架屏：形状互不相同，加载态与空态可区分。
    add_test(NAME status_shape_tests COMMAND mlash_ui_tests statusStatesUseDistinctShapes skeletonBlockOccupiesContentSpace rulesTableHasSkeletonWired -o "${CMAKE_BINARY_DIR}/status-shape-tests.xml,xml")
    set_tests_properties(status_shape_tests PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    # V2 订阅用量条：有额度显示、无额度整列隐藏、状态列后移不错位。
    add_test(NAME profile_usage_tests COMMAND mlash_ui_tests profileUsageColumnFollowsAvailableData -o "${CMAKE_BINARY_DIR}/profile-usage-tests.xml,xml")
    set_tests_properties(profile_usage_tests PROPERTIES TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    # V3 设置行内当前值：只加到值不在控件里的行，且 840×560 不出现横向滚动。
    add_test(NAME settings_value_tests COMMAND mlash_ui_tests settingsRowsShowCurrentValue -o "${CMAKE_BINARY_DIR}/settings-value-tests.xml,xml")
    set_tests_properties(settings_value_tests PROPERTIES TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    # V4 菜单分组标题：五组各有标题，标题不可触发。
    add_test(NAME menu_section_tests COMMAND mlash_ui_tests menuSectionsLabelGroupsWithoutAddingCommands -o "${CMAKE_BINARY_DIR}/menu-section-tests.xml,xml")
    set_tests_properties(menu_section_tests PROPERTIES TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    # V5 首页控制区：组选路说明、控制区失败原因。
    add_test(NAME home_control_tests COMMAND mlash_ui_tests homeControlAreaStatesRoutingRulesAndFailure -o "${CMAKE_BINARY_DIR}/home-control-tests.xml,xml")
    set_tests_properties(home_control_tests PROPERTIES TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
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
    set_tests_properties(node_links_tests PROPERTIES TIMEOUT 75 ENVIRONMENT "QT_QPA_PLATFORM=offscreen" RESOURCE_LOCK system_clipboard)
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
    add_executable(mlash_app_update_tests tests/AppUpdateTests.cpp)
    target_link_libraries(mlash_app_update_tests PRIVATE mlash_engine Qt6::Test)
    add_test(NAME app_update_tests COMMAND mlash_app_update_tests)
    set_tests_properties(app_update_tests PROPERTIES TIMEOUT 15 LABELS "unit")
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
    set_tests_properties(connection_view_tests PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen" RESOURCE_LOCK system_clipboard)
    add_test(NAME connection_return_ui COMMAND mlash_connection_view_tests returningToConnectionsRestoresFilterSelectionAndPosition -o "${CMAKE_BINARY_DIR}/connection-return-ui.xml,xml")
    set_tests_properties(connection_return_ui PROPERTIES TIMEOUT 20 ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "ui")
    add_test(NAME log_commands_ui COMMAND mlash_connection_view_tests logCommandsHaveOneVisibleEntryAndCopySelection -o "${CMAKE_BINARY_DIR}/log-commands-ui.xml,xml")
    set_tests_properties(log_commands_ui PROPERTIES TIMEOUT 20 ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "ui" RESOURCE_LOCK system_clipboard)
    add_test(NAME connection_columns_ui COMMAND mlash_connection_view_tests optionalColumnsAndLegacyPreferences searchFlagsErrorsAndColumnPersistence -o "${CMAKE_BINARY_DIR}/connection-columns-ui.xml,xml")
    set_tests_properties(connection_columns_ui PROPERTIES TIMEOUT 15 ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "ui")
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
    # 2026-11-02：45s 与实际耗时长期不符（基线 57.3s、本轮 49.4~64.4s），整轮里必然撞限被杀，
    # 而被杀的是用例整体、不是某条断言。放宽到 100s 留出余量；它仍是上限，不是性能断言。
    set_tests_properties(appearance_tests PROPERTIES TIMEOUT 100 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
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
        add_test(NAME architecture_boundaries COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/test_architecture_boundaries.py")
        add_test(NAME release_tools COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/test_release_tools.py")
        set_tests_properties(release_tools PROPERTIES TIMEOUT 20)
        set_tests_properties(architecture_boundaries PROPERTIES LABELS "unit")
        set_tests_properties(release_tools PROPERTIES LABELS "integration")
    endif()
    set_tests_properties(engine core_readiness_tests official_core_tests service_tests home_probe_tests
        home_metrics_tests website_tests dns_tests protocol_tests mrs_tests geo_tests
        PROPERTIES LABELS "integration")
    set_tests_properties(tun_tests workflow_depth_tests remote_backup_tests core_update_tests
        proxy_chain_tests proxy_edits_tests provider_edits_tests rules_tests backup_tests
        yaml_forms_tests node_links_tests outbound_tests proxy_bypass_tests hotkey_tests
        PROPERTIES LABELS "scenario")
    set_tests_properties(flat_workspace_tests prototype_ui_tests navigation_contract_tests
        ui_interactions ui_components status_bar_tests feedback_routing_tests
        page_lifecycle_tests tool_window_tests status_shape_tests profile_usage_tests
        settings_value_tests menu_section_tests home_control_tests appearance_tests
        appearance_prefs_tests connection_view_tests PROPERTIES LABELS "ui")
    if(NOT WIN32)
        set_tests_properties(ui_refinement_tests PROPERTIES LABELS "ui")
    endif()
    if(WIN32)
        set_tests_properties(system_test_guard PROPERTIES LABELS "system-gated")
    endif()
    set_tests_properties(gui_smoke PROPERTIES LABELS "ui")
    # Legacy mixed suites retain both labels until their cases move to focused targets.
    set_property(TEST engine ui_components appearance_tests APPEND PROPERTY LABELS "scenario")
    set_property(TEST website_tests APPEND PROPERTY LABELS "ui")
endif()
