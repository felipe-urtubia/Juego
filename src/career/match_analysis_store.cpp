#include "career/match_analysis_store.h"

#include "career/match_center_service.h"
#include "career/career_support.h"

#include <algorithm>
#include <sstream>

using namespace std;

namespace {

string buildCompactXgText(double xg) {
    ostringstream out;
    out.setf(ios::fixed);
    out.precision(1);
    out << xg;
    return out.str();
}

void appendSection(ostringstream& out,
                   const string& title,
                   const vector<string>& lines,
                   size_t limit) {
    if (lines.empty() || limit == 0) return;
    out << title << "\r\n";
    const size_t count = min(limit, lines.size());
    for (size_t i = 0; i < count; ++i) {
        out << "- " << lines[i] << "\r\n";
    }
}

}  // namespace

namespace career_match_analysis {

void storeMatchAnalysis(Career& career,
                        const Team& home,
                        const Team& away,
                        const MatchResult& result,
                        bool cupMatch) {
    if (!career.myTeam) return;
    const bool myHome = (&home == career.myTeam);
    const bool myAway = (&away == career.myTeam);
    if (!myHome && !myAway) return;

    const int myGoals = myHome ? result.homeGoals : result.awayGoals;
    const int oppGoals = myHome ? result.awayGoals : result.homeGoals;
    const int myShots = myHome ? result.homeShots : result.awayShots;
    const int oppShots = myHome ? result.awayShots : result.homeShots;
    const int myPoss = myHome ? result.homePossession : result.awayPossession;
    const int oppPoss = myHome ? result.awayPossession : result.homePossession;
    const int mySubs = myHome ? result.homeSubstitutions : result.awaySubstitutions;
    const int oppSubs = myHome ? result.awaySubstitutions : result.homeSubstitutions;
    const int myCorners = myHome ? result.homeCorners : result.awayCorners;
    const int oppCorners = myHome ? result.awayCorners : result.homeCorners;
    const string opponent = myHome ? away.name : home.name;
    const Team& myTeam = myHome ? home : away;
    const Team& oppTeam = myHome ? away : home;

    string verdict = (myGoals > oppGoals) ? "Partido controlado"
                     : (myGoals < oppGoals) ? "Derrota con ajustes pendientes"
                                            : "Empate cerrado";
    string recommendation = "Mantener base y ajustar rotacion segun forma individual";
    string reason = (myTeam.matchInstruction == "Juego directo" && oppTeam.defensiveLine >= 4)
                        ? "se encontro espacio a la espalda"
                        : (myTeam.pressingIntensity >= 4 && oppPoss <= 46)
                              ? "la presion sostuvo recuperaciones altas"
                              : (oppTeam.pressingIntensity >= 4 && myPoss <= 45)
                                    ? "costo salir ante la presion rival"
                                    : "el partido se definio por detalles";

    if (myShots < oppShots && myPoss < 50) verdict += ", falto presencia ofensiva";
    else if (myShots > oppShots && myGoals <= oppGoals) verdict += ", falto eficacia";
    else if (myPoss >= 55 && myGoals >= oppGoals) verdict += ", buen control del ritmo";
    if (myCorners > oppCorners + 2) verdict += ", se cargo el area rival";
    if (myPoss < 45 && myGoals > oppGoals) verdict += ", transiciones muy efectivas";
    if (myShots + 2 < oppShots && myPoss < 48) {
        recommendation = "Subir un punto la altura de linea o el tempo para recuperar iniciativa";
    } else if (myGoals < oppGoals && myShots >= oppShots) {
        recommendation = "Trabajar finalizacion y pelota parada en la semana";
    } else if (myCorners + 2 < oppCorners) {
        recommendation = "Explorar 'Por bandas' o laterales mas altos para cargar el area";
    } else if (myPoss >= 55 && myGoals == 0) {
        recommendation = "Mantener posesion pero acelerar ultimo tercio con juego mas directo";
    }
    if (!result.report.explanation.likelyReason.empty()) {
        reason = result.report.explanation.likelyReason;
    }
    if (!result.report.explanation.tacticalStory.empty()) {
        recommendation = result.report.explanation.tacticalStory;
    }

    career.lastMatchAnalysis =
        string(cupMatch ? "Copa" : "Liga") + ": " + career.myTeam->name + " " +
        to_string(myGoals) + "-" + to_string(oppGoals) + " " + opponent +
        " | Tiros " + to_string(myShots) + "-" + to_string(oppShots) +
        " | Posesion " + to_string(myPoss) + "-" + to_string(oppPoss) +
        " | Corners " + to_string(myCorners) + "-" + to_string(oppCorners) +
        " | xG " + buildCompactXgText(myHome ? result.stats.homeExpectedGoals : result.stats.awayExpectedGoals) + "/" +
        buildCompactXgText(myHome ? result.stats.awayExpectedGoals : result.stats.homeExpectedGoals) +
        " | Cambios " + to_string(mySubs) + "-" + to_string(oppSubs) +
        " | Clima " + result.weather +
        " | " + lineMap(myTeam) +
        " | " + verdict +
        " | Clave: " + reason +
        " | Recomendacion: " + recommendation +
        (result.report.explanation.fatigueStory.empty() ? "" : " | Fatiga: " + result.report.explanation.fatigueStory) +
        (result.report.playerOfTheMatch.empty() ? "" : " | Figura: " + result.report.playerOfTheMatch) +
        (result.report.postMatchImpact.empty() ? "" : " | Post: " + result.report.postMatchImpact);

    career.lastMatchReportLines = result.reportLines;
    career.lastMatchEvents = result.events;
    career.lastMatchPlayerOfTheMatch = result.report.playerOfTheMatch;
    match_center_service::captureLastMatchCenter(career, home, away, result, cupMatch);
}

string buildLastMatchInsightText(const Career& career,
                                 size_t maxReportLines,
                                 size_t maxEvents) {
    if (career.lastMatchAnalysis.empty() && career.lastMatchReportLines.empty() && career.lastMatchEvents.empty()) {
        return "No hay ultimo analisis de partido.";
    }

    ostringstream out;
    out << match_center_service::formatLastMatchCenter(career, min(maxReportLines, static_cast<size_t>(4)), maxEvents) << "\r\n";
    if (!career.lastMatchAnalysis.empty()) {
        out << "\r\nMatchReport\r\n";
        const size_t count = min(maxReportLines, career.lastMatchReportLines.size());
        for (size_t i = 0; i < count; ++i) {
            out << "- " << career.lastMatchReportLines[i] << "\r\n";
        }
    }
    appendSection(out, "\r\nMatchTimeline", career.lastMatchEvents, maxEvents);
    return out.str();
}

}  // namespace career_match_analysis
