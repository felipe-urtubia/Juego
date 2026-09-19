# Interactive Match Center - Design

## Objetivo

Convertir el Match Center actual, que hoy reproduce un partido ya simulado, en una experiencia interactiva donde las decisiones del usuario durante el encuentro afecten realmente las fases posteriores de la simulacion.

## Estado actual

- `match_engine::simulate(...)` simula el partido completo en seis fases de 15 minutos.
- Antes de cada fase, `ai_match_manager::applyInMatchManagement(...)` gestiona automaticamente a ambos equipos.
- `match_center::showMatchCenter(...)` recibe un `MatchResult` ya terminado y solo reproduce su timeline.
- El equipo del usuario actualmente puede recibir cambios tacticos y sustituciones automaticas durante la simulacion.
- `LiveState` ya permite representar marcador, tiros, posesion, momentum, tarjetas, sustituciones, cambios tacticos, xG y valoraciones en vivo.

## Principios de diseño

1. Mantener intactas las APIs actuales de simulacion para partidos normales y CPU vs CPU.
2. El motor de simulacion no debe leer teclado ni imprimir menus.
3. Las decisiones humanas deben representarse mediante estructuras de datos y callbacks.
4. El motor conserva la autoridad para validar y aplicar las decisiones.
5. La IA rival mantiene su gestion automatica actual.
6. El equipo del usuario deja de usar gestion automatica normal durante un partido interactivo.
7. Las decisiones se toman entre fases y afectan el snapshot de la siguiente fase.
8. El postprocesado actual debe seguir ejecutandose una sola vez al finalizar el encuentro.

## Flujo interactivo

Partido
  -> simular fase
  -> actualizar timeline, estadisticas y momentum
  -> construir estado parcial
  -> solicitar decision del manager humano
  -> validar/aplicar decision
  -> reconstruir snapshots
  -> simular siguiente fase
  -> repetir hasta el final

## Puntos de decision

La primera version ofrecera una pausa despues de cada fase:

- minuto 15
- minuto 30
- minuto 45
- minuto 60
- minuto 75

No se solicitara decision despues del minuto 90.

## Decisiones soportadas

Primera version:

- Continuar sin cambios.
- Cambiar mentalidad/tactica.
- Cambiar instruccion de partido.
- Realizar una sustitucion manual.

Las decisiones futuras podran ampliar esta estructura sin cambiar la interfaz principal del motor.

## API propuesta

Agregar estructuras publicas al modulo de simulacion para representar:

- estado parcial visible del partido;
- equipo controlado por el usuario;
- jugadores actualmente en cancha;
- jugadores disponibles en el banco;
- numero de sustituciones usadas;
- decision solicitada por callback.

El callback devolvera una decision al motor. El callback no modificara directamente las estructuras internas del simulador.

## Validacion

El motor validara:

- que la tactica solicitada sea valida;
- que la instruccion solicitada sea valida;
- que una sustitucion use un jugador actualmente en cancha;
- que el jugador entrante este disponible;
- que no se excedan cinco sustituciones;
- que un jugador sustituido no vuelva a entrar;
- que los indices pertenezcan al plantel correcto.

Una decision invalida no debe romper la simulacion y se tratara como continuar sin cambios.

## Gestion automatica

CPU:
- conserva `ai_match_manager::applyInMatchManagement(...)`.

Equipo humano:
- en modo interactivo no recibe la gestion automatica normal.
- las lesiones que requieran una solucion obligatoria podran tener un fallback automatico seguro si el usuario no realiza un cambio valido.

Modo no interactivo:
- conserva exactamente el comportamiento actual.

## Presentacion

La consola reutilizara el Match Center y `LiveState`.

La capa de presentacion:
- muestra el estado parcial;
- presenta las opciones;
- convierte la eleccion del usuario en una decision estructurada;
- devuelve esa decision al motor.

El renderer no decide reglas de simulacion.

## Compatibilidad

Deben seguir funcionando sin cambios:

- `simulate(home, away, ...)`
- `simulate(home, away, career, ...)`
- `simulateMatch(...)`
- `playMatch(...)`
- partidos CPU vs CPU
- simulacion detallada tradicional
- guardado y analisis postpartido

La nueva ruta interactiva solo se utilizara cuando `WeekSimulationPresentation::MatchCenter` este activo para el partido del usuario.

## Pruebas

Se agregaran pruebas para comprobar al menos:

1. una decision tactica humana cambia la tactica usada en fases posteriores;
2. una instruccion humana se aplica durante el partido;
3. una sustitucion manual valida cambia el XI;
4. no se permiten mas de cinco sustituciones;
5. una decision invalida no corrompe el partido;
6. la IA sigue gestionando al rival;
7. las APIs de simulacion existentes mantienen su comportamiento;
8. el Match Center interactivo produce un resultado final valido.

## Fuera de alcance de la primera version

- cambios de formacion completos durante el partido;
- arrastrar jugadores en GUI;
- instrucciones individuales en vivo;
- pausas evento por evento;
- VAR;
- tiempo agregado interactivo;
- ventanas reglamentarias de sustituciones;
- multijugador.

Estas mejoras pueden añadirse despues sobre la misma arquitectura.