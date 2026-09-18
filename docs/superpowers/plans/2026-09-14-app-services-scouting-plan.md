# App Services Scouting Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extraer la responsabilidad de Scouting desde `src/career/app_services.cpp` hacia `src/career/app_services_scouting.cpp` sin modificar la API pública ni el comportamiento del juego.

**Architecture:** `include/career/app_services.h` continuará siendo la fachada pública. El nuevo archivo `app_services_scouting.cpp` contendrá los servicios públicos y helpers privados exclusivos de Scouting; `app_services.cpp` conservará las demás responsabilidades.

**Tech Stack:** C++17, CMake, MinGW, PowerShell, tests personalizados mediante `FootballManagerTests`, CTest.

**Spec:** `docs/superpowers/specs/2026-09-14-app-services-scouting-design.md`

## Global Constraints

* No modificar reglas del juego.
* No modificar firmas públicas.
* No modificar mensajes visibles.
* No modificar costos ni cálculos de Scouting.
* No modificar estructura de guardado.
* No mover `changeYouthRegionService`.
* No refactorizar transferencias, juveniles, board ni manager interactions.
* Mantener `include/career/app_services.h` como fachada pública.
* Seguir RED → GREEN antes de cualquier limpieza adicional.

---

### Task 1: Crear prueba estructural RED para el módulo de Scouting

**Files:**

* Modify: `tests/project_tests.cpp`
* Future create: `src/career/app_services_scouting.cpp`

**Interfaces:**

* Consumes: `resolveProjectPath`, `pathExists`, `expect`.

* Produces: test registrado como `app_services_scouting_split`.

* [ ] **Step 1: Agregar la prueba estructural**

Agregar cerca de `testAppServiceReportsAreSeparatedFromMainOrchestrator()`:

```cpp
void testAppServiceScoutingIsSeparatedFromMainOrchestrator() {
    const string scoutingPath =
        resolveProjectPath("src/career/app_services_scouting.cpp");

    expect(pathExists(scoutingPath),
           "Los servicios de scouting de app_services deben vivir en app_services_scouting.cpp.");
}
```

* [ ] **Step 2: Registrar la prueba**

Agregar en la lista de pruebas de `main()`:

```cpp
{"app_services_scouting_split", testAppServiceScoutingIsSeparatedFromMainOrchestrator},
```

Ubicarla junto a:

```cpp
{"app_services_report_split", testAppServiceReportsAreSeparatedFromMainOrchestrator},
```

* [ ] **Step 3: Compilar tests**

Run:

```powershell
cmake --build .\build-ci --target FootballManagerTests
```

Expected:

```text
Built target FootballManagerTests
```

* [ ] **Step 4: Ejecutar CTest y comprobar RED**

Run:

```powershell
ctest --test-dir .\build-ci --output-on-failure
```

Expected:

```text
[FAIL] app_services_scouting_split
```

La prueba debe fallar porque `src/career/app_services_scouting.cpp` todavía no existe.

No continuar si la prueba pasa antes de crear el archivo.

---

### Task 2: Crear el módulo `app_services_scouting.cpp`

**Files:**

* Create: `src/career/app_services_scouting.cpp`
* Modify: `src/career/app_services.cpp`
* Modify: `CMakeLists.txt`

**Interfaces:**

* Consumes:

  * `Career`
  * `Team`
  * `Player`
  * `ScoutingSessionResult`
  * `ScoutingCandidate`
  * `ScoutingAssignment`
  * `ServiceResult`

* Produces, sin cambiar firmas:

  * `runScoutingSessionService`
  * `scoutPlayersService`
  * `createScoutingAssignmentService`
  * `shortlistPlayerService`
  * `followShortlistService`
  * `listYouthRegionsService`

* [ ] **Step 1: Crear el archivo**

Crear:

```text
src/career/app_services_scouting.cpp
```

El archivo deberá comenzar incluyendo la fachada pública:

```cpp
#include "career/app_services.h"
```

Y deberá incluir directamente los headers necesarios para las dependencias utilizadas por Scouting, entre ellos los que declaran:

```text
formatMoneyValue
detectScoutingNeed
positionFitScore
playerReliabilityLabel
playerFormLabel
ensureTeamIdentity
joinStringValues
personalityLabel
wageDemandFor
normalizePosition
player_condition
world_state_service
```

No depender de includes accidentales provenientes de `app_services.cpp`.

* [ ] **Step 2: Mover los helpers privados exclusivos de Scouting**

Mover sin modificar comportamiento:

```text
resolveAssignmentRegion
assignmentPriorityLabel
hasScoutingCoverage
scoutingCoverageLabel
availabilityLabel
agentProfileLabel
scoutingReportStage
scoutingHiddenRiskLabel
scoutingAssignmentBoost
appendScoutInbox
```

Mantenerlos privados al `.cpp`, dentro del mismo ámbito anónimo utilizado por el proyecto si corresponde.

No modificar condiciones, constantes, textos ni cálculos.

* [ ] **Step 3: Mover `runScoutingSessionService`**

Mover completa y literalmente la implementación existente de:

```cpp
ScoutingSessionResult runScoutingSessionService(
    Career& career,
    const string& region,
    const string& focusPos);
```

Preservar:

* validación de carrera activa;
* costo de scouting;
* selección de región;
* posición objetivo;
* cobertura regional;
* orden de candidatos;
* error estimado;
* confianza;
* readiness;
* riesgo médico;
* recomendaciones;
* valoración;
* salario esperado;
* informes;
* inbox;
* noticias.

No cambiar números ni mensajes.

* [ ] **Step 4: Mover los demás servicios públicos**

Mover sin modificaciones funcionales:

```cpp
ServiceResult scoutPlayersService(
    Career& career,
    const string& region,
    const string& focusPos);
```

```cpp
ServiceResult createScoutingAssignmentService(
    Career& career,
    const string& region,
    const string& focusPos,
    int durationWeeks);
```

```cpp
ServiceResult shortlistPlayerService(...);
```

```cpp
ServiceResult followShortlistService(Career& career);
```

```cpp
std::vector<std::string> listYouthRegionsService();
```

* [ ] **Step 5: Eliminar solamente esas implementaciones desde `app_services.cpp`**

Después de moverlas, comprobar que `app_services.cpp` ya no contiene definiciones de:

```text
resolveAssignmentRegion
assignmentPriorityLabel
hasScoutingCoverage
scoutingCoverageLabel
availabilityLabel
agentProfileLabel
scoutingReportStage
scoutingHiddenRiskLabel
scoutingAssignmentBoost
appendScoutInbox
runScoutingSessionService
scoutPlayersService
createScoutingAssignmentService
shortlistPlayerService
followShortlistService
listYouthRegionsService
```

No eliminar llamadas a estos servicios desde otras funciones.

No mover:

```text
changeYouthRegionService
```

* [ ] **Step 6: Registrar el nuevo archivo en CMake**

En:

```cmake
set(FM_CAREER_SOURCES
```

agregar:

```cmake
src/career/app_services_scouting.cpp
```

cerca de:

```cmake
src/career/app_services.cpp
src/career/app_services_reports.cpp
```

El bloque esperado será conceptualmente:

```cmake
set(FM_CAREER_SOURCES
    src/career/analytics_service.cpp
    src/career/app_services.cpp
    src/career/app_services_reports.cpp
    src/career/app_services_scouting.cpp
    src/career/career_manager.cpp
```

---

### Task 3: Resolver dependencias de compilación sin ampliar el alcance

**Files:**

* Modify only if required: `src/career/app_services_scouting.cpp`

**Interfaces:**

* Consumes headers públicos existentes.

* Produces un translation unit independiente que compila sin depender de `app_services.cpp`.

* [ ] **Step 1: Reconfigurar CMake**

Run:

```powershell
cmake -S . -B .\build-ci
```

Expected:

```text
Configuring done
Generating done
```

* [ ] **Step 2: Compilar solamente tests**

Run:

```powershell
cmake --build .\build-ci --target FootballManagerTests
```

Expected:

```text
Built target FootballManagerTests
```

* [ ] **Step 3: Si existen errores de símbolos o declarations**

Corregir exclusivamente includes/dependencias del nuevo:

```text
src/career/app_services_scouting.cpp
```

No duplicar implementaciones compartidas.

No mover helpers genéricos desde otros módulos.

No modificar interfaces públicas para resolver includes.

---

### Task 4: Confirmar GREEN

**Files:**

* No nuevos archivos.

**Interfaces:**

* Verifica el resultado de Tasks 1–3.

* [ ] **Step 1: Ejecutar tests**

Run:

```powershell
ctest --test-dir .\build-ci --output-on-failure
```

Expected:

```text
100% tests passed
```

La prueba:

```text
app_services_scouting_split
```

debe pasar.

* [ ] **Step 2: Confirmar ausencia de definiciones movidas**

Run:

```powershell
Select-String -Path .\src\career\app_services.cpp -Pattern "runScoutingSessionService|scoutPlayersService|createScoutingAssignmentService|shortlistPlayerService|followShortlistService|listYouthRegionsService"
```

Expected:

```text
sin resultados
```

Las llamadas desde otras funciones pueden requerir una comprobación separada si el patrón encuentra usos que no sean definiciones.

---

### Task 5: Compilar las aplicaciones reales

**Files:**

* No modificaciones previstas.

* [ ] **Step 1: Compilar GUI y CLI**

Run:

```powershell
cmake --build .\build-ci --target FootballManager FootballManagerCLI
```

Expected:

```text
Built target FootballManager
Built target FootballManagerCLI
```
* [ ] **Step 2: Confirmar que el nuevo módulo participa en ambos targets**

En la salida de compilación debe aparecer, cuando corresponda:

```text
app_services_scouting.cpp.obj
```

---

### Task 6: Ejecutar la validación funcional

**Files:**

* No modificaciones previstas.

* [ ] **Step 1: Ejecutar suite de validación**

Run:

```powershell
.\build-ci\bin\FootballManagerCLI.exe --validate
```

Expected:

```text
Divisiones: 5
Equipos revisados: 90
Jugadores crudos: 2200
Errores: 0
Advertencias: 0
Resultado: sin fallas
```

Si los conteos de datos cambian legítimamente por cambios externos realizados entre commits, lo obligatorio para este refactor es:

```text
Errores: 0
Advertencias: 0
Resultado: sin fallas
```

---

### Task 7: Revisar integridad del cambio

**Files:**

* `CMakeLists.txt`

* `src/career/app_services.cpp`

* `src/career/app_services_scouting.cpp`

* `tests/project_tests.cpp`

* [ ] **Step 1: Revisar estado**

Run:

```powershell
git status
```

Esperado antes de documentación:

```text
modified: CMakeLists.txt
modified: src/career/app_services.cpp
modified: tests/project_tests.cpp
untracked: src/career/app_services_scouting.cpp
```

Además estará presente el plan de implementación si todavía no fue committeado.

* [ ] **Step 2: Revisar diff**

Run:

```powershell
git --no-pager diff -- CMakeLists.txt src/career/app_services.cpp tests/project_tests.cpp
```

Verificar:

* CMake solo incorpora el nuevo source.

* `app_services.cpp` solo pierde código correspondiente a Scouting.

* tests solo reciben la nueva prueba y su registro.

* [ ] **Step 3: Revisar archivo nuevo**

Run:

```powershell
Get-Content .\src\career\app_services_scouting.cpp
```

Verificar que las implementaciones movidas mantengan los mismos cálculos y mensajes.

* [ ] **Step 4: Verificar whitespace**

Run:

```powershell
git diff --check
```

Expected:

```text
sin errores
```

Las advertencias de conversión LF/CRLF en Windows no se consideran fallos funcionales.

---

### Task 8: Registrar evidencia en `TODO.md`

**Files:**

* Modify: `TODO.md`

* [ ] **Step 1: Agregar evidencia de la segunda fase**

Registrar:

```text
Refactor App Services - Separación de Scouting
```

Incluyendo:

* estado completado;

* objetivo;

* archivo creado;

* helpers movidos;

* servicios movidos;

* prueba RED;

* resultado GREEN;

* compilación de Tests, GUI y CLI;

* resultado de `--validate`;

* resultado de `git diff --check`;

* confirmación de que no hubo cambios funcionales.

* [ ] **Step 2: Volver a verificar el diff**

Run:

```powershell
git diff --check
```

Expected:

```text
sin errores
```

---

### Task 9: Verificación final antes del commit

**Files:**

* Todos los archivos de esta fase.

* [ ] **Step 1: Ejecutar nuevamente CTest**

Run:

```powershell
ctest --test-dir .\build-ci --output-on-failure
```

Expected:

```text
100% tests passed
```

* [ ] **Step 2: Ejecutar nuevamente validación CLI**

Run:

```powershell
.\build-ci\bin\FootballManagerCLI.exe --validate
```

Expected:

```text
Errores: 0
Advertencias: 0
Resultado: sin fallas
```

* [ ] **Step 3: Revisar estado final**

Run:

```powershell
git status
```

Confirmar que no existan archivos inesperados.

---

### Task 10: Commit de implementación

**Files expected:**

* `CMakeLists.txt`

* `TODO.md`

* `src/career/app_services.cpp`

* `src/career/app_services_scouting.cpp`

* `tests/project_tests.cpp`

* `docs/superpowers/plans/2026-09-14-app-services-scouting-plan.md`

* [ ] **Step 1: Preparar archivos**

Run:

```powershell
git add CMakeLists.txt TODO.md src/career/app_services.cpp src/career/app_services_scouting.cpp tests/project_tests.cpp docs/superpowers/plans/2026-09-14-app-services-scouting-plan.md
```

* [ ] **Step 2: Revisar staging**

Run:

```powershell
git status
```

y:

```powershell
git diff --cached --stat
```

* [ ] **Step 3: Comprobar whitespace del staging**

Run:

```powershell
git diff --cached --check
```

Expected:

```text
sin errores
```

* [ ] **Step 4: Crear commit**

Run:

```powershell
git commit -m "refactor: separar scouting de app services"
```

* [ ] **Step 5: Confirmar árbol limpio**

Run:

```powershell
git status
```

Expected:

```text
nothing to commit, working tree clean
```

---

### Task 11: Publicar y comprobar integración remota

* [ ] **Step 1: Push**

Run:

```powershell
git push origin main
```

Expected:

```text
main -> main
```

El push deberá incluir también el commit previo del diseño si todavía estaba únicamente en local.

* [ ] **Step 2: Confirmar sincronización**

Run:

```powershell
git status
```

Expected:

```text
Your branch is up to date with 'origin/main'.
nothing to commit, working tree clean
```

* [ ] **Step 3: Revisar GitHub Actions**

Comprobar que el workflow correspondiente al commit final complete correctamente antes de declarar cerrada la fase.

## Resultado final

La fase se considera completada únicamente cuando:

* `app_services_scouting.cpp` existe;
* la lógica de Scouting definida en la especificación dejó `app_services.cpp`;
* la API pública no cambió;
* todos los tests pasan;
* GUI y CLI compilan;
* `--validate` termina sin fallas;
* la evidencia está en `TODO.md`;
* el repositorio queda limpio;
* los commits están publicados;
* el CI remoto está verificado.
