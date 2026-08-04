#include "simulation/match_commentary.h"

#include <sstream>
#include <string>

namespace match_commentary {
namespace {

bool belongsToHome(
    const MatchEvent& event,
    const Team& home,
    const Team& away) {

    if (event.teamName == home.name) {
        return true;
    }

    if (event.teamName == away.name) {
        return false;
    }

    const int homeImpact =
        event.impact.homeGoalsDelta +
        event.impact.homeShotsDelta +
        event.impact.homeShotsOnTargetDelta +
        event.impact.homeCornersDelta;

    const int awayImpact =
        event.impact.awayGoalsDelta +
        event.impact.awayShotsDelta +
        event.impact.awayShotsOnTargetDelta +
        event.impact.awayCornersDelta;

    return homeImpact >= awayImpact;
}

std::string subjectName(
    const MatchEvent& event,
    const Team& team) {

    if (!event.playerName.empty()) {
        return event.playerName;
    }

    return team.name;
}

std::string originalDescription(
    const MatchEvent& event) {

    if (event.description.empty()) {
        return {};
    }

    return " " + event.description;
}

std::string momentumContext(
    const match_center::LiveState& state) {

    if (state.momentumScore >= 55) {
        return " El local atraviesa su mejor momento.";
    }

    if (state.momentumScore >= 25) {
        return " El local mantiene la iniciativa.";
    }

    if (state.momentumScore <= -55) {
        return " El visitante domina claramente este tramo.";
    }

    if (state.momentumScore <= -25) {
        return " El visitante está creciendo en el partido.";
    }

    return {};
}

}  // namespace

std::string buildCommentary(
    const MatchEvent& event,
    const match_center::LiveState& state,
    const Team& home,
    const Team& away) {

    const bool homeEvent =
        belongsToHome(event, home, away);

    const Team& attackingTeam =
        homeEvent ? home : away;

    const Team& defendingTeam =
        homeEvent ? away : home;

    const std::string subject =
        subjectName(event, attackingTeam);

    std::ostringstream text;
    text << event.minute << "' ";

    switch (event.type) {
        case MatchEventType::PossessionPhase:
            text << attackingTeam.name
                 << " intenta adueñarse del balón.";
            break;

        case MatchEventType::Progression:
            text << attackingTeam.name
                 << " supera la primera línea y progresa hacia campo rival.";
            break;

        case MatchEventType::AttackBuildUp:
            text << attackingTeam.name
                 << " construye el ataque con paciencia.";
            break;

        case MatchEventType::Shot:
            text << subject
                 << " prueba el remate contra "
                 << defendingTeam.name << '.';
            break;

        case MatchEventType::BigChance:
            text << "¡Gran ocasión para "
                 << attackingTeam.name << "!";
            break;

        case MatchEventType::Goal:
            text << "¡GOOOOOOL DE "
                 << attackingTeam.name << "! ";

            if (!event.playerName.empty()) {
                text << event.playerName
                     << " convierte y mueve el marcador.";
            } else {
                text << "La jugada termina dentro del arco.";
            }
            break;

        case MatchEventType::Miss:
            text << subject
                 << " no logra encontrar el arco.";
            break;

        case MatchEventType::Save:
            text << "Gran respuesta del arquero de "
                 << defendingTeam.name
                 << " para evitar el gol.";
            break;

        case MatchEventType::Foul:
            text << "Falta cometida por "
                 << attackingTeam.name
                 << ". El juego queda detenido.";
            break;

        case MatchEventType::YellowCard:
            text << "Tarjeta amarilla para "
                 << subject << '.';
            break;

        case MatchEventType::RedCard:
            text << "¡Expulsión! "
                 << subject
                 << " deja a su equipo con un jugador menos.";
            break;

        case MatchEventType::Injury:
            text << subject
                 << " queda tendido y necesita atención médica.";
            break;

        case MatchEventType::Corner:
            text << "Córner para "
                 << attackingTeam.name
                 << ". Puede llegar el peligro por arriba.";
            break;

        case MatchEventType::Offside:
            text << subject
                 << " queda en posición adelantada.";
            break;

        case MatchEventType::Counterattack:
            text << "¡Contraataque peligroso de "
                 << attackingTeam.name << "!";
            break;

        case MatchEventType::TacticalChange:
            text << attackingTeam.name
                 << " modifica su planteamiento táctico.";
            break;

        case MatchEventType::Substitution:
            text << attackingTeam.name
                 << " mueve el banco y realiza una sustitución.";
            break;
    }

    text << originalDescription(event);
    text << momentumContext(state);

    return text.str();
}

}  // namespace match_commentary
