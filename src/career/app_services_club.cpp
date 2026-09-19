#include "career/app_services.h"

#include "career/career_reports.h"
#include "career/staff_service.h"
#include "career/world_state_service.h"
#include "engine/models.h"
#include "utils/utils.h"

#include <algorithm>

using namespace std;

namespace {

ServiceResult failure(const string& message) {
    ServiceResult result;
    result.ok = false;
    result.messages.push_back(message);
    return result;
}

void syncInfrastructureFromTeam(Career& career, const Team& team) {
    career.infrastructure.levels.trainingGround = clampInt(team.trainingFacilityLevel, 1, 5);
    career.infrastructure.levels.youthAcademy = clampInt(team.youthFacilityLevel, 1, 5);
    career.infrastructure.levels.medical = clampInt(1 + team.medicalTeam / 25, 1, 5);
    career.infrastructure.levels.stadium = clampInt(team.stadiumLevel, 1, 5);
    career.infrastructure.levels.facilities = clampInt(1 + team.assistantCoach / 30, 1, 5);
}

ClubUpgrade staffUpgradeForRole(const string& role) {
    const string lower = toLower(trim(role));
    if (lower.find("asistente") != string::npos) return ClubUpgrade::AssistantCoach;
    if (lower.find("preparador") != string::npos) return ClubUpgrade::FitnessCoach;
    if (lower.find("scouting") != string::npos) return ClubUpgrade::Scouting;
    if (lower.find("juveniles") != string::npos) return ClubUpgrade::YouthCoach;
    if (lower.find("medico") != string::npos) return ClubUpgrade::Medical;
    if (lower.find("arquero") != string::npos) return ClubUpgrade::GoalkeepingCoach;
    return ClubUpgrade::PerformanceAnalyst;
}

string nextStaffHireName(const Team& team, const string& roleLabel, int currentLevel) {
    static const vector<string> firstNames = {"Oscar", "German", "Fabian", "Leonardo", "Victor", "Damian", "Ignacio", "Tomas"};
    static const vector<string> lastNames = {"Navarro", "Rivera", "Carrasco", "Saavedra", "Abarca", "Henriquez", "Vera", "Olivares"};
    int hash = currentLevel;
    const string key = normalizeTeamId(team.name) + normalizeTeamId(roleLabel);
    for (char ch : key) hash += static_cast<unsigned char>(ch);
    return firstNames[static_cast<size_t>(hash % static_cast<int>(firstNames.size()))] + " " +
           lastNames[static_cast<size_t>((hash / 7) % static_cast<int>(lastNames.size()))];
}

long long upgradeCost(const Team& team, ClubUpgrade upgrade) {
    switch (upgrade) {
        case ClubUpgrade::Stadium: return 60000LL * (team.stadiumLevel + 1);
        case ClubUpgrade::Youth: return 50000LL * (team.youthFacilityLevel + 1);
        case ClubUpgrade::Training: return 55000LL * (team.trainingFacilityLevel + 1);
        case ClubUpgrade::Scouting: return 36000LL + static_cast<long long>(team.scoutingChief) * 1300LL;
        case ClubUpgrade::Medical: return 33000LL + static_cast<long long>(team.medicalTeam) * 1200LL;
        case ClubUpgrade::AssistantCoach: return 35000LL + static_cast<long long>(team.assistantCoach) * 1200LL;
        case ClubUpgrade::FitnessCoach: return 32000LL + static_cast<long long>(team.fitnessCoach) * 1200LL;
        case ClubUpgrade::YouthCoach: return 34000LL + static_cast<long long>(team.youthCoach) * 1200LL;
        case ClubUpgrade::GoalkeepingCoach: return 31000LL + static_cast<long long>(team.goalkeepingCoach) * 1100LL;
        case ClubUpgrade::PerformanceAnalyst: return 30000LL + static_cast<long long>(team.performanceAnalyst) * 1100LL;
    }
    return 0;
}

string upgradeLabel(ClubUpgrade upgrade) {
    switch (upgrade) {
        case ClubUpgrade::Stadium: return "estadio";
        case ClubUpgrade::Youth: return "cantera";
        case ClubUpgrade::Training: return "entrenamiento";
        case ClubUpgrade::Scouting: return "scouting";
        case ClubUpgrade::Medical: return "medico";
        case ClubUpgrade::AssistantCoach: return "asistente tecnico";
        case ClubUpgrade::FitnessCoach: return "preparador fisico";
        case ClubUpgrade::YouthCoach: return "jefe de juveniles";
        case ClubUpgrade::GoalkeepingCoach: return "entrenador de arqueros";
        case ClubUpgrade::PerformanceAnalyst: return "analista de rendimiento";
    }
    return "club";
}

bool isFacilityUpgrade(ClubUpgrade upgrade) {
    return upgrade == ClubUpgrade::Stadium ||
           upgrade == ClubUpgrade::Youth ||
           upgrade == ClubUpgrade::Training ||
           upgrade == ClubUpgrade::Medical;
}

}  // namespace

ServiceResult upgradeClubService(Career& career, ClubUpgrade upgrade) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    Team& team = *career.myTeam;
    ensureTeamIdentity(team);
    if (isFacilityUpgrade(upgrade) && career.debtStatus.debtSeverity >= 70) {
        return failure("La deuda bloquea inversiones de infraestructura hasta recuperar estabilidad financiera.");
    }
    long long cost = upgradeCost(team, upgrade);
    if (team.budget < cost) return failure("Presupuesto insuficiente para mejorar " + upgradeLabel(upgrade) + ".");
    team.budget -= cost;
    string message;
    switch (upgrade) {
        case ClubUpgrade::Stadium:
            team.stadiumLevel++;
            team.fanBase += 3;
            team.sponsorWeekly += 5000;
            message = team.name + " amplia su estadio.";
            break;
        case ClubUpgrade::Youth:
            team.youthFacilityLevel++;
            message = team.name + " mejora su cantera.";
            break;
        case ClubUpgrade::Training:
            team.trainingFacilityLevel++;
            team.goalkeepingCoach = clampInt(team.goalkeepingCoach + 3, 1, 99);
            if (team.goalkeepingCoachName.empty()) team.goalkeepingCoachName = nextStaffHireName(team, "arquero", team.goalkeepingCoach);
            message = team.name + " mejora su centro de entrenamiento.";
            break;
        case ClubUpgrade::Scouting:
            team.scoutingChief = clampInt(team.scoutingChief + 5, 1, 99);
            team.scoutingChiefName = nextStaffHireName(team, "scouting", team.scoutingChief);
            for (const auto& regionName : world_state_service::listConfiguredScoutingRegions()) {
                const bool coveredRegion =
                    regionName.empty() || regionName == "Todas" ||
                    find(team.scoutingRegions.begin(), team.scoutingRegions.end(), regionName) != team.scoutingRegions.end();
                if (!coveredRegion) {
                    team.scoutingRegions.push_back(regionName);
                    break;
                }
            }
            message = team.name + " fortalece su red de scouting con " + team.scoutingChiefName + ".";
            break;
        case ClubUpgrade::Medical:
            team.medicalTeam = clampInt(team.medicalTeam + 5, 1, 99);
            team.medicalChiefName = nextStaffHireName(team, "medico", team.medicalTeam);
            message = team.name + " fortalece el cuerpo medico con " + team.medicalChiefName + ".";
            break;
        case ClubUpgrade::AssistantCoach:
            team.assistantCoach = clampInt(team.assistantCoach + 5, 1, 99);
            team.performanceAnalyst = clampInt(team.performanceAnalyst + 3, 1, 99);
            team.assistantCoachName = nextStaffHireName(team, "assistant", team.assistantCoach);
            if (team.performanceAnalystName.empty()) team.performanceAnalystName = nextStaffHireName(team, "analyst", team.performanceAnalyst);
            message = team.name + " refuerza su cuerpo tecnico con " + team.assistantCoachName + ".";
            break;
        case ClubUpgrade::FitnessCoach:
            team.fitnessCoach = clampInt(team.fitnessCoach + 5, 1, 99);
            team.fitnessCoachName = nextStaffHireName(team, "fitness", team.fitnessCoach);
            message = team.name + " mejora su preparacion fisica con " + team.fitnessCoachName + ".";
            break;
        case ClubUpgrade::YouthCoach:
            team.youthCoach = clampInt(team.youthCoach + 5, 1, 99);
            team.youthCoachName = nextStaffHireName(team, "youth", team.youthCoach);
            message = team.name + " mejora la conduccion de juveniles con " + team.youthCoachName + ".";
            break;
        case ClubUpgrade::GoalkeepingCoach:
            team.goalkeepingCoach = clampInt(team.goalkeepingCoach + 5, 1, 99);
            team.goalkeepingCoachName = nextStaffHireName(team, "goalkeeping", team.goalkeepingCoach);
            message = team.name + " incorpora a " + team.goalkeepingCoachName + " para el trabajo de arqueros.";
            break;
        case ClubUpgrade::PerformanceAnalyst:
            team.performanceAnalyst = clampInt(team.performanceAnalyst + 5, 1, 99);
            team.performanceAnalystName = nextStaffHireName(team, "analyst", team.performanceAnalyst);
            message = team.name + " suma al analista " + team.performanceAnalystName + ".";
            break;
    }
    ensureTeamIdentity(team);
    syncInfrastructureFromTeam(career, team);
    career.infrastructure.upgradesThisSeason++;
    career.addNews(message);
    ServiceResult result;
    result.ok = true;
    result.messages.push_back(message + " Inversion " + formatMoneyValue(cost) + ".");
    return result;
}

ServiceResult reviewStaffStructureService(Career& career) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    Team& team = *career.myTeam;
    ensureTeamIdentity(team);
    const string weakestRole = staff_service::weakestStaffRole(team);
    ServiceResult result = upgradeClubService(career, staffUpgradeForRole(weakestRole));
    if (result.ok) {
        result.messages.insert(result.messages.begin(), "Revision de staff: se prioriza reforzar " + weakestRole + ".");
    } else {
        result.messages.insert(result.messages.begin(), "Revision de staff: el area mas debil hoy es " + weakestRole + ".");
    }
    return result;
}
