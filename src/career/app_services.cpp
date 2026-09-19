#include "app_services.h"

#include "ai/ai_transfer_manager.h"
#include "career/season_flow_controller.h"
#include "career/career_reports.h"
#include "career/career_runtime.h"
#include "career/career_support.h"
#include "career/match_center_service.h"
#include "career/weekly_focus_service.h"
#include "career/world_state_service.h"
#include "career/week_simulation.h"
#include "career/team_management.h"
#include "career/dressing_room_service.h"
#include "career/inbox_service.h"
#include "career/medical_service.h"
#include "career/staff_service.h"
#include "competition.h"
#include "development/training_impact_system.h"
#include "simulation/player_condition.h"
#include "transfers/negotiation_system.h"
#include "transfers/transfer_market.h"
#include "engine/social_system.h"
#include "engine/rival_ai.h"
#include "engine/rivalry_system.h"
#include "engine/debt_system.h"
#include "engine/facilities_system.h"
#include "utils.h"

#include <algorithm>
#include <sstream>

using namespace std;

namespace {

ServiceResult failure(const string& message) {
    ServiceResult result;
    result.ok = false;
    result.messages.push_back(message);
    return result;
}

}  // namespace

ServiceResult changeYouthRegionService(Career& career, const string& region) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    static const vector<string> regions = {"Metropolitana", "Norte", "Centro", "Sur", "Patagonia"};
    if (find(regions.begin(), regions.end(), region) == regions.end()) {
        return failure("La region juvenil indicada no es valida.");
    }
    Team& team = *career.myTeam;
    if (team.youthRegion == region) return failure("Esa region juvenil ya esta activa.");
    const long long cost = 12000LL;
    if (team.budget < cost) return failure("Presupuesto insuficiente para reorientar la captacion juvenil.");
    team.budget -= cost;
    team.youthRegion = region;
    ensureTeamIdentity(team);
    career.addNews(team.name + " reorienta su captacion juvenil hacia " + team.youthRegion + ".");
    ServiceResult result;
    result.ok = true;
    result.messages.push_back("Nueva region juvenil: " + region + ". Inversion " + formatMoneyValue(cost) + ".");
    return result;
}

ServiceResult takeManagerJobService(Career& career, const string& teamName, const string& reason) {
    if (!career.myTeam) return failure("No hay una carrera activa.");
    Team* team = career.findTeamByName(teamName);
    if (!team) return failure("No se encontro el club seleccionado.");
    if (team == career.myTeam) return failure("Ya diriges ese club.");
    takeManagerJob(career, team, reason.empty() ? string("Cambio de club voluntario.") : reason);
    ServiceResult result;
    result.ok = true;
    result.messages.push_back("Nuevo destino: " + career.myTeam->name + ".");
    return result;
}
