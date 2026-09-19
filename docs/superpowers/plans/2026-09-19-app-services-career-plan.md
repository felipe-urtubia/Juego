# Plan: separación career de app_services

Fecha: 2026-09-19

## Objetivo

Extraer el ciclo principal de carrera desde `src/career/app_services.cpp` hacia:

`src/career/app_services_career.cpp`

sin modificar comportamiento, mensajes ni API pública.

## Paso 1 - Prueba estructural RED

Agregar una prueba estructural llamada:

`app_services_career_split`

La prueba debe comprobar que el nuevo módulo:

`src/career/app_services_career.cpp`

existe.

Antes de crear el nuevo módulo, ejecutar los tests y confirmar que la nueva prueba falla.

## Paso 2 - Crear nuevo módulo

Crear:

`src/career/app_services_career.cpp`

Mantener:

`#include "career/app_services.h"`

como interfaz pública principal y agregar únicamente los includes necesarios para conservar la implementación actual.

## Paso 3 - Mover helpers privados del ciclo de carrera

Mover al namespace privado del nuevo módulo:

- `autoOfferDecision`
- `autoRenewDecision`
- `autoManagerJobDecision`
- `toServiceResult`
- `syncInfrastructureFromTeam`
- `syncTeamFromInfrastructure`
- `recommendedWeeklyDecisionSummary`
- `appendPostWeekActionDigest`

Eliminar sus definiciones originales de `app_services.cpp`.

Las copias privadas equivalentes que ya existen en otros módulos permanecen sin cambios.

## Paso 4 - Mover servicios públicos

Mover sin alterar su implementación:

- `startCareerService`
- `loadCareerService`
- `saveCareerService`
- `simulateSeasonStepService`
- `simulateCareerWeekService`

Las declaraciones públicas permanecen en:

`include/career/app_services.h`

## Paso 5 - Mantener servicios restantes en app_services

No mover en esta etapa:

- `changeYouthRegionService`
- `takeManagerJobService`

Mantener únicamente los helpers privados que estos servicios necesiten.

## Paso 6 - Dependencias entre módulos

`app_services_career.cpp` continuará utilizando las APIs públicas existentes de otros módulos cuando corresponda.

En particular, el cierre semanal podrá seguir utilizando:

`buildWeeklyDecisionOptions(...)`

No duplicar servicios públicos de weekly, club, scouting, transferencias, reportes ni manager.

## Paso 7 - CMake

Agregar:

`src/career/app_services_career.cpp`

a `FM_CAREER_SOURCES` en `CMakeLists.txt`.

## Paso 8 - GREEN

Compilar:

`FootballManagerTests`

y ejecutar:

`ctest --test-dir .\build-ci --output-on-failure`

La prueba:

`app_services_career_split`

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

Buscar las definiciones de los servicios y helpers movidos para confirmar que ya no permanezcan en `app_services.cpp`.

Verificar especialmente:

- `startCareerService`
- `loadCareerService`
- `saveCareerService`
- `simulateSeasonStepService`
- `simulateCareerWeekService`

También comprobar qué funciones quedan finalmente en `app_services.cpp`.

## Paso 11 - Calidad de diff

Ejecutar:

`git diff --check`

No deben existir errores de whitespace ni modificaciones accidentales de codificación.

## Paso 12 - Commit

Crear un commit de implementación con un mensaje equivalente a:

`refactor: separar career de app services`

## Resultado final

`app_services_career.cpp` concentrará el inicio, carga, guardado y avance temporal de la carrera.

`app_services.cpp` quedará reducido principalmente a servicios restantes que todavía no pertenecen a un módulo especializado, acercando el cierre de esta fase de refactor arquitectónico.