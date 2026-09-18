#include "simulation/match_report.h"
#include "simulation/player_rating_system.h"

#include "simulation/tactics_engine.h"

#include <algorithm>
#include <cmath>
#include <sstream>

using namespace std;

namespace {

string formatDouble2(double value) {
    ostringstream out;
    out.setf(ios::fixed);
    out.precision(2);
    out << value;
    return out.str();
}

string summarizeStyle(const Team& team, const TacticalProfile& profile) {
    string summary = team.tactics + " / " + team.matchInstruction;
    if (profile.pressing >= 0.72) summary += ", presion alta";
    else if (profile.defensiveBlock >= 0.62) summary += ", bloque protegido";

    if (profile.directness >= 0.55) summary += ", pase directo";
    else if (profile.width >= 0.62) summary += ", amplitud sostenida";
    else summary += ", ataque interior";
    return summary;
}

string buildLikelyReason(const MatchSetup& setup, const MatchStats& stats, const Team& home, const Team& away) {
    if (stats.homeExpectedGoals > stats.awayExpectedGoals + 0.45) {
        return home.name + " produjo ocasiones mas limpias y sostuvo mejor sus ataques maduros.";
    }
    if (stats.awayExpectedGoals > stats.homeExpectedGoals + 0.45) {
        return away.name + " encontro mejores ventanas de remate y castigo los espacios locales.";
    }
    if (setup.context.midfieldControlHome > setup.context.midfieldControlAway + 4.0) {
        return home.name + " marco el ritmo desde el mediocampo y redujo la continuidad rival.";
    }
    if (setup.context.midfieldControlAway > setup.context.midfieldControlHome + 4.0) {
        return away.name + " goberno la zona media y obligo al local a correr detras del partido.";
    }
    if (stats.homeRedCards != stats.awayRedCards) {
        return "La inferioridad numerica condiciono el desarrollo del encuentro.";
    }
    if (stats.homeShots + 3 < stats.awayShots && stats.homeGoals >= stats.awayGoals) {
        return home.name + " resistio mas de lo esperado y aprovecho mejor sus remates utiles.";
    }
    if (stats.awayShots + 3 < stats.homeShots && stats.awayGoals >= stats.homeGoals) {
        return away.name + " fue mas eficiente en las pocas llegadas realmente claras.";
    }
    return "El resultado se definio por detalles en un partido de margenes cortos.";
}

}  // namespace

namespace match_report {

MatchReport buildReport(const MatchSetup& setup,
                        const Team& home,
                        const Team& away,
                        const MatchTimeline& timeline,
                        const MatchStats& stats) {
    MatchReport report;
    player_rating_system::LiveRatings liveRatings;
    int homeDominantPhases = 0;
    int awayDominantPhases = 0;
    int homeFatigueLoad = 0;
    int awayFatigueLoad = 0;
    int homeLateFatigue = 0;
    int awayLateFatigue = 0;
    int homeChasingPhases = 0;
    int awayChasingPhases = 0;
    int homeShortHandedPhases = 0;
    int awayShortHandedPhases = 0;
    int tacticalChanges = 0;
    int homeTotalAttacks = 0;
    int awayTotalAttacks = 0;
    double homeRiskLoad = 0.0;
    double awayRiskLoad = 0.0;

    for (size_t i = 0; i < timeline.phases.size(); ++i) {
        const MatchPhaseReport& phase = timeline.phases[i];
        if (phase.dominantTeam == home.name) homeDominantPhases++;
        else if (phase.dominantTeam == away.name) awayDominantPhases++;
        if (phase.homeTacticalChange || phase.awayTacticalChange) tacticalChanges++;
        if (phase.homeUrgency > phase.awayUrgency + 0.06) homeChasingPhases++;
        if (phase.awayUrgency > phase.homeUrgency + 0.06) awayChasingPhases++;
        if (phase.homePlayersAvailable < phase.awayPlayersAvailable) homeShortHandedPhases++;
        if (phase.awayPlayersAvailable < phase.homePlayersAvailable) awayShortHandedPhases++;
        homeFatigueLoad += phase.homeFatigueGain;
        awayFatigueLoad += phase.awayFatigueGain;
        if (i >= 4) {
            homeLateFatigue += phase.homeFatigueGain;
            awayLateFatigue += phase.awayFatigueGain;
        }
        homeTotalAttacks += phase.homeAttacks;
        awayTotalAttacks += phase.awayAttacks;
        homeRiskLoad += phase.homeDefensiveRisk;
        awayRiskLoad += phase.awayDefensiveRisk;

        ostringstream line;
        line << phase.minuteStart << "-" << phase.minuteEnd << ": domina " << phase.dominantTeam
             << ", posesion " << phase.homePossessionShare << "-" << phase.awayPossessionShare
             << ", progresiones " << phase.homeProgressions << "-" << phase.awayProgressions
             << ", ataques " << phase.homeAttacks << "-" << phase.awayAttacks
             << ", remates " << phase.homeShotsGenerated << "-" << phase.awayShotsGenerated
             << ", riesgo " << formatDouble2(phase.homeDefensiveRisk) << "-" << formatDouble2(phase.awayDefensiveRisk);
        if (phase.homePlayersAvailable != phase.awayPlayersAvailable) {
            line << ", hombres " << phase.homePlayersAvailable << "-" << phase.awayPlayersAvailable;
        }
        if (phase.homeUrgency >= 0.10 || phase.awayUrgency >= 0.10) {
            line << ", empuje " << formatDouble2(phase.homeUrgency) << "-" << formatDouble2(phase.awayUrgency);
        }
        if (phase.homeTacticalChange || phase.awayTacticalChange) {
            line << ", ajustes tacticos";
        }
        report.phaseSummaries.push_back(line.str());
    }

    for (const MatchEvent& event : timeline.events) {
        liveRatings.applyEvent(event);
    }

    const auto finalRatings =
        liveRatings.topPlayers(home.players.size() + away.players.size());

    for (const auto& playerRating : finalRatings) {
        ostringstream ratingLine;
        ratingLine.setf(ios::fixed);
        ratingLine.precision(1);
        ratingLine << playerRating.playerName
                   << " | " << playerRating.teamName
                   << " | " << playerRating.rating;
        report.playerRatingLines.push_back(ratingLine.str());
    }

    if (!finalRatings.empty()) {
        report.playerOfTheMatch = finalRatings.front().playerName;
        report.playerOfTheMatchScore =
            static_cast<int>(lround(finalRatings.front().rating * 10.0));
    }

    report.tacticalImpact.homeControlScore =
        setup.context.midfieldControlHome + setup.context.tacticalAdvantageHome * 18.0;
    report.tacticalImpact.awayControlScore =
        setup.context.midfieldControlAway + setup.context.tacticalAdvantageAway * 18.0;
    report.tacticalImpact.homePressingLoad =
        (setup.home.tacticalProfile.pressing + setup.home.tacticalProfile.tempo) * 10.0;
    report.tacticalImpact.awayPressingLoad =
        (setup.away.tacticalProfile.pressing + setup.away.tacticalProfile.tempo) * 10.0;
    report.tacticalImpact.homeTransitionThreat = tactics_engine::transitionThreatWeight(setup.home.tacticalProfile);
    report.tacticalImpact.awayTransitionThreat = tactics_engine::transitionThreatWeight(setup.away.tacticalProfile);
    report.tacticalImpact.homeSummary = home.name + ": " + summarizeStyle(home, setup.home.tacticalProfile);
    report.tacticalImpact.awaySummary = away.name + ": " + summarizeStyle(away, setup.away.tacticalProfile);

    report.fatigueImpact.homeFatigueLoad = homeFatigueLoad;
    report.fatigueImpact.awayFatigueLoad = awayFatigueLoad;
    report.fatigueImpact.homeLateDrop = homeLateFatigue >= awayLateFatigue + 4;
    report.fatigueImpact.awayLateDrop = awayLateFatigue >= homeLateFatigue + 4;
    if (report.fatigueImpact.homeLateDrop) {
        report.fatigueImpact.summary = home.name + " llego mas castigado al ultimo tramo.";
    } else if (report.fatigueImpact.awayLateDrop) {
        report.fatigueImpact.summary = away.name + " sufrio mas desgaste en el cierre.";
    } else {
        report.fatigueImpact.summary = "La carga fisica fue pareja y no desordeno el plan de ninguno.";
    }

    report.explanation.likelyReason = buildLikelyReason(setup, stats, home, away);
    const string dominanceStory =
        (homeDominantPhases > awayDominantPhases)
            ? home.name + " impuso mas fases de control (" + to_string(homeDominantPhases) + " de " +
                  to_string(static_cast<int>(timeline.phases.size())) + ")."
            : (awayDominantPhases > homeDominantPhases)
                  ? away.name + " sostuvo mas tramos de dominio territorial."
                  : "Ningun equipo monopolizo el partido; hubo alternancia de control.";
    string riskStory;
    const double avgHomeRisk = timeline.phases.empty() ? 0.0 : homeRiskLoad / timeline.phases.size();
    const double avgAwayRisk = timeline.phases.empty() ? 0.0 : awayRiskLoad / timeline.phases.size();
    if (avgHomeRisk > avgAwayRisk + 0.08) {
        riskStory = home.name + " asumio mas riesgo defensivo y dejo mas ventanas de transicion.";
    } else if (avgAwayRisk > avgHomeRisk + 0.08) {
        riskStory = away.name + " se expuso mas cuando debio correr hacia atras.";
    } else {
        riskStory = "Los riesgos defensivos estuvieron bastante equilibrados.";
    }
    if (homeShortHandedPhases > awayShortHandedPhases) {
        riskStory += " " + home.name + " paso mas tiempo con inferioridad numerica o reajustes por disponibilidad.";
    } else if (awayShortHandedPhases > homeShortHandedPhases) {
        riskStory += " " + away.name + " jugo varios tramos con menos disponibilidad real.";
    }
    if (homeChasingPhases > awayChasingPhases) {
        riskStory += " " + home.name + " tuvo que empujar el marcador durante buena parte del cierre.";
    } else if (awayChasingPhases > homeChasingPhases) {
        riskStory += " " + away.name + " acelero mas al verse por detras o bajo presion.";
    }
    string fatigueOverlay;
    if (report.fatigueImpact.homeLateDrop && report.tacticalImpact.homePressingLoad > report.tacticalImpact.awayPressingLoad + 1.0) {
        fatigueOverlay = "La presion del local perdio altura en el cierre.";
    } else if (report.fatigueImpact.awayLateDrop &&
               report.tacticalImpact.awayPressingLoad > report.tacticalImpact.homePressingLoad + 1.0) {
        fatigueOverlay = "La visita no pudo sostener su agresividad hasta el final.";
    }
    report.explanation.tacticalStory = dominanceStory + " " + riskStory +
                                       (fatigueOverlay.empty() ? "" : " " + fatigueOverlay);
    report.explanation.fatigueStory = report.fatigueImpact.summary;
    report.explanation.disciplineStory =
        (stats.homeRedCards + stats.awayRedCards > 0)
            ? "La disciplina altero el partido con " + to_string(stats.homeRedCards + stats.awayRedCards) +
                  " expulsiones."
            : "Las tarjetas incidieron poco en la estructura del partido.";
    report.explanation.chanceStory =
        "xG " + formatDouble2(stats.homeExpectedGoals) + "-" + formatDouble2(stats.awayExpectedGoals) +
        " | ocasiones claras " + to_string(stats.homeBigChances) + "-" + to_string(stats.awayBigChances) +
        " | ataques " + to_string(homeTotalAttacks) + "-" + to_string(awayTotalAttacks) +
        " | tiros " + to_string(stats.homeShots) + "-" + to_string(stats.awayShots) +
        " | cambios tacticos " + to_string(tacticalChanges);
    report.postMatchImpact =
        "Moral " + to_string(stats.homeGoals > stats.awayGoals ? +3 : stats.homeGoals < stats.awayGoals ? -2 : +1) +
        "/" +
        to_string(stats.awayGoals > stats.homeGoals ? +3 : stats.awayGoals < stats.homeGoals ? -2 : +1) +
        " | disciplina " + to_string(stats.homeYellowCards + stats.homeRedCards) +
        "-" + to_string(stats.awayYellowCards + stats.awayRedCards);
    return report;
}

void appendSummaryLines(const MatchReport& report, vector<string>& lines) {
    lines.push_back("Impacto tactico: " + report.tacticalImpact.homeSummary + " || " + report.tacticalImpact.awaySummary);
    lines.push_back("Riesgo tactico: control " + formatDouble2(report.tacticalImpact.homeControlScore) + "-" +
                    formatDouble2(report.tacticalImpact.awayControlScore) +
                    " | transicion " + formatDouble2(report.tacticalImpact.homeTransitionThreat) + "-" +
                    formatDouble2(report.tacticalImpact.awayTransitionThreat));
    if (!report.playerOfTheMatch.empty()) {
        ostringstream figureLine;
        figureLine.setf(ios::fixed);
        figureLine.precision(1);
        figureLine << "Figura: " << report.playerOfTheMatch
                   << " (" << report.playerOfTheMatchScore / 10.0 << ")";
        lines.push_back(figureLine.str());
    }
    if (!report.playerRatingLines.empty()) {
        lines.push_back("Valoraciones:");
        for (const string& ratingLine : report.playerRatingLines) {
            lines.push_back("- " + ratingLine);
        }
    }
    lines.push_back("Claves: " + report.explanation.likelyReason);
    lines.push_back("Tactica: " + report.explanation.tacticalStory);
    lines.push_back("Fatiga: " + report.explanation.fatigueStory);
    lines.push_back("Disciplina: " + report.explanation.disciplineStory);
    lines.push_back("Detalle: " + report.explanation.chanceStory);
    lines.push_back("Impacto posterior: " + report.postMatchImpact);
    for (const string& phase : report.phaseSummaries) {
        lines.push_back("Fase " + phase);
    }
}

}  // namespace match_report
