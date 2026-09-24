#include "career/match_center_service.h"

#include "simulation/match_center_state.h"
#include "simulation/player_rating_system.h"

#include <algorithm>
#include <cmath>
#include <sstream>

using namespace std;

namespace {

string formatXgTenth(int value) {
    ostringstream out;
    out.setf(ios::fixed);
    out.precision(1);
    out << (value / 10.0);
    return out.str();
}

string phaseLeadLabel(const MatchCenterSnapshot& snapshot) {
    if (snapshot.phaseSummaries.empty()) return "Sin lectura por fases.";
    return snapshot.phaseSummaries.front();
}

string resultLabel(const MatchCenterSnapshot& snapshot) {
    if (snapshot.opponentName.empty()) return "Sin marcador";
    if (snapshot.myGoals > snapshot.oppGoals) return "Victoria";
    if (snapshot.myGoals < snapshot.oppGoals) return "Derrota";
    return "Empate";
}

string matchControlLabel(const MatchCenterSnapshot& snapshot) {
    const int xgGap = snapshot.myExpectedGoalsTenths - snapshot.oppExpectedGoalsTenths;
    const int shotGap = snapshot.myShots - snapshot.oppShots;
    const int possessionGap = snapshot.myPossession - snapshot.oppPossession;
    if (xgGap >= 5 && shotGap >= 3) return "El equipo genero las mejores ocasiones.";
    if (xgGap <= -5 || shotGap <= -4) return "El rival encontro mas peligro y obligo a ajustar.";
    if (possessionGap >= 10 && xgGap >= 0) return "Hubo control territorial con produccion suficiente.";
    if (possessionGap <= -10 && xgGap <= 0) return "Falto control de mediocampo y salida limpia.";
    return "Partido equilibrado, decidido por detalles.";
}

vector<string> buildPlayerStatLines(const MatchTimeline& timeline) {
    player_rating_system::LiveRatings ratings;
    for (const MatchEvent& event : timeline.events) {
        ratings.applyEvent(event);
    }

    const auto players = ratings.topPlayers(100);
    vector<string> lines;
    lines.reserve(players.size());

    for (const auto& player : players) {
        ostringstream line;
        line.setf(ios::fixed);
        line.precision(1);
        line << player.playerName
             << " | " << player.teamName
             << " | Nota " << player.rating
             << " | Tiros " << player.shots
             << " | Al arco " << player.shotsOnTarget
             << " | Goles " << player.goals
             << " | Ocasiones claras " << player.bigChances;
        line.precision(2);
        line << " | xG " << player.expectedGoals
             << " | Atajadas " << player.saves
             << " | TA " << player.yellowCards
             << " | TR " << player.redCards;
        lines.push_back(line.str());
    }

    return lines;
}

vector<string> buildTimelineLines(const MatchTimeline& timeline) {
    vector<const MatchEvent*> events;

    for (const MatchEvent& event : timeline.events) {
        const bool relevant =
            event.type == MatchEventType::Shot ||
            event.type == MatchEventType::BigChance ||
            event.type == MatchEventType::Goal ||
            event.type == MatchEventType::Miss ||
            event.type == MatchEventType::Save ||
            event.type == MatchEventType::YellowCard ||
            event.type == MatchEventType::RedCard ||
            event.type == MatchEventType::Injury ||
            event.type == MatchEventType::Corner ||
            event.type == MatchEventType::Counterattack ||
            event.type == MatchEventType::TacticalChange ||
            event.type == MatchEventType::Substitution;

        if (relevant) {
            events.push_back(&event);
        }
    }

    stable_sort(
        events.begin(),
        events.end(),
        [](const MatchEvent* left, const MatchEvent* right) {
            return left->minute < right->minute;
        });

    vector<string> lines;
    lines.reserve(events.size());

    for (const MatchEvent* event : events) {
        lines.push_back(
            to_string(event->minute) + "' " +
            event->teamName + ": " +
            event->description);
    }

    return lines;
}
vector<string> buildRecommendationLines(const MatchCenterSnapshot& snapshot) {
    vector<string> lines;
    const int shotGap = snapshot.myShots - snapshot.oppShots;
    const int shotOnTargetGap = snapshot.myShotsOnTarget - snapshot.oppShotsOnTarget;
    const int xgGap = snapshot.myExpectedGoalsTenths - snapshot.oppExpectedGoalsTenths;
    const int possessionGap = snapshot.myPossession - snapshot.oppPossession;

    if (snapshot.myExpectedGoalsTenths <= 9 && snapshot.myShots <= 8) {
        lines.push_back("Subir volumen ofensivo: faltaron ataques claros y remate final.");
    } else if (snapshot.myExpectedGoalsTenths >= 14 && snapshot.myGoals < snapshot.oppGoals) {
        lines.push_back("Mantener plan de ataque: el problema fue mas de definicion que de produccion.");
    }
    if (snapshot.oppExpectedGoalsTenths >= 14 || shotOnTargetGap <= -3) {
        lines.push_back("Proteger mejor el area: el rival encontro demasiados tiros limpios.");
    }
    if (possessionGap <= -12) {
        lines.push_back("Recuperar control del medio: el partido se jugo demasiado tiempo en campo propio.");
    } else if (possessionGap >= 12 && xgGap < 0) {
        lines.push_back("La posesion no alcanzo: conviene acelerar el ultimo tercio o atacar mas directo.");
    }
    if (shotGap >= 5 && snapshot.myGoals == 0) {
        lines.push_back("Trabajar la finalizacion y el balon parado: llegaste, pero no convertiste.");
    }
    if (!snapshot.fatigueSummary.empty()) {
        lines.push_back("Revisar carga postpartido: la lectura fisica sugiere gestionar mejor la semana.");
    }
    if (snapshot.myGoals < snapshot.oppGoals && snapshot.oppExpectedGoalsTenths >= 14) {
        lines.push_back("Decision semanal sugerida: Preparar rival o Defensa antes de volver a simular.");
    } else if (snapshot.myGoals < snapshot.oppGoals && snapshot.myExpectedGoalsTenths <= 10) {
        lines.push_back("Decision semanal sugerida: Entrenar fuerte con foco Ataque.");
    } else if (snapshot.myGoals >= snapshot.oppGoals && snapshot.myExpectedGoalsTenths >= snapshot.oppExpectedGoalsTenths + 4) {
        lines.push_back("Decision semanal sugerida: sostener plan y rotar cargas para proteger la ventaja.");
    }
    if (lines.empty()) {
        lines.push_back("Partido equilibrado: el siguiente ajuste fino deberia salir de tu lectura del rival.");
    }
    if (lines.size() > 4) lines.resize(4);
    return lines;
}

}  // namespace

namespace match_center_service {

void captureLastMatchCenter(Career& career,
                            const Team& home,
                            const Team& away,
                            const MatchResult& result,
                            bool cupMatch) {
    if (!career.myTeam) return;
    const bool myHome = (&home == career.myTeam);
    const bool myAway = (&away == career.myTeam);
    if (!myHome && !myAway) return;

    MatchCenterSnapshot snapshot;
    snapshot.competitionLabel = cupMatch ? "Copa" : "Liga";
    snapshot.opponentName = myHome ? away.name : home.name;
    snapshot.venueLabel = myHome ? "Local" : "Visita";
    snapshot.myGoals = myHome ? result.homeGoals : result.awayGoals;
    snapshot.oppGoals = myHome ? result.awayGoals : result.homeGoals;
    snapshot.myShots = myHome ? result.homeShots : result.awayShots;
    snapshot.oppShots = myHome ? result.awayShots : result.homeShots;
    snapshot.myShotsOnTarget = myHome ? result.stats.homeShotsOnTarget : result.stats.awayShotsOnTarget;
    snapshot.oppShotsOnTarget = myHome ? result.stats.awayShotsOnTarget : result.stats.homeShotsOnTarget;
    snapshot.myPossession = myHome ? result.homePossession : result.awayPossession;
    snapshot.oppPossession = myHome ? result.awayPossession : result.homePossession;
    snapshot.myCorners = myHome ? result.homeCorners : result.awayCorners;
    snapshot.oppCorners = myHome ? result.awayCorners : result.homeCorners;
    snapshot.mySubstitutions = myHome ? result.homeSubstitutions : result.awaySubstitutions;
    snapshot.oppSubstitutions = myHome ? result.awaySubstitutions : result.homeSubstitutions;
    snapshot.myExpectedGoalsTenths =
        static_cast<int>(std::round((myHome ? result.stats.homeExpectedGoals : result.stats.awayExpectedGoals) * 10.0));
    snapshot.oppExpectedGoalsTenths =
        static_cast<int>(std::round((myHome ? result.stats.awayExpectedGoals : result.stats.homeExpectedGoals) * 10.0));
    snapshot.weather = result.weather;
    snapshot.dominanceSummary = result.report.explanation.likelyReason;
    snapshot.tacticalSummary = result.report.explanation.tacticalStory;
    snapshot.fatigueSummary = result.report.explanation.fatigueStory;
    snapshot.postMatchImpact = result.report.postMatchImpact;
    snapshot.playerRatingLines = result.report.playerRatingLines;
    snapshot.phaseSummaries = result.report.phaseSummaries;

    match_center::LiveState heatState;
    for (const MatchEvent& event : result.timeline.events) {
        match_center::updateHeatMap(heatState, event, home, away);
    }
    snapshot.myHeatMap = myHome ? heatState.homeHeatMap : heatState.awayHeatMap;
    snapshot.oppHeatMap = myHome ? heatState.awayHeatMap : heatState.homeHeatMap;
    snapshot.playerStatLines = buildPlayerStatLines(result.timeline);
    snapshot.timelineLines = buildTimelineLines(result.timeline);

    career.lastMatchCenter = snapshot;
}

MatchCenterView buildLastMatchCenter(const Career& career,
                                     size_t maxPhases,
                                     size_t maxEvents) {
    MatchCenterView view;
    const MatchCenterSnapshot& snapshot = career.lastMatchCenter;
    if (career.lastMatchAnalysis.empty() && snapshot.opponentName.empty() &&
        career.lastMatchEvents.empty() && career.lastMatchReportLines.empty()) {
        return view;
    }

    view.available = true;
    view.headline = career.lastMatchAnalysis;
    if (!snapshot.opponentName.empty()) {
        view.scoreboard = snapshot.competitionLabel + " | " + snapshot.venueLabel + " vs " + snapshot.opponentName +
                          " | " + to_string(snapshot.myGoals) + "-" + to_string(snapshot.oppGoals) +
                          " | clima " + snapshot.weather;
    }
    view.tacticalSummary = snapshot.tacticalSummary;
    view.fatigueSummary = snapshot.fatigueSummary;
    view.postMatchImpact = snapshot.postMatchImpact;
    view.playerOfTheMatch = career.lastMatchPlayerOfTheMatch;
    view.myHeatMap = snapshot.myHeatMap;
    view.oppHeatMap = snapshot.oppHeatMap;
    view.playerStatLines = snapshot.playerStatLines;
    view.timelineLines = snapshot.timelineLines;
    if (!snapshot.opponentName.empty()) {
        view.metrics.push_back({"Tiros", to_string(snapshot.myShots), to_string(snapshot.oppShots)});
        view.metrics.push_back({"Arco", to_string(snapshot.myShotsOnTarget), to_string(snapshot.oppShotsOnTarget)});
        view.metrics.push_back({"Posesion", to_string(snapshot.myPossession) + "%", to_string(snapshot.oppPossession) + "%"});
        view.metrics.push_back({"Corners", to_string(snapshot.myCorners), to_string(snapshot.oppCorners)});
        view.metrics.push_back({"xG", formatXgTenth(snapshot.myExpectedGoalsTenths), formatXgTenth(snapshot.oppExpectedGoalsTenths)});
        view.metrics.push_back({"Cambios", to_string(snapshot.mySubstitutions), to_string(snapshot.oppSubstitutions)});
    }

    const size_t phaseCount = min(maxPhases, snapshot.phaseSummaries.size());
    for (size_t i = 0; i < phaseCount; ++i) {
        view.phaseLines.push_back(snapshot.phaseSummaries[i]);
    }

    const size_t eventCount = min(maxEvents, career.lastMatchEvents.size());
    for (size_t i = 0; i < eventCount; ++i) {
        view.eventLines.push_back(career.lastMatchEvents[i]);
    }
    view.recommendationLines = buildRecommendationLines(snapshot);
    return view;
}

string formatLastMatchCenter(const Career& career,
                             size_t maxPhases,
                             size_t maxEvents) {
    MatchCenterView view = buildLastMatchCenter(career, maxPhases, maxEvents);
    if (!view.available) return "No hay match center disponible.";

    ostringstream out;
    const MatchCenterSnapshot& snapshot = career.lastMatchCenter;
    out << "Match Center\r\n";
    out << "Resultado: " << resultLabel(snapshot);
    if (!view.scoreboard.empty()) out << " | " << view.scoreboard;
    out << "\r\n";
    if (!view.headline.empty()) out << "Lectura rapida: " << view.headline << "\r\n";
    if (!snapshot.opponentName.empty()) out << "Control: " << matchControlLabel(snapshot) << "\r\n";
    if (!view.playerOfTheMatch.empty()) out << "Jugador clave: " << view.playerOfTheMatch << "\r\n";
    if (!snapshot.playerRatingLines.empty()) {
        out << "\r\nValoraciones:\r\n";
        for (const string& ratingLine : snapshot.playerRatingLines) {
            out << "- " << ratingLine << "\r\n";
        }
    }
    if (!view.playerStatLines.empty()) {
        out << "\r\nEstadisticas avanzadas:\r\n";
        for (const string& line : view.playerStatLines) {
            out << "- " << line << "\r\n";
        }
    }
    if (!view.metrics.empty()) {
        out << "\r\nIndicadores (tu equipo / rival)\r\n";
        for (const MatchCenterMetric& metric : view.metrics) {
            out << "- " << metric.label << ": " << metric.myValue << " / " << metric.oppValue << "\r\n";
        }
    }
    const bool hasHeatMap =
        any_of(view.myHeatMap.begin(), view.myHeatMap.end(), [](int value) { return value > 0; }) ||
        any_of(view.oppHeatMap.begin(), view.oppHeatMap.end(), [](int value) { return value > 0; });
    if (hasHeatMap) {
        out << "\r\nMapa de calor por zonas\r\n";
        out << "Tu equipo\r\n";
        out << "- Propio: " << view.myHeatMap[0] << " / " << view.myHeatMap[1] << " / " << view.myHeatMap[2] << "\r\n";
        out << "- Mediocampo: " << view.myHeatMap[3] << " / " << view.myHeatMap[4] << " / " << view.myHeatMap[5] << "\r\n";
        out << "- Ultimo tercio: " << view.myHeatMap[6] << " / " << view.myHeatMap[7] << " / " << view.myHeatMap[8] << "\r\n";
        out << "Rival\r\n";
        out << "- Propio: " << view.oppHeatMap[0] << " / " << view.oppHeatMap[1] << " / " << view.oppHeatMap[2] << "\r\n";
        out << "- Mediocampo: " << view.oppHeatMap[3] << " / " << view.oppHeatMap[4] << " / " << view.oppHeatMap[5] << "\r\n";
        out << "- Ultimo tercio: " << view.oppHeatMap[6] << " / " << view.oppHeatMap[7] << " / " << view.oppHeatMap[8] << "\r\n";
    }
    if (!view.tacticalSummary.empty() || !view.fatigueSummary.empty() || !view.postMatchImpact.empty()) {
        out << "\r\nDiagnostico\r\n";
        if (!view.tacticalSummary.empty()) out << "- Tactica: " << view.tacticalSummary << "\r\n";
        if (!view.fatigueSummary.empty()) out << "- Fisico: " << view.fatigueSummary << "\r\n";
        if (!view.postMatchImpact.empty()) out << "- Impacto: " << view.postMatchImpact << "\r\n";
    }
    if (!view.phaseLines.empty()) {
        out << "\r\nMomentos del partido\r\n";
        for (const string& line : view.phaseLines) out << "- " << line << "\r\n";
    } else if (!career.lastMatchCenter.phaseSummaries.empty()) {
        out << "\r\nMomentos del partido\r\n- " << phaseLeadLabel(career.lastMatchCenter) << "\r\n";
    }
    if (!view.eventLines.empty()) {
        out << "\r\nEventos\r\n";
        for (const string& event : view.eventLines) out << "- " << event << "\r\n";
    }
    if (!view.timelineLines.empty()) {
        out << "\r\nLinea temporal del partido\r\n";
        for (const string& event : view.timelineLines) {
            out << "- " << event << "\r\n";
        }
    }
    if (!view.recommendationLines.empty()) {
        out << "\r\nPlan inmediato\r\n";
        for (const string& line : view.recommendationLines) out << "- " << line << "\r\n";
    }
    return out.str();
}

}  // namespace match_center_service
