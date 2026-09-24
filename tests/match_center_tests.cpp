#include "simulation/match_center.h"
#include "simulation/match_center_renderer.h"
#include "simulation/match_center_state.h"

#include <cmath>
#include <iostream>
#include <sstream>
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


void testEventTextFormatting() {

    MatchEvent described;
    described.minute = 12;
    described.teamName = "Local";
    described.description = "Remate peligroso";

    MatchEvent playerOnly;
    playerOnly.minute = 34;
    playerOnly.playerName = "Jugador Prueba";

    MatchEvent typeOnly;
    typeOnly.minute = 56;
    typeOnly.type = MatchEventType::Corner;

    assertTest(
        match_center::eventText(described) == "12' [Local] Remate peligroso",
        "eventText usa minuto, equipo y descripcion"
    );

    assertTest(
        match_center::eventText(playerOnly) == "34' Jugador Prueba",
        "eventText usa jugador cuando no hay descripcion"
    );

    assertTest(
        match_center::eventText(typeOnly).find("56' ") == 0 &&
        match_center::eventText(typeOnly).size() > 4,
        "eventText usa etiqueta del tipo como respaldo"
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
    event.impact.homeDangerousAttacksDelta = 2;
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
        state.homeDangerousAttacks == 2,
        "Actualiza ataques peligrosos"
    );

    assertTest(
        std::abs(state.homeExpectedGoals - 0.65) < 0.001,
        "Actualiza xG"
    );
}


void testLivePossessionByPhases() {

    MatchResult result;

    MatchPhaseReport firstPhase;
    firstPhase.minuteStart = 1;
    firstPhase.minuteEnd = 15;
    firstPhase.homePossessionShare = 60;

    MatchPhaseReport secondPhase;
    secondPhase.minuteStart = 16;
    secondPhase.minuteEnd = 30;
    secondPhase.homePossessionShare = 40;

    result.timeline.phases = {
        firstPhase,
        secondPhase
    };

    match_center::LiveState state;

    match_center::updateLivePossession(
        state,
        result,
        0
    );

    assertTest(
        state.homePossession == 50 &&
        state.awayPossession == 50,
        "Posesion en vivo inicia neutral"
    );

    match_center::updateLivePossession(
        state,
        result,
        20
    );

    assertTest(
        state.homePossession == 55 &&
        state.awayPossession == 45,
        "Posesion en vivo pondera fases hasta el minuto visible"
    );
}

void testHeatMapCounting() {

    Team home;
    Team away;
    home.name = "Local";
    away.name = "Visita";

    match_center::LiveState state;

    MatchEvent homeProgression;
    homeProgression.teamName = home.name;
    homeProgression.type = MatchEventType::Progression;
    homeProgression.zone = MatchFieldZone::MiddleCenter;
    match_center::updateHeatMap(state, homeProgression, home, away);

    MatchEvent awayBuildUp;
    awayBuildUp.teamName = away.name;
    awayBuildUp.type = MatchEventType::AttackBuildUp;
    awayBuildUp.zone = MatchFieldZone::FinalRight;
    match_center::updateHeatMap(state, awayBuildUp, home, away);

    MatchEvent goal;
    goal.teamName = home.name;
    goal.type = MatchEventType::Goal;
    goal.zone = MatchFieldZone::MiddleCenter;
    match_center::updateHeatMap(state, goal, home, away);

    assertTest(
        state.homeHeatMap[4] == 1,
        "Mapa de calor cuenta progresion local una vez"
    );

    assertTest(
        state.awayHeatMap[8] == 1,
        "Mapa de calor cuenta ataque visitante en zona final derecha"
    );
}


void testLiveMomentumAndLabels() {

    Team home;
    Team away;
    home.name = "Local";
    away.name = "Visita";

    match_center::LiveState state;

    MatchEvent homeGoal;
    homeGoal.teamName = home.name;
    homeGoal.type = MatchEventType::Goal;

    match_center::updateLiveMomentum(
        state,
        homeGoal,
        home,
        away
    );

    assertTest(
        state.momentumScore > 0,
        "Gol local genera momentum favorable al local"
    );

    match_center::LiveState disciplinaryState;

    MatchEvent homeRedCard;
    homeRedCard.teamName = home.name;
    homeRedCard.type = MatchEventType::RedCard;

    match_center::updateLiveMomentum(
        disciplinaryState,
        homeRedCard,
        home,
        away
    );

    assertTest(
        disciplinaryState.momentumScore < 0,
        "Tarjeta roja local favorece momentum visitante"
    );

    assertTest(
        match_center::momentumLabel(70) == "Dominio total del local" &&
        match_center::momentumLabel(35) == "El local controla el partido" &&
        match_center::momentumLabel(15) == "Ligera iniciativa local" &&
        match_center::momentumLabel(0) == "Partido equilibrado" &&
        match_center::momentumLabel(-15) == "Ligera iniciativa visitante" &&
        match_center::momentumLabel(-35) == "El visitante controla el partido" &&
        match_center::momentumLabel(-70) == "Dominio total del visitante",
        "Etiquetas de momentum cubren todos los rangos"
    );
}


void testLiveManagementEvents() {

    Team home;
    Team away;
    home.name = "Local";
    away.name = "Visita";

    match_center::LiveState state;

    MatchEvent homeSub;
    homeSub.teamName = home.name;
    homeSub.type = MatchEventType::Substitution;

    MatchEvent awayTactical;
    awayTactical.teamName = away.name;
    awayTactical.type = MatchEventType::TacticalChange;

    MatchEvent unknownSub;
    unknownSub.teamName = "Otro";
    unknownSub.type = MatchEventType::Substitution;

    match_center::applyLiveManagementEvent(
        state,
        homeSub,
        home,
        away
    );

    match_center::applyLiveManagementEvent(
        state,
        awayTactical,
        home,
        away
    );

    match_center::applyLiveManagementEvent(
        state,
        unknownSub,
        home,
        away
    );

    assertTest(
        state.homeSubstitutions == 1 &&
        state.awaySubstitutions == 0,
        "Gestion en vivo cuenta sustitucion del equipo correcto"
    );

    assertTest(
        state.homeTacticalChanges == 0 &&
        state.awayTacticalChanges == 1,
        "Gestion en vivo cuenta cambio tactico del equipo correcto"
    );
}

void testMatchCenterRendererOutput() {

    Team home;
    Team away;
    home.name = "Local";
    away.name = "Visita";

    MatchResult result;
    result.weather = "Despejado";

    match_center::LiveState state;
    state.minute = 37;
    state.homeGoals = 2;
    state.awayGoals = 1;
    state.homePossession = 58;
    state.awayPossession = 42;
    state.momentumScore = 35;
    state.homeShots = 8;
    state.awayShots = 5;
    state.homeShotsOnTarget = 4;
    state.awayShotsOnTarget = 2;
    state.homeCorners = 3;
    state.awayCorners = 1;
    state.homeExpectedGoals = 1.35;
    state.awayExpectedGoals = 0.72;
    state.homeHeatMap[4] = 3;
    state.awayHeatMap[8] = 2;
    state.lastEvent = "37' [Local] Remate peligroso";

    std::ostringstream output;
    std::streambuf* oldOutput = std::cout.rdbuf(output.rdbuf());

    match_center::drawMatchCenter(
        home,
        away,
        result,
        state,
        false
    );

    std::cout.rdbuf(oldOutput);

    const std::string text = output.str();

    assertTest(
        text.find("MATCH CENTER") != std::string::npos &&
        text.find("Local 2 - 1 Visita") != std::string::npos &&
        text.find("MINUTO 37") != std::string::npos,
        "Renderer muestra cabecera, marcador y minuto"
    );

    assertTest(
        text.find("Posesion en vivo") != std::string::npos &&
        text.find("El local controla el partido") != std::string::npos &&
        text.find("Mapa de calor por zonas") != std::string::npos,
        "Renderer muestra posesion, momentum y mapa de calor"
    );

    assertTest(
        text.find("Estadisticas") != std::string::npos &&
        text.find("xG") != std::string::npos &&
        text.find("Remate peligroso") != std::string::npos,
        "Renderer muestra estadisticas y ultimo evento"
    );
}

void testShowMatchCenterPlayback() {

    Team home;
    Team away;
    home.name = "Local";
    away.name = "Visita";

    MatchResult result;
    result.homeGoals = 1;
    result.awayGoals = 0;
    result.homePossession = 54;
    result.awayPossession = 46;
    result.stats.homeShots = 7;
    result.stats.awayShots = 4;
    result.stats.homeExpectedGoals = 1.10;
    result.stats.awayExpectedGoals = 0.55;
    result.weather = "Despejado";

    match_center::PlaybackOptions options;
    options.speed = match_center::PlaybackSpeed::Fast;
    options.clearScreenBetweenEvents = false;
    options.showAllEvents = false;

    std::ostringstream output;
    std::streambuf* oldOutput = std::cout.rdbuf(output.rdbuf());

    match_center::showMatchCenter(
        home,
        away,
        result,
        options
    );

    std::cout.rdbuf(oldOutput);

    const std::string text = output.str();

    assertTest(
        text.find("MATCH CENTER") != std::string::npos &&
        text.find("Local 1 - 0 Visita") != std::string::npos &&
        text.find("FINAL") != std::string::npos,
        "Reproduccion del Match Center llega al estado final"
    );

    assertTest(
        text.find("Clima: Despejado") != std::string::npos &&
        text.find("Posesion en vivo") != std::string::npos &&
        text.find("xG") != std::string::npos,
        "Reproduccion del Match Center muestra datos principales"
    );
}

void testInteractiveFinalSummaryOutput() {

    Team home;
    Team away;
    home.name = "Local";
    away.name = "Visita";

    MatchResult result;
    result.homeGoals = 3;
    result.awayGoals = 2;
    result.homeShots = 14;
    result.awayShots = 9;
    result.homePossession = 57;
    result.awayPossession = 43;
    result.homeSubstitutions = 4;
    result.awaySubstitutions = 3;
    result.stats.homeDangerousAttacks = 7;
    result.stats.awayDangerousAttacks = 5;

    MatchEvent earlyGoal;
    earlyGoal.minute = 20;
    earlyGoal.teamName = home.name;
    earlyGoal.type = MatchEventType::Goal;
    earlyGoal.description = "Gol temprano";

    MatchEvent lateYellow;
    lateYellow.minute = 81;
    lateYellow.teamName = away.name;
    lateYellow.type = MatchEventType::YellowCard;
    lateYellow.description = "Tarjeta final";

    MatchEvent lateGoal;
    lateGoal.minute = 88;
    lateGoal.teamName = home.name;
    lateGoal.type = MatchEventType::Goal;
    lateGoal.description = "Gol decisivo";

    result.timeline.events = {
        lateGoal,
        earlyGoal,
        lateYellow
    };

    std::ostringstream output;
    std::streambuf* oldOutput = std::cout.rdbuf(output.rdbuf());

    match_center::showInteractiveFinalSummary(
        home,
        away,
        result
    );

    std::cout.rdbuf(oldOutput);

    const std::string text = output.str();

    assertTest(
        text.find("MATCH CENTER - FINAL") != std::string::npos &&
        text.find("Local 3 - 2 Visita") != std::string::npos &&
        text.find("Tiros: 14 - 9") != std::string::npos,
        "Resumen final interactivo muestra marcador y estadisticas"
    );

    assertTest(
        text.find("81' Visita: Tarjeta final") != std::string::npos &&
        text.find("88' Local: Gol decisivo") != std::string::npos &&
        text.find("Gol temprano") == std::string::npos,
        "Resumen final filtra y ordena eventos importantes tardios"
    );
}

void testManagerDecisionInput() {

    Team controlledTeam;
    controlledTeam.name = "Local";

    match_engine::InteractiveMatchState state;
    state.minute = 30;
    state.currentTactics = "Balanced";
    state.currentInstruction = "Equilibrado";

    Player starter;
    starter.name = "Titular Prueba";
    starter.position = "MC";

    Player substitute;
    substitute.name = "Suplente Prueba";
    substitute.position = "MC";

    controlledTeam.players = {
        starter,
        substitute
    };

    state.activeXi = {0};
    state.availableBench = {1};

    {
        std::istringstream input("1\n");
        std::streambuf* oldInput = std::cin.rdbuf(input.rdbuf());

        const auto decision =
            match_center::askManagerDecision(
                controlledTeam,
                state
            );

        std::cin.rdbuf(oldInput);

        assertTest(
            decision.type == match_engine::ManagerDecisionType::Continue,
            "Decision interactiva permite continuar"
        );
    }

    {
        std::istringstream input("2\n4\n");
        std::streambuf* oldInput = std::cin.rdbuf(input.rdbuf());

        const auto decision =
            match_center::askManagerDecision(
                controlledTeam,
                state
            );

        std::cin.rdbuf(oldInput);

        assertTest(
            decision.type == match_engine::ManagerDecisionType::ChangeTactics &&
            decision.tactics == "Pressing",
            "Decision interactiva permite cambiar tactica"
        );
    }

    {
        std::istringstream input("3\n7\n");
        std::streambuf* oldInput = std::cin.rdbuf(input.rdbuf());

        const auto decision =
            match_center::askManagerDecision(
                controlledTeam,
                state
            );

        std::cin.rdbuf(oldInput);

        assertTest(
            decision.type == match_engine::ManagerDecisionType::ChangeInstruction &&
            decision.instruction == "Juego directo",
            "Decision interactiva permite cambiar instruccion"
        );
    }

    {
        std::istringstream input("4\n1\n1\n");
        std::streambuf* oldInput = std::cin.rdbuf(input.rdbuf());

        const auto decision =
            match_center::askManagerDecision(
                controlledTeam,
                state
            );

        std::cin.rdbuf(oldInput);

        assertTest(
            decision.type == match_engine::ManagerDecisionType::Substitute &&
            decision.playerOutIndex == 0 &&
            decision.playerInIndex == 1,
            "Decision interactiva permite hacer sustitucion"
        );
    }
}

void testFinalState() {

    MatchResult result;

    result.homeGoals = 2;
    result.awayGoals = 1;

    result.stats.homeShots = 12;
    result.stats.awayShots = 7;

    result.stats.homeDangerousAttacks = 6;
    result.stats.awayDangerousAttacks = 4;

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
        state.homeDangerousAttacks == 6 &&
        state.awayDangerousAttacks == 4,
        "Estado final conserva ataques peligrosos"
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
    testEventTextFormatting();
    testApplyGoalEvent();
    testStatisticsImpact();
    testLivePossessionByPhases();
    testHeatMapCounting();
    testLiveMomentumAndLabels();
    testLiveManagementEvents();
    testMatchCenterRendererOutput();
    testShowMatchCenterPlayback();
    testInteractiveFinalSummaryOutput();
    testManagerDecisionInput();
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
