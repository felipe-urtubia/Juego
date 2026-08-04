cmake_minimum_required(VERSION 3.16)

project(FootballManagerGame LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MINGW)
    set(CMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS 1)
    set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_OBJECTS 1)
    set(CMAKE_C_USE_RESPONSE_FILE_FOR_INCLUDES 1)
    set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES 1)
    set(CMAKE_C_RESPONSE_FILE_LINK_FLAG "@")
    set(CMAKE_CXX_RESPONSE_FILE_LINK_FLAG "@")
endif()

# Career orchestration, state adapters and weekly simulation.
set(FM_CAREER_SOURCES
    src/career/analytics_service.cpp
    src/career/app_services.cpp
    src/career/career_manager.cpp
    src/career/career_modules.cpp
    src/career/career_reports.cpp
    src/career/career_runtime.cpp
    src/career/career_state.cpp
    src/career/career_service.cpp
    src/career/career_support.cpp
    src/career/dressing_room_service.cpp
    src/career/game_events_system.cpp
    src/career/gameplay_reports.cpp
    src/career/inbox_service.cpp
    src/career/manager_advice.cpp
    src/career/match_analysis_store.cpp
    src/career/match_center_service.cpp
    src/career/medical_service.cpp
    src/career/player_development.cpp
    src/career/season_flow_controller.cpp
    src/career/season_service.cpp
    src/career/season_transition.cpp
    src/career/staff_service.cpp
    src/career/sub_services.cpp
    src/career/team_management.cpp
    src/career/transfer_briefing.cpp
    src/career/week_simulation.cpp
    src/career/weekly_focus_service.cpp
    src/career/world_state_service.cpp
)

# CPU squad, tactic and market behavior.
set(FM_AI_SOURCES
    src/ai/ai_match_manager.cpp
    src/ai/ai_squad_planner.cpp
    src/ai/ai_transfer_manager.cpp
    src/ai/team_ai.cpp
)

set(FM_COMPETITION_SOURCES
    src/competition/competition.cpp
    src/competition/league_registry.cpp
)

set(FM_ENGINE_SOURCES
    src/engine/career_state.cpp
    src/engine/front_menu.cpp
    src/engine/game_engine.cpp
    src/engine/game_settings.cpp
    src/engine/league_table.cpp
    src/engine/models.cpp
    src/engine/team_selection.cpp
    src/engine/social_system.cpp
    src/engine/manager_stress.cpp
    src/engine/rival_ai.cpp
    src/engine/rivalry_system.cpp
    src/engine/team_personality.cpp
    src/engine/facilities_system.cpp
    src/engine/debt_system.cpp
)

set(FM_APP_SOURCES
    src/engine/game_controller.cpp
)

set(FM_IO_SOURCES
    src/io/io.cpp
    src/io/save_manager.cpp
    src/io/save_serialization.cpp
)

set(FM_SIMULATION_SOURCES
    src/simulation/fatigue_engine.cpp
    src/simulation/match_event_generator.cpp
    src/simulation/match_event_resolver.cpp
    src/simulation/match_events.cpp
    src/simulation/match_context.cpp
    src/simulation/match_engine.cpp
    src/simulation/match_phase.cpp
    src/simulation/match_postprocess.cpp
    src/simulation/match_report.cpp
    src/simulation/match_resolution.cpp
    src/simulation/match_stats.cpp
    src/simulation/match_tactics.cpp
    src/simulation/morale_engine.cpp
    src/simulation/player_condition.cpp
    src/simulation/simulation.cpp
    src/simulation/tactics_engine.cpp
    src/simulation/match_momentum.cpp
    src/simulation/match_center.cpp
    src/simulation/match_center_state.cpp
    src/simulation/match_center_renderer.cpp
    src/simulation/match_commentary.cpp
    src/simulation/player_rating_system.cpp
)

set(FM_TRANSFER_SOURCES
    src/transfers/negotiation_system.cpp
    src/transfers/transfer_market.cpp
)

set(FM_DEVELOPMENT_SOURCES
    src/development/monthly_development.cpp
    src/development/player_progression_system.cpp
    src/development/training_impact_system.cpp
    src/development/youth_generation_system.cpp
)

set(FM_FINANCE_SOURCES
    src/finance/finance_system.cpp
)

set(FM_SUPPORT_SOURCES
    src/utils/utils.cpp
    src/utils/career_safety.cpp
    src/utils/localization.cpp
    src/utils/logger.cpp
    src/utils/event_system.cpp
)

set(FM_VALIDATOR_SOURCES
    src/validators/validators.cpp
)

set(FM_CORE_SOURCES
    ${FM_CAREER_SOURCES}
    ${FM_AI_SOURCES}
    ${FM_COMPETITION_SOURCES}
    ${FM_ENGINE_SOURCES}
    ${FM_IO_SOURCES}
    ${FM_SIMULATION_SOURCES}
    ${FM_TRANSFER_SOURCES}
    ${FM_DEVELOPMENT_SOURCES}
    ${FM_FINANCE_SOURCES}
    ${FM_SUPPORT_SOURCES}
)

set(FM_CONSOLE_UI_SOURCES
    src/ui/career_reports_ui.cpp
    src/ui/cup_ui.cpp
    src/ui/market_ui.cpp
    src/ui/team_ui.cpp
    src/ui/ui.cpp
    src/ui/global_search.cpp
    src/ui/context_menu.cpp
    src/ui/economy_fairplay.cpp
)

set(FM_CONSOLE_ENTRY_SOURCES
    src/main.cpp
)

set(FM_GUI_SOURCES)
set(FM_GUI_ENTRY_SOURCES ${FM_CONSOLE_ENTRY_SOURCES})
if(WIN32)
    list(APPEND FM_GUI_SOURCES
        src/gui/gui_audio.cpp
        src/gui/gui.cpp
        src/gui/gui_actions.cpp
        src/gui/gui_layout.cpp
        src/gui/gui_runtime.cpp
        src/gui/gui_shared.cpp
        src/gui/gui_team_logos.cpp
        src/gui/gui_view_common.cpp
        src/gui/gui_view_menu.cpp
        src/gui/gui_view_competition.cpp
        src/gui/gui_view_management.cpp
        src/gui/gui_view_overview.cpp
        src/gui/gui_views.cpp
        src/ui/keyboard_shortcuts.cpp
    )
    set(FM_GUI_ENTRY_SOURCES
        src/winmain.cpp
    )
endif()

foreach(source_list
        FM_CAREER_SOURCES
        FM_AI_SOURCES
        FM_COMPETITION_SOURCES
        FM_ENGINE_SOURCES
        FM_APP_SOURCES
        FM_IO_SOURCES
        FM_SIMULATION_SOURCES
        FM_TRANSFER_SOURCES
        FM_DEVELOPMENT_SOURCES
        FM_FINANCE_SOURCES
        FM_SUPPORT_SOURCES
        FM_VALIDATOR_SOURCES
        FM_CORE_SOURCES
        FM_CONSOLE_UI_SOURCES
        FM_CONSOLE_ENTRY_SOURCES
        FM_GUI_SOURCES
        FM_GUI_ENTRY_SOURCES)
    list(REMOVE_DUPLICATES ${source_list})
endforeach()

set(FM_FOOTBALLMANAGER_SOURCES
    ${FM_CORE_SOURCES}
    ${FM_APP_SOURCES}
    ${FM_GUI_ENTRY_SOURCES}
    ${FM_CONSOLE_UI_SOURCES}
    ${FM_VALIDATOR_SOURCES}
    ${FM_GUI_SOURCES}
)
set(FM_FOOTBALLMANAGER_CLI_SOURCES
    ${FM_CORE_SOURCES}
    ${FM_APP_SOURCES}
    ${FM_CONSOLE_ENTRY_SOURCES}
    ${FM_CONSOLE_UI_SOURCES}
    ${FM_VALIDATOR_SOURCES}
)
set(FM_FOOTBALLMANAGER_TEST_SOURCES
    tests/team_ai_momentum_tests.cpp
    tests/project_tests.cpp
    tests/match_center_tests.cpp
    ${FM_CORE_SOURCES}
    ${FM_VALIDATOR_SOURCES}
    src/ui/economy_fairplay.cpp
    ${FM_GUI_SOURCES}
)
list(REMOVE_DUPLICATES FM_FOOTBALLMANAGER_SOURCES)
list(REMOVE_DUPLICATES FM_FOOTBALLMANAGER_CLI_SOURCES)
list(REMOVE_DUPLICATES FM_FOOTBALLMANAGER_TEST_SOURCES)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES
    ${FM_FOOTBALLMANAGER_SOURCES}
    ${FM_FOOTBALLMANAGER_CLI_SOURCES}
    ${FM_FOOTBALLMANAGER_TEST_SOURCES}
)

if(WIN32)
    add_executable(FootballManager WIN32
        ${FM_FOOTBALLMANAGER_SOURCES}
    )
else()
    add_executable(FootballManager
        ${FM_FOOTBALLMANAGER_SOURCES}
    )
endif()

function(fm_configure_target target_name)
    target_include_directories(${target_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    if(WIN32)
        target_link_libraries(${target_name} PRIVATE comctl32 gdi32 winmm)
    endif()

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    if(MINGW)
    target_link_options(${target_name} PRIVATE
        -static
        -static-libgcc
        -static-libstdc++
    )
    endif()

    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${target_name}>
            ${CMAKE_SOURCE_DIR}/$<TARGET_FILE_NAME:${target_name}>
        VERBATIM
    )
endfunction()

fm_configure_target(FootballManager)
if(WIN32)
    set_target_properties(FootballManager PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if(WIN32)
    add_executable(FootballManagerCLI
        ${FM_FOOTBALLMANAGER_CLI_SOURCES}
    )
    target_compile_definitions(FootballManagerCLI PRIVATE FM_CONSOLE_DEFAULT=1)
    fm_configure_target(FootballManagerCLI)
endif()

if(BUILD_TESTING)
    add_executable(FootballManagerTests
        ${FM_FOOTBALLMANAGER_TEST_SOURCES}
    )
    fm_configure_target(FootballManagerTests)
    add_test(NAME FootballManagerTests COMMAND FootballManagerTests)
    set_tests_properties(FootballManagerTests PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()

