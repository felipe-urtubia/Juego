# Plan: separación de transferencias en App Services

## Alcance

Extraer desde `src/career/app_services.cpp` la implementación de transferencias, contratos y préstamos hacia:

`src/career/app_services_transfers.cpp`

Sin cambiar comportamiento, API pública, mensajes, costos ni reglas.

## Paso 1 - Prueba estructural RED

Agregar en `tests/project_tests.cpp` una prueba:

`app_services_transfers_split`

La prueba debe verificar que exista:

`src/career/app_services_transfers.cpp`

Antes de crear el archivo, ejecutar:

`cmake --build .\build-ci --target FootballManagerTests`

y:

`ctest --test-dir .\build-ci --output-on-failure`

Resultado esperado: RED porque el nuevo módulo todavía no existe.

## Paso 2 - Crear módulo mínimo

Crear:

`src/career/app_services_transfers.cpp`

con los includes necesarios.

Agregar el nuevo archivo a `FM_CAREER_SOURCES` en `CMakeLists.txt`.

Confirmar que la prueba estructural pase.

## Paso 3 - Mover helpers privados

Mover sin cambios funcionales:

* `debtRestrictionMessage`
* `totalNegotiationCommitment`
* `describeContractExtras`
* `eraseNamedSelection`
* `failureFromNegotiation`
* `transferWindowClosedFailure`
* `appendNegotiationMessages`
* `registerNegotiatedPromise`

Revisar dependencias antes de mover cada bloque.

## Paso 4 - Mover servicios públicos

Mover sin cambiar firmas:

* `buyTransferTargetService`
* `triggerReleaseClauseService`
* `signPreContractService`
* `renewPlayerContractService`
* `sellPlayerService`
* `loanInPlayerService`
* `loanOutPlayerService`

Mantener sus declaraciones en:

`include/career/app_services.h`

## Paso 5 - Comprobación estructural

Confirmar que las definiciones anteriores ya no estén en `app_services.cpp`.

Las llamadas desde otros servicios pueden permanecer porque usan la fachada pública de `app_services.h`.

## Paso 6 - Tests

Ejecutar:

`cmake --build .\build-ci --target FootballManagerTests`

`ctest --test-dir .\build-ci --output-on-failure`

Resultado esperado: GREEN.

## Paso 7 - Ejecutables reales

Compilar:

`cmake --build .\build-ci --target FootballManager`

`cmake --build .\build-ci --target FootballManagerCLI`

## Paso 8 - Validación completa

Ejecutar:

`.\build-ci\bin\FootballManagerCLI.exe --validate`

Esperado:

* 5 divisiones
* 90 equipos
* 2200 jugadores crudos
* 0 errores
* 0 advertencias

## Paso 9 - Evidencia

Agregar una sección en `TODO.md` con:

* objetivo
* servicios movidos
* helpers movidos
* evidencia RED/GREEN
* compilación
* validación
* `git diff --check`

## Paso 10 - Cierre

Revisar:

`git diff --check`

`git status --short`

Crear commit de implementación y subir la rama.

## Restricción principal

Si durante el movimiento aparece una dependencia compartida con otra responsabilidad de `app_services.cpp`, no se refactorizará esa otra responsabilidad. Se elegirá la solución mínima que preserve exactamente el comportamiento actual.
