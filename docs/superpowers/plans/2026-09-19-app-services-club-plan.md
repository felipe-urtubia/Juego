# Plan: separación de club y staff en App Services

## Alcance

Extraer desde `src/career/app_services.cpp` la gestión de mejoras del club y revisión de staff hacia:

`src/career/app_services_club.cpp`

Sin cambiar comportamiento, API pública, mensajes, costos ni reglas.

## Paso 1 - Prueba estructural RED

Agregar en `tests/project_tests.cpp` una prueba:

`app_services_club_split`

La prueba debe verificar que exista:

`src/career/app_services_club.cpp`

Antes de crear el archivo, ejecutar:

`cmake --build .\build-ci --target FootballManagerTests`

y:

`ctest --test-dir .\build-ci --output-on-failure`

Resultado esperado: RED porque el nuevo módulo todavía no existe.

## Paso 2 - Crear módulo mínimo

Crear:

`src/career/app_services_club.cpp`

Agregarlo a `FM_CAREER_SOURCES` en `CMakeLists.txt`.

Confirmar que la prueba estructural pase.

## Paso 3 - Preparar dependencias privadas

Agregar al nuevo módulo los includes necesarios.

Mantener una copia privada mínima de:

* `failure`
* `syncInfrastructureFromTeam`

La implementación original de `syncInfrastructureFromTeam` permanecerá en `app_services.cpp` porque también la utilizan otros flujos.

## Paso 4 - Mover helpers exclusivos

Mover sin cambios funcionales:

* `nextStaffHireName`
* `upgradeCost`
* `upgradeLabel`
* `isFacilityUpgrade`
* `staffUpgradeForRole`

## Paso 5 - Mover servicios públicos

Mover sin cambiar firmas:

* `upgradeClubService`
* `reviewStaffStructureService`

Mantener sus declaraciones en:

`include/career/app_services.h`

## Paso 6 - Comprobación estructural

Confirmar que las definiciones anteriores ya no estén en `app_services.cpp`.

Las llamadas públicas desde otros flujos pueden permanecer.

Confirmar además que la implementación original de `syncInfrastructureFromTeam` siga en `app_services.cpp`.

## Paso 7 - Tests

Ejecutar:

`cmake --build .\build-ci --target FootballManagerTests`

`ctest --test-dir .\build-ci --output-on-failure`

Resultado esperado: GREEN.

## Paso 8 - Ejecutables reales

Compilar:

`cmake --build .\build-ci --target FootballManager`

`cmake --build .\build-ci --target FootballManagerCLI`

## Paso 9 - Validación completa

Ejecutar:

`.\build-ci\bin\FootballManagerCLI.exe --validate`

Esperado:

* 5 divisiones
* 90 equipos
* 2200 jugadores crudos
* 0 errores
* 0 advertencias

## Paso 10 - Evidencia

Agregar una sección en `TODO.md` con:

* objetivo
* servicios movidos
* helpers movidos
* dependencia compartida
* evidencia RED/GREEN
* compilación
* validación
* `git diff --check`

## Paso 11 - Cierre

Revisar:

`git diff --check`

`git status --short`

Crear commit de implementación y subir la rama.

## Restricción principal

No mover `changeYouthRegionService` ni modificar otros flujos de infraestructura compartida. La solución debe ser mínima y preservar exactamente el comportamiento actual.
