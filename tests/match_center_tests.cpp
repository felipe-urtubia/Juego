#include "simulation/match_center_state.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace match_center_tests {

namespace {

int testsPassed = 0;
int testsFailed = 0;

void assertTest(bool condition, const std::string& name) {
    if (condition) {
        std::cout << "[PASS] " << name << "\n";
        testsPassed++;
    } else {
        std::cout << "[FAIL] " << name << "\n";
        testsFailed++;
    }
}


void testInitialState() {

    match_center::LiveState state;

    assertTest(
        state.minute == 0,
        "Match Center inicia minuto 0"
    );

    assertTest(
        state.homeGoals == 0 &&
        state.awayGoals == 0,
        "Match Center inicia sin goles"
    );
}


void testApplyGoalEvent() {

    MatchEvent event;

    event.minute = 45;
    event.teamName = "Local";
    event.description = "Gol de prueba";

    event.impact.homeGoalsDelta = 1;

    match_center::LiveState state;

    match_center::applyEventImpact(
        state,
        event
    );

    assertTest(
        state.minute == 45,
        "Evento actualiza minuto"
    );

    assertTest(
        state.homeGoals == 1,
        "Evento agrega gol"
    );
}


void testStatisticsImpact() {

    MatchEvent event;

    event.minute = 30;

    event.impact.homeShotsDelta = 4;
    event.impact.homeShotsOnTargetDelta = 2;
    event.impact.homeCornersDelta = 3;
    event.impact.homeExpectedGoalsDelta = 0.65;


    match_center::LiveState state;


    match_center::applyEventImpact(
        state,
        event
    );


    assertTest(
        state.homeShots == 4,
        "Actualiza tiros"
    );

    assertTest(
        state.homeShotsOnTarget == 2,
        "Actualiza tiros al arco"
    );

    assertTest(
        state.homeCorners == 3,
        "Actualiza corners"
    );

    assertTest(
        std::abs(state.homeExpectedGoals - 0.65) < 0.001,
        "Actualiza xG"
    );
}


void testFinalState() {

    MatchResult result;

    result.homeGoals = 2;
    result.awayGoals = 1;

    result.stats.homeShots = 12;
    result.stats.awayShots = 7;

    result.stats.homeExpectedGoals = 1.9;
    result.stats.awayExpectedGoals = 0.8;


    auto state =
        match_center::makeFinalState(result);


    assertTest(
        state.homeGoals == 2 &&
        state.awayGoals == 1,
        "Estado final conserva marcador"
    );


    assertTest(
        state.homeShots == 12 &&
        state.awayShots == 7,
        "Estado final conserva tiros"
    );


    assertTest(
        std::abs(state.homeExpectedGoals - 1.9) < 0.001,
        "Estado final conserva xG"
    );
}

}


void runMatchCenterTests() {

    std::cout << "\n=== Match Center Tests ===\n";


    testInitialState();
    testApplyGoalEvent();
    testStatisticsImpact();
    testFinalState();


    std::cout
        << "Match Center tests completados: "
        << testsPassed
        << " correctos, "
        << testsFailed
        << " fallidos\n";


    if (testsFailed > 0) {
        throw std::runtime_error(
            "Existen errores en las pruebas de Match Center"
        );
    }
}

}  // namespace match_center_tests

void runMatchCenterTests() {
    match_center_tests::runMatchCenterTests();
}
