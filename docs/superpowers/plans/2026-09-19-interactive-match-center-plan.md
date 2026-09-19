# Interactive Match Center - Implementation Plan

## Objetivo

Implementar un Match Center realmente interactivo donde las decisiones del usuario entre fases de 15 minutos modifiquen la simulacion de las fases posteriores.

## Alcance de la primera version

Decisiones humanas soportadas:

- continuar sin cambios;
- cambiar tactica/mentalidad;
- cambiar instruccion de partido;
- realizar una sustitucion manual.

Puntos de decision:

- minuto 15;
- minuto 30;
- minuto 45;
- minuto 60;
- minuto 75.

## Paso 1 - Definir API interactiva del motor

Modificar `include/simulation/match_engine.h`.

Agregar:

- tipo de decision del manager;
- estructura con la decision solicitada;
- estructura con el estado parcial visible para el callback;
- callback de decision;
- nueva funcion de simulacion interactiva.

La API existente debe mantenerse intacta.

## Paso 2 - Agregar pruebas RED

Agregar pruebas en `tests/project_tests.cpp` para cubrir:

1. callback invocado durante el partido;
2. cambio de tactica humano;
3. cambio de instruccion humano;
4. sustitucion manual valida;
5. rechazo de sustitucion invalida;
6. limite de cinco sustituciones;
7. rival gestionado automaticamente;
8. simulacion tradicional sin regresiones.

Ejecutar tests y confirmar RED antes de implementar comportamiento.

## Paso 3 - Extraer nucleo comun de simulacion

Refactorizar `src/simulation/match_engine.cpp` para evitar duplicar el loop de seis fases.

Crear internamente una ruta comun que reciba configuracion opcional de interaccion.

La simulacion tradicional debe llamar al mismo nucleo con interaccion desactivada.

## Paso 4 - Identificar equipo humano

En modo interactivo:

- determinar si el usuario controla local o visitante;
- guardar esa informacion durante toda la simulacion;
- no ejecutar gestion automatica normal sobre ese equipo;
- conservar gestion automatica para el rival.

## Paso 5 - Construir estado parcial

Despues de cada fase:

- marcador;
- minuto;
- estadisticas acumuladas;
- posesion acumulada;
- momentum;
- xG;
- tarjetas;
- sustituciones;
- tactica actual;
- instruccion actual;
- XI actual;
- banco disponible.

Este estado se entregara al callback sin permitir acceso directo al estado interno mutable.

## Paso 6 - Aplicar decision tactica

Validar valores permitidos.

Tacticas iniciales soportadas:

- Defensive
- Balanced
- Offensive
- Pressing
- Counter

Cuando sea valida:

- actualizar `TeamRuntimeState.team.tactics`;
- registrar `MatchEventType::TacticalChange`;
- permitir que la siguiente fase reconstruya su snapshot usando la nueva tactica.

## Paso 7 - Aplicar instruccion de partido

Validar instrucciones permitidas:

- Equilibrado
- Laterales altos
- Bloque bajo
- Balon parado
- Presion final
- Por bandas
- Juego directo
- Contra-presion
- Pausar juego

Registrar el cambio en el timeline como evento tactico.

## Paso 8 - Sustitucion manual

Validar:

- jugador saliente pertenece al XI;
- jugador entrante pertenece al plantel;
- jugador entrante no esta actualmente en cancha;
- jugador entrante no fue sustituido anteriormente;
- quedan sustituciones disponibles;
- indices validos.

Al aplicar:

- reemplazar indice en `xi`;
- agregar jugador entrante a `participants`;
- registrar evento `Substitution`;
- actualizar contador a traves del timeline.

## Paso 9 - Fallback de lesion

Si el usuario mantiene un jugador lesionado activo y no realiza una sustitucion valida:

- permitir fallback automatico de emergencia;
- reutilizar la logica segura existente cuando sea posible.

No activar sustituciones tacticas automaticas normales para el equipo humano.

## Paso 10 - Integrar con simulation.cpp

Agregar una nueva entrada interactiva sin modificar la semantica de:

- `simulateMatch(...)`;
- `playMatch(...)`.

La nueva funcion debe:

- preparar XI y estadisticas de inicio;
- invocar simulacion interactiva;
- ejecutar `match_postprocess::applySimulationOutcome(...)` una sola vez;
- devolver `MatchResult`.

## Paso 11 - Presentacion de consola

Agregar al modulo Match Center una funcion que:

- reciba el estado parcial;
- construya/reutilice `LiveState`;
- dibuje el Match Center;
- muestre menu de decisiones;
- devuelva una decision estructurada.

La lectura de teclado debe permanecer fuera de `match_engine.cpp`.

## Paso 12 - Integrar week_simulation

Cuando:

`presentation == WeekSimulationPresentation::MatchCenter`

usar la nueva ruta interactiva.

Para:

- CPU vs CPU;
- Detailed;
- simulacion normal;

mantener la ruta actual.

Eliminar la reproduccion posterior redundante si el partido ya fue presentado interactivamente durante la simulacion.

## Paso 13 - Verificacion automatica

Ejecutar:

`cmake --build .\build-ci --target FootballManagerTests`

`ctest --test-dir .\build-ci --output-on-failure`

`cmake --build .\build-ci --target FootballManager`

`cmake --build .\build-ci --target FootballManagerCLI`

`.\build-ci\bin\FootballManagerCLI.exe --validate`

Resultado esperado:

- compilacion correcta;
- 100% tests;
- validacion sin errores ni warnings.

## Paso 14 - Prueba manual

Iniciar una carrera y jugar un partido con Match Center.

Verificar:

- pausa en 15, 30, 45, 60 y 75;
- continuar funciona;
- cambio de tactica afecta fases siguientes;
- cambio de instruccion queda visible y afecta simulacion;
- sustitucion cambia realmente el XI;
- rival sigue realizando ajustes;
- marcador y estadisticas avanzan correctamente;
- partido termina y se procesa normalmente.

## Paso 15 - Documentacion

Actualizar `TODO.md` con:

- Match Center interactivo;
- decisiones tacticas en vivo;
- sustituciones manuales;
- estado de pruebas.

## Paso 16 - Revision final

Ejecutar:

`git diff --check`

Revisar:

`git status --short`

Luego realizar commit de implementacion solamente cuando todo este validado.