#include "gui/gui_view_builders.h"

#ifdef _WIN32

#include "ai/ai_transfer_manager.h"
#include "engine/team_personality.h"
#include "career/inbox_service.h"
#include "career/manager_advice.h"
#include "career/career_support.h"
#include "finance/finance_system.h"
#include "transfers/negotiation_system.h"
#include "ui/economy_fairplay.h"
#include "utils/utils.h"

#include <algorithm>
#include <sstream>

namespace {

bool lineMatchesAnyKeyword(const std::string& line, const std::vector<std::string>& keywords) {
    const std::string lower = toLower(line);
    for (const std::string& keyword : keywords) {
        if (lower.find(keyword) != std::string::npos) return true;
    }
    return false;
}

std::vector<std::string> selectFeedLines(const std::vector<std::string>& source,
                                         const std::vector<std::string>& keywords,
                                         std::size_t limit,
                                         const std::vector<std::string>& fallback) {
    std::vector<std::string> out;
    for (const std::string& line : source) {
        if (!lineMatchesAnyKeyword(line, keywords)) continue;
        out.push_back(line);
        if (out.size() >= limit) return out;
    }
    if (!fallback.empty()) {
        const std::size_t copyCount = std::min(limit, fallback.size());
        out.insert(out.end(), fallback.begin(), fallback.begin() + static_cast<long long>(copyCount));
    }
    if (out.empty()) out.push_back("No hay entradas para el filtro actual.");
    return out;
}

struct BoardPressureSnapshot {
    int objectivePercent = 0;
    int weeksRemaining = 0;
    int pressureScore = 0;
    int jobRiskScore = 0;
    int budgetRiskScore = 0;
    std::string objectiveState;
    std::string pressureLabel;
    std::string mandate;
    std::string nextMilestone;
    std::string riskReason;
    std::string recommendation;
};

bool isRankingObjective(const std::string& objective) {
    const std::string lower = toLower(objective);
    return lower.find("puesto") != std::string::npos ||
           lower.find("posicion") != std::string::npos ||
           lower.find("top") != std::string::npos ||
           lower.find("entrar") != std::string::npos;
}

std::string boardPressureLabel(int score) {
    if (score >= 78) return "Critica";
    if (score >= 58) return "Alta";
    if (score >= 36) return "Media";
    return "Controlada";
}

std::string boardMandateLabel(const Career& career) {
    if (!career.myTeam) return "Sin mandato activo";
    if (career.boardConfidence < 25 || career.boardWarningWeeks >= 4) return "Ultimatum deportivo";
    if (career.boardConfidence < 42 || career.boardWarningWeeks > 0) return "Reaccion inmediata";
    if (career.boardConfidence >= 72 && career.myTeam->jobSecurity >= 65) return "Proyecto respaldado";
    if (career.myTeam->transferPolicy.find("Cantera") != std::string::npos) return "Construir valor joven";
    if (career.myTeam->debt > career.myTeam->budget * 2) return "Orden financiero";
    return "Cumplir objetivos";
}

BoardPressureSnapshot buildBoardPressureSnapshot(const Career& career) {
    BoardPressureSnapshot snapshot;
    snapshot.weeksRemaining = std::max(0, career.boardMonthlyDeadlineWeek - career.currentWeek);
    snapshot.mandate = boardMandateLabel(career);

    if (!career.myTeam) {
        snapshot.objectiveState = "Sin club";
        snapshot.pressureLabel = "Sin datos";
        snapshot.nextMilestone = "Inicia una carrera";
        snapshot.riskReason = "No hay directiva activa.";
        snapshot.recommendation = "Crear o cargar carrera.";
        return snapshot;
    }

    const Team& team = *career.myTeam;
    const bool hasObjective = !career.boardMonthlyObjective.empty() && career.boardMonthlyTarget > 0;
    const bool rankingObjective = isRankingObjective(career.boardMonthlyObjective);
    bool objectiveMet = true;
    if (hasObjective) {
        objectiveMet = rankingObjective
            ? career.boardMonthlyProgress <= career.boardMonthlyTarget
            : career.boardMonthlyProgress >= career.boardMonthlyTarget;
        if (rankingObjective) {
            snapshot.objectivePercent = objectiveMet
                ? 100
                : clampInt(100 - (career.boardMonthlyProgress - career.boardMonthlyTarget) * 18, 0, 100);
        } else {
            snapshot.objectivePercent = clampInt(career.boardMonthlyProgress * 100 / std::max(1, career.boardMonthlyTarget), 0, 140);
        }
    } else {
        snapshot.objectivePercent = career.boardConfidence;
    }

    if (!hasObjective) {
        snapshot.objectiveState = "Sin objetivo mensual";
        snapshot.nextMilestone = "Mantener confianza " + std::to_string(career.boardConfidence) + "/100";
    } else if (objectiveMet) {
        snapshot.objectiveState = "En camino";
        snapshot.nextMilestone = "Sostener objetivo: " + std::to_string(career.boardMonthlyProgress) +
                                 "/" + std::to_string(career.boardMonthlyTarget);
    } else if (snapshot.weeksRemaining <= 1) {
        snapshot.objectiveState = "Urgente";
        snapshot.nextMilestone = "Resolver en " + std::to_string(snapshot.weeksRemaining) + " semana(s)";
    } else {
        snapshot.objectiveState = "En riesgo";
        snapshot.nextMilestone = "Cerrar brecha: " + std::to_string(career.boardMonthlyProgress) +
                                 "/" + std::to_string(career.boardMonthlyTarget);
    }

    long long weeklyWages = 0;
    for (const Player& player : team.players) weeklyWages += player.wage;
    snapshot.budgetRiskScore = team.budget < weeklyWages * 5 ? 26 : (team.debt > team.budget * 2 ? 18 : 0);
    snapshot.jobRiskScore = clampInt((100 - team.jobSecurity) * 2 / 3 + std::max(0, 45 - career.boardConfidence), 0, 100);

    int pressure = 100 - career.boardConfidence;
    pressure += career.boardWarningWeeks * 9;
    pressure += snapshot.jobRiskScore / 3;
    pressure += snapshot.budgetRiskScore;
    if (hasObjective && !objectiveMet) pressure += snapshot.weeksRemaining <= 1 ? 24 : 14;
    if (team.morale < 45) pressure += 8;
    if (team.points < std::max(0, career.currentWeek - 1)) pressure += 6;
    snapshot.pressureScore = clampInt(pressure, 0, 100);
    snapshot.pressureLabel = boardPressureLabel(snapshot.pressureScore);

    if (career.boardConfidence < 35) {
        snapshot.riskReason = "La confianza de directiva esta por debajo del umbral seguro.";
    } else if (career.boardWarningWeeks > 0) {
        snapshot.riskReason = "Hay semanas de advertencia acumuladas.";
    } else if (hasObjective && !objectiveMet) {
        snapshot.riskReason = "El objetivo mensual todavia no esta resuelto.";
    } else if (snapshot.budgetRiskScore > 0) {
        snapshot.riskReason = "La caja o deuda reduce margen de maniobra.";
    } else {
        snapshot.riskReason = "La directiva no ve una amenaza inmediata.";
    }

    if (snapshot.pressureScore >= 70) {
        snapshot.recommendation = "Prioriza resultado inmediato y comunica progreso visible a la directiva.";
    } else if (hasObjective && !objectiveMet) {
        snapshot.recommendation = "Alinea XI, microciclo y mercado con el objetivo mensual.";
    } else if (snapshot.budgetRiskScore > 0) {
        snapshot.recommendation = "Controla salarios y evita compras sin salida previa.";
    } else if (team.jobSecurity < 45) {
        snapshot.recommendation = "Mejora estabilidad: staff, vestuario y puntos en la proxima fecha.";
    } else {
        snapshot.recommendation = "Mantener proyecto y revisar staff cuando haya margen.";
    }
    return snapshot;
}

}  // namespace

namespace gui_win32 {

namespace {

NegotiationPromise suggestedTransferPromise(const std::string& expectedRole, const Player& player) {
    const std::string lower = toLower(expectedRole);
    if (lower.find("titular") != std::string::npos) return NegotiationPromise::Starter;
    if (lower.find("proyecto") != std::string::npos || (player.age <= 21 && player.potential >= player.skill + 10)) {
        return NegotiationPromise::Prospect;
    }
    if (lower.find("rotacion") != std::string::npos) return NegotiationPromise::Rotation;
    return NegotiationPromise::None;
}

std::string negotiationStateLabel(const NegotiationState& state) {
    if (state.clubAccepted && state.playerAccepted) return "Acuerdo posible";
    if (state.clubAccepted) return "Falta jugador";
    if (state.playerAccepted) return "Falta club";
    return "Riesgo alto";
}

std::string buildNegotiationRoomText(const Career& career,
                                     const Team& buyer,
                                     const Team& seller,
                                     const Player& player,
                                     const TransferPreviewItem* preview) {
    const std::string expectedRole = preview ? preview->expectedRole : expectedRoleLabel(buyer, player);
    const NegotiationPromise promise = suggestedTransferPromise(expectedRole, player);
    std::ostringstream out;
    out << "Mesa de negociacion\r\n";
    out << "Rol sugerido " << promiseLabel(promise)
        << " | perfil jugador " << personalityLabel(player)
        << " | agente " << agentDifficulty(player) << "/100"
        << " | competencia en puesto " << squadCompetitionAtRole(buyer, player) << "\r\n";
    out << "Vendedor " << seller.name
        << " | interes " << sellerInterestLabel(seller, player)
        << " | jugador " << playerInterestLabel(career, seller, player) << "\r\n";
    if (preview) {
        out << "Radar " << preview->scoutingLabel
            << " | mercado " << preview->marketLabel
            << " | accion sugerida " << preview->actionLabel << "\r\n";
    }

    const std::vector<NegotiationProfile> profiles = {
        NegotiationProfile::Safe,
        NegotiationProfile::Balanced,
        NegotiationProfile::Aggressive
    };
    for (NegotiationProfile profile : profiles) {
        const NegotiationState state = runTransferNegotiation(career, buyer, seller, player, profile, promise);
        const long long firstSeasonPackage =
            state.agreedFee + state.agreedBonus + state.agreedAgentFee + state.agreedLoyaltyBonus +
            state.agreedAppearanceBonus + state.agreedWage * 52;
        out << "- Perfil " << negotiationLabel(profile)
            << ": " << negotiationStateLabel(state)
            << " | fee " << formatMoneyValue(state.agreedFee > 0 ? state.agreedFee : state.latestCounter)
            << " | salario " << formatMoneyValue(state.agreedWage > 0 ? state.agreedWage : state.playerDemand)
            << " | paquete 1a temp " << formatMoneyValue(firstSeasonPackage)
            << " | estado " << state.status << "\r\n";
    }
    out << "Lectura: usa perfil seguro si el jugador es prioridad; agresivo solo si puedes perder la operacion.";
    return out.str();
}

}  // namespace

GuiPageModel buildTransfersModel(AppState& state) {
    GuiPageModel model;
    std::vector<std::string> alerts = buildAlertLines(state.career);
    const auto actionLines = manager_advice::buildManagerActionLines(state.career, 3);
    model.title = pageTitleFor(state.currentPage);
    model.breadcrumb = breadcrumbFor(state.currentPage);
    model.metrics = buildMetrics(state, alerts);
    model.infoLine = "Mercado de fichajes con filtros, lectura del plantel y objetivos prioritarios.";
    model.summary.title = "TransferSearchPanel";
    model.primary.title = "TransferMarketView";
    model.primary.columns = {
        {L"Jugador", 190}, {L"Pos", 54}, {L"Edad", 54}, {L"Media", 58}, {L"Pot", 58},
        {L"Costo", 92}, {L"Salario", 92}, {L"Radar", 92}, {L"Mercado", 100}, {L"Rol", 120}, {L"Club", 180}
    };
    model.secondary.title = "TransferSearchPanel";
    model.secondary.columns = {{L"Area", 120}, {L"Valor", 120}, {L"Lectura", 280}};
    model.footer = buildTransferPipelineModel(state.career);
    model.detail.title = "TransferTargetCard";
    model.feed.title = "NewsFeedPanel";
    model.feed.lines = buildFeedLines(state.career, "Mercado");

    if (!state.career.myTeam) {
        model.summary.content = "Sin carrera activa.";
        model.detail.content = "Inicia una carrera para abrir el mercado.";
        return model;
    }

    Team& team = *state.career.myTeam;
    const ClubTransferStrategy strategy = ai_transfer_manager::buildClubTransferStrategy(state.career, team);
    model.summary.content = "Presupuesto actual " + formatMoneyValue(team.budget) + "\r\n"
                            "Filtro " + state.currentFilter + "\r\n"
                            "Pendientes " + std::to_string(state.career.pendingTransfers.size()) + "\r\n"
                            "Shortlist " + std::to_string(state.career.scoutingShortlist.size()) + "\r\n"
                            "Red scouting " + (team.scoutingRegions.empty() ? std::string("-") : joinStringValues(team.scoutingRegions, ", ")) +
                            (actionLines.empty() ? std::string() : "\r\nPlan corto: " + actionLines.front());
    model.secondary.rows.push_back({"Necesidad", detectScoutingNeed(team), "Lectura del plantel actual"});
    model.secondary.rows.push_back({"Presupuesto", formatMoneyValue(team.budget), "Define agresividad de mercado"});
    model.secondary.rows.push_back({"Contratos cortos", std::to_string(std::count_if(team.players.begin(), team.players.end(), [](const Player& p) { return p.contractWeeks <= 12; })),
                                    "Renovar antes de fichar profundidad"});
    model.secondary.rows.push_back({"Scouting", std::to_string(team.scoutingChief), "Mejora precision de objetivos"});
    model.secondary.rows.push_back({"Politica", team.transferPolicy, "Define si el club compra valor, cantera o urgencia"});
    model.secondary.rows.push_back({"Venta IA", std::to_string(strategy.salePressure), "Presion para liberar suplentes marginales"});

    std::vector<TransferPreviewItem> targets = buildTransferTargets(state.career, state.currentFilter, state.globalSearchActive ? state.globalSearchQuery : std::string());
    if (state.selectedTransferPlayer.empty() && !targets.empty()) {
        state.selectedTransferPlayer = targets.front().player;
        state.selectedTransferClub = targets.front().club;
    }
    for (const auto& target : targets) {
        model.primary.rows.push_back({
            target.player,
            target.position,
            std::to_string(target.age),
            target.skillLabel.empty() ? std::to_string(target.skill) : target.skillLabel,
            target.potentialLabel.empty() ? std::to_string(target.potential) : target.potentialLabel,
            target.expectedFeeLabel.empty() ? formatMoneyValue(target.expectedFee) : target.expectedFeeLabel,
            target.expectedWageLabel.empty() ? formatMoneyValue(target.expectedWage) : target.expectedWageLabel,
            target.scoutingLabel,
            target.marketLabel,
            target.expectedRole,
            target.club
        });
    }
    if (model.primary.rows.empty()) {
        model.primary.rows.push_back({"Sin objetivos", "-", "-", "-", "-", "-", "-", "-", "-", "-", "-"});
    }

    const Team* myTeam = state.career.myTeam;
    const Team* seller = state.career.findTeamByName(state.selectedTransferClub);
    const Player* player = seller ? findPlayerByName(*seller, state.selectedTransferPlayer) : nullptr;
    auto selectedPreview = std::find_if(targets.begin(), targets.end(), [&](const TransferPreviewItem& item) {
        return item.player == state.selectedTransferPlayer && item.club == state.selectedTransferClub;
    });
    if (!myTeam || !seller || !player) {
        model.detail.content = "Selecciona un objetivo para ver detalle.";
    } else {
        std::ostringstream detail;
        detail << player->name << " | " << seller->name << " | " << normalizePosition(player->position) << "\r\n";
        if (selectedPreview != targets.end()) {
            detail << "Media " << selectedPreview->skillLabel
                   << " | Potencial " << selectedPreview->potentialLabel
                   << " | Confianza informe " << selectedPreview->scoutingConfidence << "%"
                   << " | Edad " << player->age << "\r\n";
            detail << "Costo estimado "
                   << (selectedPreview->expectedFeeLabel.empty() ? formatMoneyValue(selectedPreview->expectedFee) : selectedPreview->expectedFeeLabel)
                   << " | Agente "
                   << (selectedPreview->expectedAgentFeeLabel.empty() ? formatMoneyValue(selectedPreview->expectedAgentFee)
                                                                      : selectedPreview->expectedAgentFeeLabel)
                   << " | Salario esperado "
                   << (selectedPreview->expectedWageLabel.empty() ? formatMoneyValue(selectedPreview->expectedWage)
                                                                  : selectedPreview->expectedWageLabel)
                   << "\r\n";
            detail << "Radar " << selectedPreview->scoutingLabel
                   << " | Mercado " << selectedPreview->marketLabel << "\r\n";
            detail << "Riesgo informe: " << scoutingRiskLabel(selectedPreview->scoutingConfidence) << "\r\n";
            detail << "Siguiente paso: " << scoutingNextStepLabel(*selectedPreview) << "\r\n";
            detail << "Competencia " << selectedPreview->competitionLabel
                   << " | Recomendacion " << selectedPreview->actionLabel << "\r\n";
            detail << "Plan economico: " << selectedPreview->packageLabel << "\r\n";
            detail << "Lectura scouting: " << selectedPreview->scoutingNote << "\r\n";
        } else {
            detail << "Media " << player->skill << " | Potencial " << player->potential << " | Edad " << player->age << "\r\n";
            detail << "Valor " << formatMoneyValue(player->value) << " | Salario " << formatMoneyValue(player->wage) << "\r\n";
        }
        detail << "Interes jugador " << playerInterestLabel(state.career, *seller, *player)
               << " | Interes vendedor " << sellerInterestLabel(*seller, *player) << "\r\n";
        detail << "Rol esperado " << expectedRoleLabel(*myTeam, *player) << "\r\n";
        detail << "Disponibilidad " << (player->contractWeeks <= 12 ? "Contrato corto" : (player->wantsToLeave ? "Abierto a salir" : "Negociacion dura"))
               << " | Agente " << (agentDifficulty(*player) >= 72 ? "duro" : (agentDifficulty(*player) >= 58 ? "exigente" : "manejable")) << "\r\n";
        detail << "Rasgos: " << (player->traits.empty() ? "-" : joinStringValues(player->traits, ", ")) << "\r\n\r\n";
        detail << buildNegotiationRoomText(state.career,
                                           *myTeam,
                                           *seller,
                                           *player,
                                           selectedPreview != targets.end() ? &(*selectedPreview) : nullptr);
        model.detail.content = detail.str();
    }
    return model;
}

GuiPageModel buildFinancesModel(AppState& state) {
    GuiPageModel model;
    std::vector<std::string> alerts = buildAlertLines(state.career);
    const auto actionLines = manager_advice::buildManagerActionLines(state.career, 4);
    const auto storyLines = manager_advice::buildCareerStorylines(state.career, 2);
    model.title = pageTitleFor(state.currentPage);
    model.breadcrumb = breadcrumbFor(state.currentPage);
    model.metrics = buildMetrics(state, alerts);
    model.infoLine = "Finanzas, salarios e infraestructura del club.";
    model.summary.title = "FinanceSummary";
    model.primary.title = "FinanceBreakdown";
    model.primary.columns = {{L"Cuenta", 180}, {L"Valor", 140}, {L"Lectura", 280}};
    model.secondary.title = "SalaryTable";
    model.secondary.columns = {{L"Jugador", 200}, {L"Salario", 110}, {L"Contrato", 70}, {L"Rol", 110}, {L"Riesgo", 160}};
    model.footer.title = "Infrastructure";
    model.footer.columns = {{L"Area", 160}, {L"Nivel", 70}, {L"Staff", 80}, {L"Impacto", 260}};
    model.detail.title = "ClubFinancePanel";
    model.feed.title = "AlertPanel";
    model.feed.lines = alerts;

    if (!state.career.myTeam) {
        model.summary.content = "Sin club activo.";
        return model;
    }

    Team& team = *state.career.myTeam;
    WeeklyFinanceReport finance = finance_system::projectWeeklyReport(team);
    economy_fairplay::EconomyFairPlaySystem::initialize(state.career);
    const long long allowedSalary = economy_fairplay::EconomyFairPlaySystem::getMaxAllowedSalary(team, state.career);
    const int salaryPressure = static_cast<int>(finance.wageBill * 100 / std::max(1LL, allowedSalary));
    const auto fairPlayViolations = economy_fairplay::EconomyFairPlaySystem::getTeamViolations(team, state.career);
    model.summary.content = buildClubSummaryService(state.career) + "\r\nFiltro actual: " + state.currentFilter;

    std::vector<std::vector<std::string> > financeRows = {
        {"Presupuesto", formatMoneyValue(team.budget), team.budget >= 0 ? "Caja operativa positiva" : "Caja comprometida"},
        {"Deuda", formatMoneyValue(team.debt), team.debt > 0 ? "Vigilar amortizacion" : "Sin deuda relevante"},
        {"Sponsor semanal", formatMoneyValue(finance.sponsorIncome), "Ingreso fijo"},
        {"Taquilla estimada", formatMoneyValue(finance.matchdayIncome), "Proyeccion de partido local"},
        {"Merchandising", formatMoneyValue(finance.merchandisingIncome), "Pulso comercial del club"},
        {"Bonos variables", formatMoneyValue(finance.bonusIncome), "Premios por rendimiento"},
        {"Masa salarial", formatMoneyValue(finance.wageBill), "Costo semanal proyectado"},
        {"Buffer de mercado", formatMoneyValue(finance.transferBuffer), finance.riskLevel}
    };
    financeRows.push_back({"Fair play salarial",
                           formatMoneyValue(allowedSalary),
                           "Uso " + std::to_string(salaryPressure) + "% del maximo recomendado"});
    financeRows.push_back({"Riesgos fair play",
                           std::to_string(fairPlayViolations.size()),
                           fairPlayViolations.empty() ? "Sin alertas activas" : "Revisar detalle financiero"});

    std::vector<const Player*> players;
    for (const auto& player : team.players) players.push_back(&player);
    std::sort(players.begin(), players.end(), [](const Player* left, const Player* right) { return left->wage > right->wage; });
    std::vector<std::vector<std::string> > salaryRows;
    for (size_t i = 0; i < players.size() && i < 14; ++i) {
        const Player& player = *players[i];
        std::string risk = player.contractWeeks <= 12 ? "Vence pronto" : (player.wantsToLeave ? "Quiere salir" : "Controlado");
        salaryRows.push_back({
            player.name, formatMoneyValue(player.wage), std::to_string(player.contractWeeks), player.promisedRole, risk
        });
    }
    std::vector<std::vector<std::string> > infrastructureRows = {
        {"Cantera", std::to_string(team.youthFacilityLevel), team.youthCoachName, "Coach " + std::to_string(team.youthCoach) + " | impacta intake y potencial"},
        {"Entrenamiento", std::to_string(team.trainingFacilityLevel), team.fitnessCoachName, "Coach " + std::to_string(team.fitnessCoach) + " | progreso y recuperacion"},
        {"Arqueros", std::to_string(team.goalkeepingCoach), team.goalkeepingCoachName, "Sostiene rendimiento del portero"},
        {"Scouting", std::to_string(team.scoutingChief), team.scoutingChiefName, "Mercado, cobertura e informes"},
        {"Analisis", std::to_string(team.performanceAnalyst), team.performanceAnalystName, "Microciclo y preparacion rival"},
        {"Estadio", std::to_string(team.stadiumLevel), std::to_string(team.fanBase), "Impacta recaudacion"}
    };

    std::vector<std::string> infrastructureFeed = {
        "Cantera nivel " + std::to_string(team.youthFacilityLevel) + " con " + team.youthCoachName + " al mando.",
        "Entrenamiento nivel " + std::to_string(team.trainingFacilityLevel) + " | coach " + team.fitnessCoachName + ".",
        "Scouting " + std::to_string(team.scoutingChief) + " | cobertura " +
            (team.scoutingRegions.empty() ? std::string("local") : joinStringValues(team.scoutingRegions, ", ")),
        "Analisis " + std::to_string(team.performanceAnalyst) + " | microciclo y rival."
    };
    const std::vector<std::string> salaryFeed =
        selectFeedLines(alerts, {"contrato", "salida"}, 8, alerts);

    model.detail.content = "Presupuesto " + formatMoneyValue(team.budget) +
                           "\r\nDeuda " + formatMoneyValue(team.debt) +
                           "\r\nSponsor " + formatMoneyValue(finance.sponsorIncome) +
                           "\r\nTaquilla " + formatMoneyValue(finance.matchdayIncome) +
                           "\r\nMerchandising " + formatMoneyValue(finance.merchandisingIncome) +
                           "\r\nBonos variables " + formatMoneyValue(finance.bonusIncome) +
                           "\r\nFair play salarial " + formatMoneyValue(allowedSalary) +
                           "\r\nUso salarial " + std::to_string(salaryPressure) + "%" +
                           "\r\nBuffer de mercado " + formatMoneyValue(finance.transferBuffer) +
                           "\r\nRiesgo " + finance.riskLevel;
    if (!fairPlayViolations.empty()) {
        model.detail.content += "\r\n\r\nAlertas fair play";
        for (const auto& violation : fairPlayViolations) {
            model.detail.content += "\r\n- " + violation.description +
                                    " | severidad " + std::to_string(violation.severityLevel);
        }
    }

    if (state.currentFilter == "Salarios") {
        model.infoLine = "Masa salarial, contratos cortos y riesgo de salida.";
        model.primary.title = "SalaryTable";
        model.primary.columns = {{L"Jugador", 200}, {L"Salario", 110}, {L"Contrato", 70}, {L"Rol", 110}, {L"Riesgo", 160}};
        model.primary.rows = salaryRows;
        model.secondary.title = "FinanceBreakdown";
        model.secondary.columns = {{L"Cuenta", 180}, {L"Valor", 140}, {L"Lectura", 280}};
        model.secondary.rows = financeRows;
        model.footer.title = "Infrastructure";
        model.footer.columns = {{L"Area", 160}, {L"Nivel", 70}, {L"Staff", 80}, {L"Impacto", 260}};
        model.footer.rows = infrastructureRows;
        model.feed.lines = salaryFeed;
        model.detail.content =
            "Masa salarial " + formatMoneyValue(finance.wageBill) +
            "\r\nMaximo fair play " + formatMoneyValue(allowedSalary) +
            "\r\nUso recomendado " + std::to_string(salaryPressure) + "%" +
            "\r\nJugadores monitorizados " + std::to_string(salaryRows.size()) +
            "\r\nContratos cortos " + std::to_string(std::count_if(team.players.begin(),
                                                                     team.players.end(),
                                                                     [](const Player& player) { return player.contractWeeks <= 12; })) +
            "\r\nFiltro salarial activo";
    } else if (state.currentFilter == "Infraestructura") {
        model.infoLine = "Instalaciones, staff y estructura de soporte del club.";
        model.primary.title = "Infrastructure";
        model.primary.columns = {{L"Area", 160}, {L"Nivel", 70}, {L"Staff", 80}, {L"Impacto", 260}};
        model.primary.rows = infrastructureRows;
        model.secondary.title = "FinanceBreakdown";
        model.secondary.columns = {{L"Cuenta", 180}, {L"Valor", 140}, {L"Lectura", 280}};
        model.secondary.rows = financeRows;
        model.footer.title = "SalaryTable";
        model.footer.columns = {{L"Jugador", 200}, {L"Salario", 110}, {L"Contrato", 70}, {L"Rol", 110}, {L"Riesgo", 160}};
        model.footer.rows.assign(salaryRows.begin(),
                                 salaryRows.begin() + static_cast<long long>(std::min<std::size_t>(salaryRows.size(), 8)));
        model.feed.lines = infrastructureFeed;
        model.detail.content =
            "Cantera nivel " + std::to_string(team.youthFacilityLevel) +
            "\r\nEntrenamiento nivel " + std::to_string(team.trainingFacilityLevel) +
            "\r\nScouting " + std::to_string(team.scoutingChief) +
            "\r\nAnalisis " + std::to_string(team.performanceAnalyst) +
            "\r\nFiltro de infraestructura activo";
    } else {
        model.primary.rows = financeRows;
        model.secondary.rows = salaryRows;
        model.footer.rows = infrastructureRows;
        model.feed.lines = alerts;
    }

    if (!actionLines.empty()) {
        model.detail.content += "\r\n\r\nAcciones sugeridas";
        for (const auto& line : actionLines) model.detail.content += "\r\n- " + line;
    }
    if (!storyLines.empty()) {
        model.detail.content += "\r\n\r\nNarrativa";
        for (const auto& line : storyLines) model.detail.content += "\r\n- " + line;
    }
    return model;
}

GuiPageModel buildBoardModel(AppState& state) {
    GuiPageModel model;
    std::vector<std::string> alerts = buildAlertLines(state.career);
    const auto actionLines = manager_advice::buildManagerActionLines(state.career, 4);
    const auto storyLines = manager_advice::buildCareerStorylines(state.career, 3);
    model.title = pageTitleFor(state.currentPage);
    model.breadcrumb = breadcrumbFor(state.currentPage);
    model.metrics = buildMetrics(state, alerts);
    model.infoLine = "Objetivos, confianza y memoria institucional.";
    model.summary.title = "BoardObjectives";
    model.primary.title = "BoardObjectiveTable";
    model.primary.columns = {{L"Objetivo", 220}, {L"Estado", 120}, {L"Contexto", 220}};
    model.secondary.title = "ClubProfile";
    model.secondary.columns = {{L"Perfil", 160}, {L"Valor", 120}, {L"Lectura", 220}};
    model.footer.title = "CoachHistory";
    model.footer.columns = {{L"Temp", 60}, {L"Club", 170}, {L"Puesto", 60}, {L"Campeon", 180}, {L"Nota", 240}};
    model.detail.title = "BoardReport";
    model.feed.title = "AlertPanel / NewsFeedPanel";
    model.feed.lines = alerts;

    const BoardPressureSnapshot boardPressure = buildBoardPressureSnapshot(state.career);
    model.summary.content = buildBoardSummaryService(state.career);
    model.summary.content += "\r\nMandato: " + boardPressure.mandate +
                             "\r\nPresion institucional: " + boardPressure.pressureLabel +
                             " " + std::to_string(boardPressure.pressureScore) + "/100" +
                             "\r\nObjetivo mensual: " + boardPressure.objectiveState +
                             " (" + std::to_string(boardPressure.objectivePercent) + "%)" +
                             "\r\nSiguiente hito: " + boardPressure.nextMilestone;
    model.detail.content = buildBoardSummaryService(state.career);
    if (!actionLines.empty()) {
        model.detail.content += "\r\n\r\nAcciones sugeridas";
        for (const auto& line : actionLines) model.detail.content += "\r\n- " + line;
    }
    if (!storyLines.empty()) {
        model.detail.content += "\r\n\r\nNarrativa de la semana";
        for (const auto& line : storyLines) model.detail.content += "\r\n- " + line;
    }

    std::vector<std::vector<std::string> > objectiveRows;
    std::vector<std::vector<std::string> > profileRows;
    std::vector<std::vector<std::string> > historyRows;

    if (state.career.myTeam) {
        objectiveRows.push_back({"Puesto esperado", std::to_string(state.career.boardExpectedFinish), "Meta de temporada"});
        objectiveRows.push_back({"Objetivo mensual", state.career.boardMonthlyObjective.empty() ? "Sin objetivo" : state.career.boardMonthlyObjective,
                                 std::to_string(state.career.boardMonthlyProgress) + "/" + std::to_string(state.career.boardMonthlyTarget)});
        objectiveRows.push_back({"Objetivo sugerido", manager_advice::buildSuggestedBoardObjective(state.career), "Propuesta coherente con club"});
        objectiveRows.push_back({"Razon objetivo", manager_advice::buildSuggestedBoardObjectiveReason(state.career), "Por que se recomienda"});
        objectiveRows.push_back({"Estado objetivo", boardPressure.objectiveState,
                                 std::to_string(boardPressure.objectivePercent) + "% | " + boardPressure.nextMilestone});
        objectiveRows.push_back({"Confianza", std::to_string(state.career.boardConfidence) + "/100", boardStatusLabel(state.career.boardConfidence)});
        objectiveRows.push_back({"Presion directiva", boardPressure.pressureLabel + " " + std::to_string(boardPressure.pressureScore) + "/100",
                                 boardPressure.riskReason});
        objectiveRows.push_back({"Mandato", boardPressure.mandate, boardPressure.recommendation});
        objectiveRows.push_back({"Advertencias", std::to_string(state.career.boardWarningWeeks), "Semanas bajo revision"});

        Team& team = *state.career.myTeam;
        profileRows.push_back({"Expectativa", teamExpectationLabel(team), "Perfil de club"});
        profileRows.push_back({"Prestigio", std::to_string(team.clubPrestige), "Influye en fichajes"});
        profileRows.push_back({"DT del club", team.headCoachName, team.headCoachStyle});
        profileRows.push_back({"Antiguedad DT", std::to_string(team.headCoachTenureWeeks) + " sem", "Tiempo del proyecto actual"});
        profileRows.push_back({"Seguridad", std::to_string(team.jobSecurity), "Estabilidad del banquillo"});
        profileRows.push_back({"Riesgo cargo", std::to_string(boardPressure.jobRiskScore) + "/100",
                               boardPressure.jobRiskScore >= 60 ? "El banquillo necesita resultados" : "Margen de trabajo razonable"});
        profileRows.push_back({"Riesgo caja", std::to_string(boardPressure.budgetRiskScore) + "/100",
                               boardPressure.budgetRiskScore > 0 ? "La directiva vigila la caja" : "Sin presion financiera inmediata"});
        profileRows.push_back({"Politica", team.transferPolicy, "Mercado del club"});
        profileRows.push_back({"Perfil competitivo", teamPersonalityHeadline(team), "Como se refleja la identidad en decisiones CPU"});
        profileRows.push_back({"Red scouting", team.scoutingRegions.empty() ? std::string("-") : joinStringValues(team.scoutingRegions, ", "), "Cobertura activa"});
        profileRows.push_back({"Identidad cantera", team.youthIdentity, "Condiciona objetivos"});
        profileRows.push_back({"Estilo", team.clubStyle, "Contexto institucional"});
        profileRows.push_back(std::vector<std::string>{"Coherencia filos", std::to_string(clubPhilosophyAlignmentScore(state.career, team)) + "/100", "Alineacion estrategia, cantera y objetivo"});
    }
    for (auto it = state.career.history.rbegin(); it != state.career.history.rend() && historyRows.size() < 8; ++it) {
        historyRows.push_back({
            std::to_string(it->season), it->club, std::to_string(it->finish), it->champion, it->note
        });
    }

    const std::vector<std::string> objectiveFeed =
        selectFeedLines(alerts, {"directiva", "objetivo", "promesa"}, 8, alerts);
    std::vector<std::string> historyFeed;
    for (auto it = state.career.history.rbegin(); it != state.career.history.rend() && historyFeed.size() < 8; ++it) {
        historyFeed.push_back("T" + std::to_string(it->season) + " | " + it->club + " | puesto " +
                              std::to_string(it->finish) + " | campeon " + it->champion);
    }
    if (historyFeed.empty()) historyFeed = alerts;

    if (state.currentFilter == "Objetivos") {
        model.infoLine = "Seguimiento fino de metas, confianza y riesgo institucional.";
        model.primary.rows = objectiveRows;
        model.secondary.rows = profileRows;
        model.footer.rows = historyRows;
        model.feed.lines = objectiveFeed;
        model.detail.content = buildBoardSummaryService(state.career) +
                               "\r\n\r\nPanel de presion institucional" +
                               "\r\n- Mandato: " + boardPressure.mandate +
                               "\r\n- Presion: " + boardPressure.pressureLabel + " " +
                               std::to_string(boardPressure.pressureScore) + "/100" +
                               "\r\n- Objetivo: " + boardPressure.objectiveState + " (" +
                               std::to_string(boardPressure.objectivePercent) + "%)" +
                               "\r\n- Motivo: " + boardPressure.riskReason +
                               "\r\n- Recomendacion: " + boardPressure.recommendation +
                               "\r\n\r\nVista priorizada: objetivos y riesgo de directiva.";
    } else if (state.currentFilter == "Historial") {
        model.infoLine = "Memoria del proyecto: temporadas previas y contexto institucional.";
        model.primary.title = "CoachHistory";
        model.primary.columns = {{L"Temp", 60}, {L"Club", 170}, {L"Puesto", 60}, {L"Campeon", 180}, {L"Nota", 240}};
        model.primary.rows = historyRows;
        model.secondary.title = "BoardObjectiveTable";
        model.secondary.columns = {{L"Objetivo", 220}, {L"Estado", 120}, {L"Contexto", 220}};
        model.secondary.rows = objectiveRows;
        model.footer.title = "ClubProfile";
        model.footer.columns = {{L"Perfil", 160}, {L"Valor", 120}, {L"Lectura", 220}};
        model.footer.rows = profileRows;
        model.feed.lines = historyFeed;
        model.detail.content = buildBoardSummaryService(state.career) + "\r\n\r\nVista historica priorizada.";
    } else {
        model.primary.rows = objectiveRows;
        model.secondary.rows = profileRows;
        model.footer.rows = historyRows;
    }

    if (state.currentFilter != "Objetivos") {
        model.detail.content += "\r\n\r\nPanel de presion institucional" +
                                std::string("\r\n- Mandato: ") + boardPressure.mandate +
                                "\r\n- Presion: " + boardPressure.pressureLabel + " " +
                                std::to_string(boardPressure.pressureScore) + "/100" +
                                "\r\n- Objetivo: " + boardPressure.objectiveState + " (" +
                                std::to_string(boardPressure.objectivePercent) + "%)" +
                                "\r\n- Motivo: " + boardPressure.riskReason +
                                "\r\n- Recomendacion: " + boardPressure.recommendation;
    }
    model.summary.content += "\r\nFiltro actual: " + state.currentFilter;
    return model;
}

GuiPageModel buildNewsModel(AppState& state) {
    GuiPageModel model;
    std::vector<std::string> alerts = buildAlertLines(state.career);
    const auto storyLines = manager_advice::buildCareerStorylines(state.career, 4);
    const auto hubLines = inbox_service::buildPriorityInboxLines(state.career, 12);
    const auto weeklyDecisionOptions = buildWeeklyDecisionOptions(state.career);
    model.title = pageTitleFor(state.currentPage);
    model.breadcrumb = breadcrumbFor(state.currentPage);
    model.metrics = buildMetrics(state, alerts);
    model.infoLine = "Centro del manager: inbox, scouting, rumores y alertas para seguir el pulso del mundo.";
    model.summary.title = "ScoutingInbox";
    model.primary.title = "NewsCardList";
    model.primary.columns = {{L"Tipo", 120}, {L"Titular", 620}};
    model.secondary.title = "ActionableInbox";
    model.secondary.columns = {{L"Tipo", 90}, {L"Prioridad", 82}, {L"Destino", 95}, {L"Accion", 126}, {L"Detalle", 360}};
    model.footer.title = "AlertPanel";
    model.footer.columns = {{L"Nivel", 120}, {L"Detalle", 560}};
    model.detail.title = "NewsDetail";
    model.feed.title = "NewsFeedPanel";
    model.feed.lines = buildFeedLines(state.career, state.currentFilter == "Todo" ? "" : state.currentFilter, 24);

    for (const auto& line : model.feed.lines) {
        std::string type = "Info";
        std::string lower = toLower(line);
        if (lower.find("lesion") != std::string::npos) type = "Lesiones";
        else if (lower.find("fich") != std::string::npos || lower.find("contrat") != std::string::npos) type = "Mercado";
        else if (lower.find("copa") != std::string::npos || lower.find("campeon") != std::string::npos) type = "Resultados";
        else if (lower.find("directiva") != std::string::npos || lower.find("objetivo") != std::string::npos) type = "Directiva";
        model.primary.rows.push_back({type, line});
    }

    for (const auto& entry : inbox_service::buildActionableInbox(state.career, 12)) {
        model.secondary.rows.push_back({
            entry.channel,
            entry.priority,
            entry.destination,
            entry.command,
            entry.text
        });
    }
    if (model.secondary.rows.empty()) model.secondary.rows.push_back({"Inbox", "-", "-", "Revisar", "Sin novedades recientes"});

    for (const auto& alert : alerts) {
        model.footer.rows.push_back({"Alerta", alert});
    }
    for (size_t i = 0; i < weeklyDecisionOptions.size() && i < 4; ++i) {
        model.footer.rows.push_back({"Decision", weeklyDecisionOptions[i]});
    }
    model.summary.content = "Entradas visibles: " + std::to_string(model.feed.lines.size()) +
                            "\r\nInbox manager: " + std::to_string(state.career.managerInbox.size()) +
                            "\r\nScouting: " + std::to_string(state.career.scoutInbox.size()) +
                            "\r\nAsignaciones: " + std::to_string(state.career.scoutingAssignments.size()) +
                            "\r\nFiltro actual: " + state.currentFilter;
    if (!weeklyDecisionOptions.empty()) {
        model.summary.content += "\r\nDecision semanal: " + weeklyDecisionOptions.front();
        model.summary.content += "\r\nBoton Decidir: aplica la recomendacion del staff.";
    }
    if (!hubLines.empty()) {
        model.summary.content += "\r\nPrioridad: " + hubLines.front();
    }
    if (!storyLines.empty()) {
        model.summary.content += "\r\nNarrativa activa: " + storyLines.front();
    }
    model.detail.content = inbox_service::buildManagerHubDigest(state.career, 8) + "\r\n" +
                           lastMatchPanelText(state.career, 5, 8) + "\r\n" + dressingRoomPanelText(state.career, 4);
    if (!weeklyDecisionOptions.empty()) {
        model.detail.content += "\r\n\r\nTablero de decisiones";
        model.detail.content += "\r\nBoton Decidir aplica la primera opcion viable y consume el digest semanal.";
        model.detail.content += "\r\n\r\nCentro semanal";
        for (size_t i = 0; i < weeklyDecisionOptions.size() && i < 6; ++i) {
            model.detail.content += "\r\n- " + weeklyDecisionOptions[i];
        }
    }
    if (!storyLines.empty()) {
        model.detail.content += "\r\n\r\nNarrativa de la semana";
        for (const auto& line : storyLines) model.detail.content += "\r\n- " + line;
    }
    return model;
}

}  // namespace gui_win32

#endif

