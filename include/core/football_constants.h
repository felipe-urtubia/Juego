#pragma once

#include <string_view>

namespace Football::Tactics
{
    constexpr std::string_view Balanced = "Balanced";
    constexpr std::string_view Offensive = "Offensive";
    constexpr std::string_view Defensive = "Defensive";
    constexpr std::string_view Counter = "Counter";
    constexpr std::string_view Pressing = "Pressing";
}

namespace Football::Instruction
{
    constexpr std::string_view Balanced = "Equilibrado";
    constexpr std::string_view Wings = "Por bandas";
    constexpr std::string_view Direct = "Juego directo";
    constexpr std::string_view LowBlock = "Bloque bajo";
    constexpr std::string_view CounterPress = "Contra-presion";
    constexpr std::string_view SlowGame = "Pausar juego";
    constexpr std::string_view FinalPress = "Presion final";
}

namespace Football::Marking
{
    constexpr std::string_view Man = "Hombre";
    constexpr std::string_view Zonal = "Zonal";
}

namespace Football::Rotation
{
    constexpr std::string_view Starters = "Titulares";
    constexpr std::string_view Rotation = "Rotacion";
    constexpr std::string_view Balanced = "Balanceado";
}