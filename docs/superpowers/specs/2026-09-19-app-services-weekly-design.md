# Diseño: separación del centro semanal desde app_services

Fecha: 2026-09-19

## Objetivo

Reducir responsabilidades de `src/career/app_services.cpp` separando la lógica del centro semanal, decisiones del manager, preparación de partido e inbox accionable en un módulo independiente, sin cambiar la API pública ni el comportamiento del juego.

## Nuevo módulo

`src/career/app_services_weekly.cpp`

## Servicios públicos a mover

- `consumeLatestWeeklyDigestService`
- `applyWeeklyDecisionService`
- `applyMatchPreparationPlanService`
- `buildWeeklyDecisionOptions`
- `resolveInboxDecisionService`

Las declaraciones públicas continúan en:

`include/career/app_services.h`

## Helpers privados a mover

- `consumeMatchingInboxEntry`
- `latestWeeklyDigestInboxEntry`
- `weeklyDecisionLabel`
- `countFatiguedPlayers`
- `countLowMoralePlayers`
- `countYouthCandidates`
- `decisionFromLastMatchCenter`
- `chooseAutomaticWeeklyDecision`

## Helpers compartidos

Los siguientes helpers permanecen en `app_services.cpp` porque también son usados por inicio/carga de carrera:

- `syncInfrastructureFromTeam`
- `syncTeamFromInfrastructure`

El nuevo módulo podrá mantener copias privadas idénticas si las necesita para preservar la separación sin cambiar comportamiento.

## Helpers que permanecen en app_services.cpp

- `recommendedWeeklyDecisionSummary`
- `appendPostWeekActionDigest`

Estos forman parte del cierre de simulación semanal y seguirán consumiendo el servicio público `buildWeeklyDecisionOptions(...)`.

## Fuera de alcance

No se moverán en esta etapa:

- `startCareerService`
- `loadCareerService`
- `saveCareerService`
- `simulateCareerWeekService`
- `changeYouthRegionService`
- `takeManagerJobService`
- persistencia
- club/staff
- scouting
- transferencias
- reportes
- interacciones directas del manager ya separadas

## Restricciones

El refactor no debe modificar:

- decisiones automáticas
- umbrales de fatiga
- umbrales de moral
- criterios juveniles
- reglas financieras
- estrés del manager
- preparación de partido
- mensajes visibles
- inbox
- noticias
- API pública

## Integración de build

Se añadirá:

`src/career/app_services_weekly.cpp`

a `FM_CAREER_SOURCES` en `CMakeLists.txt`.

## Estrategia TDD

Se agregará una prueba estructural:

`app_services_weekly_split`

La prueba deberá fallar inicialmente mientras el nuevo módulo no exista y pasar después de integrarlo.

## Verificación final

Al completar el refactor se ejecutarán:

- build de `FootballManagerTests`
- `ctest --test-dir .\build-ci --output-on-failure`
- build de `FootballManager`
- build de `FootballManagerCLI`
- `.\build-ci\bin\FootballManagerCLI.exe --validate`
- verificación estructural de definiciones
- `git diff --check`

## Resultado esperado

`app_services.cpp` conserva el flujo de inicio/carga/simulación y los helpers compartidos necesarios, mientras el centro semanal y sus decisiones pasan a un módulo independiente sin cambios funcionales.
