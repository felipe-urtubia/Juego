# Diseño: separación del ciclo de carrera desde app_services

Fecha: 2026-09-19

## Objetivo

Reducir las responsabilidades restantes de `src/career/app_services.cpp` separando el inicio, carga, guardado y avance temporal de la carrera en un módulo independiente, sin cambiar la API pública ni el comportamiento del juego.

## Nuevo módulo

`src/career/app_services_career.cpp`

## Servicios públicos a mover

- `startCareerService`
- `loadCareerService`
- `saveCareerService`
- `simulateSeasonStepService`
- `simulateCareerWeekService`

Las declaraciones públicas continúan en:

`include/career/app_services.h`

## Helpers privados a mover

- `autoOfferDecision`
- `autoRenewDecision`
- `autoManagerJobDecision`
- `toServiceResult`
- `syncInfrastructureFromTeam`
- `syncTeamFromInfrastructure`
- `recommendedWeeklyDecisionSummary`
- `appendPostWeekActionDigest`

Estos helpers forman parte del inicio, restauración o avance de la carrera y no necesitan permanecer en el orquestador principal.

## Dependencias con otros módulos

`app_services_career.cpp` seguirá utilizando servicios públicos ya separados cuando corresponda, incluyendo:

- `buildWeeklyDecisionOptions(...)`

No se duplicará lógica pública de weekly, club, scouting, transferencias, reportes ni manager.

Las copias privadas de sincronización que ya existen en:

- `app_services_club.cpp`
- `app_services_weekly.cpp`

permanecen sin cambios.

## Servicios que permanecen en app_services.cpp

En esta etapa permanecen:

- `changeYouthRegionService`
- `takeManagerJobService`

También permanecerán únicamente los helpers privados que estos servicios necesiten.

## Fuera de alcance

No se modificará en esta etapa:

- comportamiento de simulación
- persistencia
- reglas de inicio de carrera
- decisiones automáticas de ofertas
- decisiones automáticas de renovación
- decisiones automáticas de empleo del manager
- lógica del centro semanal
- scouting
- transferencias
- club/staff
- reportes
- interacciones directas del manager
- API pública

## Restricciones

El refactor no debe modificar:

- resultados de simulación
- calendario
- economía
- plantillas
- infraestructura
- contratos
- decisiones automáticas
- mensajes visibles
- inbox
- noticias
- guardado o carga de partidas

## Integración de build

Se añadirá:

`src/career/app_services_career.cpp`

a `FM_CAREER_SOURCES` en `CMakeLists.txt`.

## Estrategia TDD

Se agregará una prueba estructural:

`app_services_career_split`

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

`app_services.cpp` deja de encargarse del ciclo principal de inicio, carga, guardado y avance semanal de la carrera. Esa responsabilidad pasa a `app_services_career.cpp`, manteniendo el mismo comportamiento y la misma API pública.