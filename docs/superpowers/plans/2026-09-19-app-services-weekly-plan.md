# Plan: separación weekly de app_services

Fecha: 2026-09-19

## Objetivo

Extraer la lógica del centro semanal desde `src/career/app_services.cpp` hacia:

`src/career/app_services_weekly.cpp`

sin modificar comportamiento, mensajes ni API pública.

## Paso 1 - Prueba estructural RED

Agregar una prueba estructural llamada:

`app_services_weekly_split`

La prueba debe comprobar que los siguientes servicios ya no estén definidos en `app_services.cpp` y vivan en `app_services_weekly.cpp`:

- `consumeLatestWeeklyDigestService`
- `applyWeeklyDecisionService`
- `applyMatchPreparationPlanService`
- `buildWeeklyDecisionOptions`
- `resolveInboxDecisionService`

Ejecutar los tests y confirmar que la nueva prueba falla antes de realizar la extracción.

## Paso 2 - Crear nuevo módulo

Crear:

`src/career/app_services_weekly.cpp`

Agregar los includes mínimos necesarios y mantener:

`#include "career/app_services.h"`

como interfaz pública principal.

## Paso 3 - Mover helpers privados exclusivos

Mover al namespace privado del nuevo módulo:

- `consumeMatchingInboxEntry`
- `latestWeeklyDigestInboxEntry`
- `weeklyDecisionLabel`
- `countFatiguedPlayers`
- `countLowMoralePlayers`
- `countYouthCandidates`
- `decisionFromLastMatchCenter`
- `chooseAutomaticWeeklyDecision`

Eliminar sus definiciones originales de `app_services.cpp`.

## Paso 4 - Helpers compartidos

Mantener en `app_services.cpp`:

- `syncInfrastructureFromTeam`
- `syncTeamFromInfrastructure`

porque son usados por inicio y carga de carrera.

Si el módulo weekly necesita estos helpers, añadir copias privadas idénticas en `app_services_weekly.cpp`.

No convertirlos en API pública durante este refactor.

## Paso 5 - Mover servicios públicos

Mover sin alterar su implementación:

- `consumeLatestWeeklyDigestService`
- `applyWeeklyDecisionService`
- `applyMatchPreparationPlanService`
- `buildWeeklyDecisionOptions`
- `resolveInboxDecisionService`

Las declaraciones permanecen en:

`include/career/app_services.h`

## Paso 6 - Mantener cierre de simulación en app_services

No mover:

- `recommendedWeeklyDecisionSummary`
- `appendPostWeekActionDigest`

Estos seguirán llamando a:

`buildWeeklyDecisionOptions(...)`

a través de la API pública existente.

## Paso 7 - CMake

Agregar:

`src/career/app_services_weekly.cpp`

a `FM_CAREER_SOURCES` en `CMakeLists.txt`.

## Paso 8 - GREEN

Compilar:

`FootballManagerTests`

y ejecutar:

`ctest --test-dir .\build-ci --output-on-failure`

La prueba:

`app_services_weekly_split`

debe pasar junto con el resto.

## Paso 9 - Verificación completa

Compilar:

- `FootballManager`
- `FootballManagerCLI`

Ejecutar:

`.\build-ci\bin\FootballManagerCLI.exe --validate`

Resultado esperado:

- 5 divisiones
- 90 equipos
- 2200 jugadores
- 0 errores
- 0 warnings
- `Resultado: sin fallas`

## Paso 10 - Verificación estructural

Buscar las definiciones de los servicios y helpers movidos para confirmar que no permanezcan duplicados en `app_services.cpp`.

Los helpers compartidos de infraestructura sí pueden existir tanto en `app_services.cpp` como como copias privadas en módulos especializados.

## Paso 11 - Calidad de diff

Ejecutar:

`git diff --check`

Las advertencias de conversión LF/CRLF en Windows no representan fallos del código.

## Paso 12 - Commit

Crear un commit de implementación con un mensaje equivalente a:

`refactor: separar weekly de app services`

## Resultado final

`app_services.cpp` conservará inicio, carga, guardado y simulación general de carrera.

`app_services_weekly.cpp` concentrará el centro semanal, decisiones automáticas, preparación de partido y resolución accionable del inbox.
