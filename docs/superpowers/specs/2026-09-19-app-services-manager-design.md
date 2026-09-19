# Diseño: separación de interacciones del manager desde app_services

Fecha: 2026-09-19

## Objetivo

Reducir responsabilidades de `src/career/app_services.cpp` separando en un módulo independiente los servicios relacionados con interacción del manager, desarrollo individual, vestuario y ajustes directos de entrenamiento/instrucción, sin cambiar la API pública ni el comportamiento del juego.

## Nuevo módulo

`src/career/app_services_manager.cpp`

## Servicios públicos a mover

- `cyclePlayerDevelopmentPlanService`
- `cyclePlayerInstructionService`
- `holdTeamMeetingService`
- `talkToPlayerService`
- `cycleTrainingFocusService`
- `cycleMatchInstructionService`

Las declaraciones públicas continúan en:

`include/career/app_services.h`

## Helpers privados a mover

- `nextDevelopmentPlan`
- `nextInstructionForPlayer`
- `nextTrainingFocus`
- `nextMatchInstruction`

Estos helpers solo sirven al bloque de servicios incluido en este refactor.

## Dependencias compartidas que NO se moverán

El nuevo módulo seguirá consumiendo las implementaciones públicas existentes de:

- `promiseAtRisk(...)`
- `playerHasTrait(...)`
- `defaultDutyForPosition(...)`
- `normalizePosition(...)`
- `ensureTeamIdentity(...)`
- `clampInt(...)`

No se duplicarán estas funciones porque ya pertenecen a otros módulos compartidos del proyecto.

## Helper genérico

`failure(...)` permanecerá disponible en `app_services.cpp`.

El nuevo módulo podrá mantener una copia privada mínima de `failure(...)`, siguiendo el patrón usado en los módulos ya separados.

## Fuera de alcance

No se moverán en esta etapa:

- `applyWeeklyDecisionService`
- `applyMatchPreparationPlanService`
- `resolveInboxDecisionService`
- `changeYouthRegionService`
- `takeManagerJobService`
- persistencia
- scouting
- transferencias
- reportes
- club/staff

`applyWeeklyDecisionService(...)` podrá seguir llamando a `holdTeamMeetingService(...)`.

`resolveInboxDecisionService(...)` podrá seguir llamando a los servicios públicos movidos.

## Restricciones

El refactor no debe modificar:

- mensajes visibles
- cálculos de moral
- felicidad
- química
- momentum
- reglas de promesas
- lógica de roles
- planes de desarrollo
- instrucciones individuales
- entrenamiento semanal
- instrucciones de partido
- noticias generadas
- API pública

## Integración de build

Se añadirá:

`src/career/app_services_manager.cpp`

a `FM_CAREER_SOURCES` en `CMakeLists.txt`.

## Estrategia TDD

Se agregará una prueba estructural:

`app_services_manager_split`

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

`app_services.cpp` conserva las llamadas legítimas desde decisiones semanales e inbox, mientras las implementaciones de interacción directa del manager pasan a un módulo independiente sin cambios funcionales.
