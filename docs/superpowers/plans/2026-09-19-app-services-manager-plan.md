# Plan de implementación: separación de interacciones del manager desde app_services

Fecha: 2026-09-19

## Objetivo

Mover las interacciones directas del manager desde `src/career/app_services.cpp` hacia:

`src/career/app_services_manager.cpp`

sin modificar la API pública ni el comportamiento del juego.

## Alcance

### Servicios públicos a mover

- `cyclePlayerDevelopmentPlanService`
- `cyclePlayerInstructionService`
- `holdTeamMeetingService`
- `talkToPlayerService`
- `cycleTrainingFocusService`
- `cycleMatchInstructionService`

### Helpers privados a mover

- `nextDevelopmentPlan`
- `nextInstructionForPlayer`
- `nextTrainingFocus`
- `nextMatchInstruction`

### Helpers compartidos que permanecen en sus módulos actuales

- `promiseAtRisk`
- `playerHasTrait`
- `defaultDutyForPosition`
- `normalizePosition`
- `ensureTeamIdentity`
- `clampInt`

### Fuera de alcance

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

## Pasos

### 1. Agregar prueba estructural RED

En `tests/project_tests.cpp`:

- crear `testAppServiceManagerIsSeparatedFromMainOrchestrator`
- verificar existencia de `src/career/app_services_manager.cpp`
- registrar el test como `app_services_manager_split`

Ejecutar:

`cmake --build .\build-ci --target FootballManagerTests`

y confirmar que la nueva prueba falla porque el archivo todavía no existe.

### 2. Crear módulo mínimo

Crear:

`src/career/app_services_manager.cpp`

con el include mínimo de:

`career/app_services.h`

Agregar el archivo a `FM_CAREER_SOURCES` en `CMakeLists.txt`.

Recompilar y confirmar GREEN estructural mínimo.

### 3. Preparar dependencias privadas

Agregar al nuevo módulo los headers necesarios para:

- gestión de plantilla
- vestuario
- entrenamiento
- negociación/promesas
- modelos/utilidades

Agregar una copia privada mínima de:

`failure(...)`

### 4. Copiar helpers exclusivos

Copiar al nuevo módulo:

- `nextDevelopmentPlan`
- `nextInstructionForPlayer`
- `nextTrainingFocus`
- `nextMatchInstruction`

Mantenerlos temporalmente en `app_services.cpp`.

Compilar para validar dependencias.

### 5. Mover servicios públicos

Mover al nuevo módulo:

- `cyclePlayerDevelopmentPlanService`
- `cyclePlayerInstructionService`
- `holdTeamMeetingService`
- `talkToPlayerService`
- `cycleTrainingFocusService`
- `cycleMatchInstructionService`

Eliminar sus definiciones de `app_services.cpp`.

Mantener intactas las declaraciones de `include/career/app_services.h`.

### 6. Eliminar helpers exclusivos de app_services.cpp

Una vez que el nuevo módulo compile correctamente, eliminar del archivo principal:

- `nextDevelopmentPlan`
- `nextInstructionForPlayer`
- `nextTrainingFocus`
- `nextMatchInstruction`

### 7. Verificación estructural

Comprobar que:

- las definiciones movidas viven en `app_services_manager.cpp`
- `app_services.cpp` solo mantiene llamadas legítimas desde decisiones semanales e inbox
- no quedan duplicados de helpers exclusivos

### 8. Validación completa

Ejecutar:

`cmake --build .\build-ci --target FootballManagerTests`

`ctest --test-dir .\build-ci --output-on-failure`

`cmake --build .\build-ci --target FootballManager`

`cmake --build .\build-ci --target FootballManagerCLI`

`.\build-ci\bin\FootballManagerCLI.exe --validate`

Resultado esperado de validación:

- 5 divisiones
- 90 equipos
- 2200 jugadores
- 0 errores
- 0 advertencias
- sin fallas

### 9. Documentación

Agregar evidencia del refactor a `TODO.md`:

- alcance
- TDD RED/GREEN
- compilación
- validación
- verificación estructural
- resultado

### 10. Revisión final

Ejecutar:

`git diff --check`

Revisar:

`git status --short`

Preparar únicamente los archivos correspondientes al refactor.

### 11. Commit e integración

Commit sugerido:

`refactor: separar manager de app services`

Publicar la rama:

`refactor/app-services-manager`

Integrar con `main` mediante fast-forward y subir `main`.

## Criterio de éxito

El refactor se considera completo si:

- todos los tests pasan
- los tres targets compilan
- la validación del juego queda limpia
- no cambia el comportamiento visible
- `app_services.cpp` queda más pequeño y conserva solo la orquestación legítima hacia estos servicios
