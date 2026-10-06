# Changelog

## v0.1.2.0-alpha - 2026-10-06

### Match Center interactivo
- El Match Center ahora permite tomar decisiones durante el partido.
- Se agregaron cambios de mentalidad e instrucciones desde la interfaz.
- Mentalidad e instruccion pueden aplicarse juntas en el mismo corte.
- Se agregaron sustituciones manuales con seleccion de jugador que sale y entra.
- Las sustituciones realizadas por la IA ahora son visibles en el Match Center.
- Se incorporaron controles de pausa y reanudacion.
- Se agregaron velocidades de simulacion `1x`, `2x` y `4x`.
- Los cortes de decision continúan detenidos hasta confirmar con `CONTINUAR`.

### Interfaz
- Se rediseño el menu principal.
- Se rediseño el flujo de nueva carrera.
- Se rediseñaron las pantallas de guardados, configuraciones y creditos.
- Se mejoro la proteccion frente a errores al iniciar o navegar por una nueva carrera.
- Se centro y ajusto el boton volver en configuraciones.
- Se incorporaron mejoras visuales y de distribucion en el dashboard.

### Gestion de carrera
- Se amplio la modularizacion de `CareerService`.
- Se migraron finanzas semanales, contratos, ofertas de transferencias y actualizaciones fisicas.
- Se migraron simulaciones de divisiones, liga activa y copa.
- Se migraron comunicaciones del staff, noticias de plantilla, narrativas y eventos de carrera.
- Se migraron reputacion y despido del manager al servicio de carrera.

### Economia y dashboard
- Se agrego salary cap relativo a los ingresos del club.
- Se añadieron KPIs visuales al dashboard.
- Se incorporo menu contextual de jugadores.

### Calidad y pruebas
- Se ampliaron las pruebas de sustituciones manuales y de la IA.
- Se agrego cobertura para decisiones combinadas de mentalidad e instruccion.
- Se amplio la cobertura del runtime del Match Center.
- Compilacion completa y suite automatica verificadas durante el desarrollo.

## v0.1.1-alpha - 2026-09-26

### Match Center
- Se agrego mapa de calor del partido.
- Se incorporaron estadisticas avanzadas por jugador.
- Se agrego linea temporal interactiva de eventos.
- Se integro el Match Center con el analisis postpartido.
- Se amplio la cobertura de pruebas unitarias del Match Center.

### Notificaciones e interfaz
- Las transferencias completadas ahora generan notificaciones.
- Los eventos criticos de la interfaz quedan registrados en el sistema de eventos.
- Se agregaron atajos de teclado: Ctrl+F para busqueda global, S para scouting y V para vender.
- La busqueda global permite localizar jugadores y clubes desde la interfaz de fichajes.
- Se agregaron KPIs visuales y accionables al dashboard para facilitar la lectura del estado del club.

### Gestion y finanzas
- Se incorporo un salary cap relativo a los ingresos semanales reales del club.
- Las infracciones salariales ahora calculan exceso y severidad contra el limite economico permitido.
- La pantalla de Finanzas muestra el limite de fair play y el porcentaje de uso salarial.

### Calidad y mantenimiento
- Se cerro la cobertura pendiente de pruebas para objetivos sugeridos por la directiva.
- Se ampliaron las pruebas automatizadas relacionadas con las nuevas funciones de interfaz y Match Center.

## 2026-04-04

### Mejoras Gameplay Mayores - Nuevos Sistemas

#### Sistema Social de Vestuario
- **Cliques de Jugadores**: Los jugadores se agrupan en cliques sociales con personalidades distintas ("Líderes", "Profesionales", "Revoltosos")
- **Dinámicas de Grupo**: Las cliques tienen cohesión variable que afecta la moral colectiva
- **Química entre Jugadores**: Relaciones interpersonales entre jugadores con impacto en rendimiento de cancha
- **Resolución de Conflictos**: Sistema para manejar tensiones internas y conflictos entre jugadores

#### Estrés del Manager
- **Nivel de Estrés**: Afecta directamente la calidad de decisiones en fichajes, tácticas y scouting
- **Energía y Fortaleza Mental**: Recursos que se recuperan con descanso
- **Efectos en Decisiones**: Mayor estrés = decisiones más erráticas (ej: pagar más en transferencias, mal scouting)
- **Eventos de Colapso**: Si estrés supera 90, riesgo de crisis mental del manager

#### IA Rival Adaptativa
- **Personalidades de Equipos**: Cada rival tiene estilo único (agresivo, defensivo, balanced, contra-ataque, posesión)
- **Memoria de Encuentros**: La IA recuerda sus enfrentamientos previos contra ti y se adapta
- **Cambios Tácticos Reactivos**: Los rivales ajustan su táctica según cómo va el partido (si van perdiendo, atacan más)
- **Tasa de Error**: Equipos con menor prestigio cometen más errores por "presión"

#### Sistema de Rivalidades Dinámicas
- **Historial de Enfrentamientos**: Registro de todos los encuentros entre equipos
- **Intensidad de Rivalidad**: Se incrementa con encuentros frecuentes y rachas significativas
- **Rivalidades Locales e Históricas**: Se detectan automáticamente (ej: Colo-Colo vs U. de Chile = 85 de intensidad)
- **Narrativa Generada**: Se crean reportes sobre el estado de cada rivalidad

#### Cambios Tácticos en Tiempo Real
- **3 Cambios por Partido**: El jugador puede cambiar táctica/formación hasta 3 veces por partido
- **Restricciones Tácticas**: No permite cambios en primeros 10 minutos o últimos 5 minutos
- **Costo Energético**: Cada cambio cuesta energía del manager
- **Espaciado Mínimo**: No dos cambios en menos de 15 minutos

#### Sistema de Patrocinio Dinámico
- **Patrocinadores con Objetivos**: Pagan según cumples metas (posición final, victorias, puntos)
- **Bonificaciones/Penalizaciones**: Ingresos extras si cumples, castigos económicos si no
- **Conflictos Potenciales**: Algunos sponsors no quieren jugar contra rivales específicos

#### Infraestructura con Impacto Real
- **5 Niveles de Mejora** para cada instalación:
  - Centro de Entrenamiento (efectividad de entrenamientos)  
  - Academia de Cantera (probabilidad de talento joven)
  - Departamento Médico (velocidad de recuperación de lesiones)
  - Estadio (ingresos por taquilla y ambiente local)
  - Comodidades (felicidad de jugadores)
- **Modificadores Multiplicativos**: Cada nivel aplica bonificadores reales al rendimiento
- **Presupuesto de Mejoras**: Capacity limitado basado en finanzas

#### Sistema de Deuda y Consecuencias
- **Cálculo Automático de Severidad**: Deuda/Ingresos determina severidad (0-100)
- **Restricciones Progresivas**:
  - Severidad >30: Salarios limitados
  - Severidad >70: No puedes comprar jugadores
  - Severidad >85: Riesgo de embargo en 6 meses
  - En default: Puntos descontados en liga
- **Sanciones Financieras**: Baneos de transferencia, caps salariales, restricciones de cantera

### Integración de Sistemas
- Todos los sistemas están integrados en la estructura Career
- Los datos persisten en saves (listos para implementación en sistema de guardado)
- Compilación exitosa: FootballManager.exe, CLI y Tests

## 2026-03-26

### Frontend y portada

- se profesionalizo la portada de `Chilean Footballito` en GUI y CLI con arquitectura de menu compartida
- se agregaron `Continuar`, `Cargar guardado`, `Creditos` y `Salir` al frontend real
- se ampliaron las configuraciones con idioma, velocidad de texto, perfil visual, modo de musica y fade de audio
- las configuraciones ahora se persisten en `saves/game_settings.cfg`

### GUI y rendimiento

- se diferio el cambio de pagina con cola de mensajes para evitar solapes visuales
- se agrego cache de modelos para vistas pesadas como `Fichajes`, `Finanzas` y `Noticias`
- se incorporo traza de tiempo por pagina en el runtime de GUI
- la cabecera ahora resuelve y muestra mejor la division activa, y bloquea `Division` y `Club` cuando hay carrera activa

### Audio y assets

- la musica del frontend se movio a `assets/audio/`
- se agrego `assets/audio/menu_themes.csv` como manifiesto de temas y metadatos basicos
- el audio del menu ahora soporta alcance configurable y fade-in/fade-out

### Persistencia y compatibilidad

- los saves legacy migran automaticamente nombres visibles de division a IDs canonicos
- la reconstruccion de divisiones cargadas ahora se hace desde el registro real del juego

### Tooling y documentacion

- `build.bat` se preparo para compilar objetivos especificos y correr tests opcionalmente
- `README.md` se actualizo para reflejar frontend, audio, settings persistentes y nuevas rutas de compilacion
- `TODO.md` mantiene el historial append-only de todas las entregas
