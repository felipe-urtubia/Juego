# App Services Scouting - Diseño

**Fecha:** 2026-09-14
**Estado:** Aprobado para implementación

## Objetivo

Reducir las responsabilidades de `src/career/app_services.cpp` separando la lógica de Scouting en un módulo dedicado:

`src/career/app_services_scouting.cpp`

El cambio será exclusivamente arquitectónico. No se modificarán las reglas del juego, la API pública, la interfaz gráfica, los guardados ni el comportamiento observable del sistema.

## Situación actual

`app_services.cpp` continúa actuando como una capa de orquestación con múltiples responsabilidades.

La primera fase ya separó los wrappers de reportes hacia:

`src/career/app_services_reports.cpp`

La segunda fase continuará esta modularización extrayendo únicamente la responsabilidad de Scouting.

## Arquitectura propuesta

La organización resultante será:

```text
src/career/
├── app_services.cpp
├── app_services_reports.cpp
└── app_services_scouting.cpp
```

### `app_services.cpp`

Continuará conteniendo las responsabilidades generales todavía no separadas, entre ellas:

* orquestación general;
* transferencias;
* gestión semanal;
* gestión del club;
* juveniles;
* interacciones del manager.

### `app_services_reports.cpp`

Mantiene los wrappers de reportes separados en la fase anterior.

### `app_services_scouting.cpp`

Contendrá los servicios y helpers privados relacionados exclusivamente con Scouting.

## Funciones que se moverán

### Helpers privados de Scouting

Se moverán desde `app_services.cpp`:

* `resolveAssignmentRegion`
* `assignmentPriorityLabel`
* `hasScoutingCoverage`
* `scoutingCoverageLabel`
* `availabilityLabel`
* `agentProfileLabel`
* `scoutingReportStage`
* `scoutingHiddenRiskLabel`
* `scoutingAssignmentBoost`
* `appendScoutInbox`

Estos helpers se consideran parte del flujo interno necesario para construir y mantener informes de scouting, asignaciones y shortlist.

### Servicios públicos de Scouting

Se moverán:

* `runScoutingSessionService`
* `scoutPlayersService`
* `createScoutingAssignmentService`
* `shortlistPlayerService`
* `followShortlistService`
* `listYouthRegionsService`

Las firmas públicas existentes se mantendrán sin cambios.

## Funciones fuera de alcance

No se moverán en esta fase:

* `changeYouthRegionService`
* lógica de mejoras del club;
* lógica de contratación de staff;
* decisiones semanales;
* transferencias;
* negociación de jugadores;
* procesamiento general del inbox del manager.

`changeYouthRegionService` permanecerá fuera del módulo de Scouting porque pertenece a la gestión de cantera y captación juvenil del club.

## API pública

`include/career/app_services.h` seguirá actuando como fachada pública.

No se cambiarán las firmas de los servicios existentes.

Los consumidores actuales de la API no deberán modificar sus llamadas.

## Dependencias compartidas

El nuevo módulo reutilizará funciones ya existentes en otros módulos y no duplicará su implementación.

Entre ellas:

### `engine/models`

* `positionFitScore`
* `playerReliabilityLabel`
* `playerFormLabel`
* `ensureTeamIdentity`
* `joinStringValues`

### `career/career_reports`

* `formatMoneyValue`
* `detectScoutingNeed`

### `transfers/negotiation_system`

* `personalityLabel`
* `wageDemandFor`

### `utils`

* `normalizePosition`

También seguirá utilizando los servicios existentes de:

* `player_condition`
* `world_state_service`

No se crearán nuevos helpers genéricos en esta fase.

## CMake

`src/career/app_services_scouting.cpp` deberá agregarse a:

`FM_CAREER_SOURCES`

en `CMakeLists.txt`.

No se eliminará `app_services.cpp` del target.

## Estrategia de pruebas

La implementación seguirá TDD.

### RED

Antes de crear el nuevo módulo se agregará una prueba estructural que exija la existencia de:

`src/career/app_services_scouting.cpp`

La prueba deberá fallar antes de implementar la separación.

### GREEN

Después de crear el nuevo módulo y mover las funciones:

* `FootballManagerTests` deberá compilar;
* la suite completa deberá pasar;
* `FootballManager` deberá compilar;
* `FootballManagerCLI` deberá compilar.

También se ejecutará:

```text
FootballManagerCLI.exe --validate
```

y se espera:

* 0 errores;
* 0 advertencias;
* resultado sin fallas.

## Compatibilidad

La separación no deberá modificar:

* nombres públicos;
* parámetros;
* tipos de retorno;
* mensajes del juego;
* costos de scouting;
* cálculos de confianza;
* lógica de shortlist;
* asignaciones;
* cobertura regional;
* presupuesto;
* noticias;
* inbox;
* estructura de guardado.

El objetivo es mover implementación, no modificar comportamiento.

## Verificación

Antes del commit se revisará:

```text
git diff --check
```

También se inspeccionará el diff para comprobar que:

* las funciones fueron movidas sin cambios funcionales;
* `app_services.cpp` dejó de contenerlas;
* CMake incluye el nuevo archivo;
* las pruebas solo contienen la cobertura necesaria para esta separación;
* no aparecieron archivos ajenos al cambio.

## Evidencia

Al finalizar la implementación se registrará en `TODO.md`:

* objetivo del refactor;
* funciones separadas;
* evidencia RED;
* evidencia GREEN;
* compilación de GUI, CLI y tests;
* resultado de `--validate`;
* revisión de Git;
* commit utilizado.

## Resultado esperado

Al terminar esta fase:

`app_services.cpp` tendrá menos responsabilidades y la funcionalidad de Scouting contará con un módulo de implementación independiente.

El comportamiento externo del juego permanecerá sin cambios.
