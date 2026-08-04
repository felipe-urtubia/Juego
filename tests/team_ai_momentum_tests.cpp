#include "ai/team_ai.h"

#include "engine/models.h"
#include "engine/team_personality.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace {

void expectMomentumTest(
    bool condition,
    const std::string& message) {

    if (!condition) {
        throw std::runtime_error(message);
    }
}

Player makeMomentumTestPlayer(
    const std::string& name,
    const std::string& position,
    int skill = 70,
    int fitness = 78) {

    Player player{};
    player.name = name;
    player.position = position;
    player.preferredFoot = "Derecho";
    player.skill = skill;
    player.potential = skill + 6;
    player.age = 25;
    player.stamina = 76;
    player.fitness = fitness;
    player.attack = skill;
    player.defense = skill;
    player.currentForm = 68;
    player.tacticalDiscipline = 68;
    player.professionalism = 70;
    player.consistency = 70;
    player.bigMatches = 65;
    player.happiness = 70;
    player.chemistry = 70;
    player.contractWeeks = 100;

    applyPositionStats(player);
    ensurePlayerProfile(player, true);

    return player;
}

Team makeMomentumTestTeam(
    const std::string& name,
    int baseSkill = 70) {

    Team team(name);
    team.division = "primera division";
    team.formation = "4-3-3";
    team.tactics = "Balanced";
    team.matchInstruction = "Equilibrado";
    team.pressingIntensity = 2;
    team.defensiveLine = 3;
    team.tempo = 2;
    team.width = 3;
    team.markingStyle = "Zonal";
    team.morale = 65;
    team.lastTacticalChangeMinute = -100;

    static const std::vector<std::string> positions = {
        "ARQ",
        "DEF", "DEF", "DEF", "DEF",
        "MED", "MED", "MED",
        "DEL", "DEL", "DEL",
        "ARQ", "DEF", "MED", "DEL"
    };

    for (std::size_t i = 0; i < positions.size(); ++i) {
        team.addPlayer(
            makeMomentumTestPlayer(
                name + "_P" + std::to_string(i + 1),
                positions[i],
                baseSkill,
                78));
    }

    ensureTeamIdentity(team);
    return team;
}

void testStrongNegativeMomentumTriggersReaction() {
    Team team = makeMomentumTestTeam("Momentum Local");
    Team opponent = makeMomentumTestTeam("Momentum Rival");

    std::vector<std::string> events;

    const bool changed =
        team_ai::applyInMatchCpuAdjustment(
            team,
            opponent,
            60,
            0,
            0,
            &events,
            11,
            0,
            11,
            -70);

    expectMomentumTest(
        changed,
        "La IA debe reaccionar cuando su equipo sufre un momentum muy negativo.");

    expectMomentumTest(
        !events.empty(),
        "La reaccion por momentum debe generar una nota tactica.");

    expectMomentumTest(
        team.matchInstruction == "Contra-presion" ||
        team.matchInstruction == "Juego directo",
        "La reaccion debe buscar presion o una salida directa.");
}

void testModerateMomentumRespectsCooldown() {
    Team team = makeMomentumTestTeam("Cooldown Local");
    Team opponent = makeMomentumTestTeam("Cooldown Rival");

    team.lastTacticalChangeMinute = 55;

    std::vector<std::string> events;

    const bool changed =
        team_ai::applyInMatchCpuAdjustment(
            team,
            opponent,
            60,
            0,
            0,
            &events,
            11,
            0,
            11,
            -30);

    expectMomentumTest(
        !changed,
        "Un momentum moderado no debe ignorar el cooldown tactico.");

    expectMomentumTest(
        events.empty(),
        "Sin cambio tactico no debe registrarse un evento.");
}

void testExtremeMomentumBypassesCooldown() {
    Team team = makeMomentumTestTeam("Emergencia Local");
    Team opponent = makeMomentumTestTeam("Emergencia Rival");

    team.lastTacticalChangeMinute = 55;

    std::vector<std::string> events;

    const bool changed =
        team_ai::applyInMatchCpuAdjustment(
            team,
            opponent,
            60,
            0,
            0,
            &events,
            11,
            0,
            11,
            -75);

    expectMomentumTest(
        changed,
        "Un dominio rival extremo debe activar una reaccion de emergencia.");

    expectMomentumTest(
        !events.empty(),
        "La reaccion de emergencia debe quedar registrada.");
}

void testLegacyCallDefaultsToNeutralMomentum() {
    Team team = makeMomentumTestTeam("Legacy Local");
    Team opponent = makeMomentumTestTeam("Legacy Rival");

    std::vector<std::string> events;

    // Esta llamada no entrega momentumScore. Su compilacion verifica
    // que el valor predeterminado mantiene compatibilidad.
    const bool changed =
        team_ai::applyInMatchCpuAdjustment(
            team,
            opponent,
            30,
            0,
            0,
            &events,
            11,
            0,
            11);

    expectMomentumTest(
        !changed,
        "Con marcador equilibrado, estado fisico normal y momentum neutro no debe forzarse un cambio.");
}

}  // namespace

void runTeamAiMomentumTests() {
    testStrongNegativeMomentumTriggersReaction();
    testModerateMomentumRespectsCooldown();
    testExtremeMomentumBypassesCooldown();
    testLegacyCallDefaultsToNeutralMomentum();
}
