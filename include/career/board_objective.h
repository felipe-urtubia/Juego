#pragma once

#include <algorithm>
#include <cctype>
#include <string>

enum class BoardObjectiveType {
    Unknown,
    Points,
    YouthStarts,
    Position,
    Budget,
    Recovery,
    RelegationRisk
};

inline BoardObjectiveType boardObjectiveTypeFromText(
    const std::string& objective) {

    if (objective.empty()) return BoardObjectiveType::Unknown;

    std::string text = objective;
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                   });

    const auto contains = [&](const char* term) {
        return text.find(term) != std::string::npos;
    };

    if (contains("descender") ||
        contains("zona de riesgo de descenso")) {
        return BoardObjectiveType::RelegationRisk;
    }

    if (contains("rotar jugadores") ||
        contains("cuidar cargas")) {
        return BoardObjectiveType::Recovery;
    }

    if (contains("titularidades")) {
        return BoardObjectiveType::YouthStarts;
    }

    if (contains("presupuesto")) {
        return BoardObjectiveType::Budget;
    }

    if (contains("posicion") ||
        contains("puesto") ||
        contains("top") ||
        contains("entrar")) {
        return BoardObjectiveType::Position;
    }

    if (contains("puntos")) {
        return BoardObjectiveType::Points;
    }

    return BoardObjectiveType::Unknown;
}

inline const char* boardObjectiveStableId(BoardObjectiveType type) {
    switch (type) {
        case BoardObjectiveType::Points:
            return "points";

        case BoardObjectiveType::YouthStarts:
            return "youth_starts";

        case BoardObjectiveType::Position:
            return "position";

        case BoardObjectiveType::Budget:
            return "budget";

        case BoardObjectiveType::Recovery:
            return "recovery";

        case BoardObjectiveType::RelegationRisk:
            return "relegation_risk";

        case BoardObjectiveType::Unknown:
            return "unknown";
    }

    return "unknown";
}

inline BoardObjectiveType boardObjectiveTypeFromStableId(
    const std::string& id) {

    if (id == "points") return BoardObjectiveType::Points;
    if (id == "youth_starts") return BoardObjectiveType::YouthStarts;
    if (id == "position") return BoardObjectiveType::Position;
    if (id == "budget") return BoardObjectiveType::Budget;
    if (id == "recovery") return BoardObjectiveType::Recovery;
    if (id == "relegation_risk") return BoardObjectiveType::RelegationRisk;

    return BoardObjectiveType::Unknown;
}

inline bool boardObjectiveIsRanking(BoardObjectiveType type) {
    return type == BoardObjectiveType::Position ||
           type == BoardObjectiveType::RelegationRisk;
}

inline bool boardObjectiveUnderPressure(
    BoardObjectiveType type, int progress, int target) {

    if (target <= 0) return false;

    if (boardObjectiveIsRanking(type)) {
        return progress > 0 && progress > target;
    }

    return progress < target;
}
