# ⚽ Chilean Footballito

<p align="center">

### Simulador de gestión futbolística desarrollado en C++17

**Versión actual: `v0.1.2.0-alpha`**

Motor de simulación propio • Arquitectura modular • IA táctica • Match Center interactivo • Modo Carrera

</p>

---

## 📖 Descripción

**Chilean Footballito** es un simulador de gestión futbolística desarrollado en **C++17** cuyo objetivo es recrear la experiencia de dirigir un club profesional mediante un motor de simulación propio, una arquitectura modular y distintos sistemas interconectados para gestionar los aspectos deportivos, económicos y estratégicos de una institución.

El proyecto integra:

- Modo Carrera.
- Simulación de partidos.
- Match Center interactivo.
- Mercado de fichajes.
- Desarrollo de jugadores.
- Finanzas y control salarial.
- Inteligencia artificial táctica.
- Persistencia de partidas.
- Interfaz gráfica Win32.
- Frontend CLI.
- Validación automática de datos.
- Suite de pruebas automatizadas.

La arquitectura continúa evolucionando para separar responsabilidades entre motor, simulación, servicios de carrera, IA, transferencias, finanzas, persistencia, validación e interfaz.

---

# 📦 Última versión publicada

La versión pública actual es:

## `v0.1.2.0-alpha`

Release:

https://github.com/felipe-urtubia/Juego/releases/tag/v0.1.2.0-alpha

### Cambios principales

- Match Center interactivo.
- Cambios de mentalidad durante el partido.
- Instrucciones tácticas en vivo.
- Mentalidad e instrucción aplicables en un mismo corte de decisión.
- Sustituciones manuales.
- Sustituciones realizadas por la IA visibles en el Match Center.
- Pausa y reanudación.
- Velocidades de simulación `1x`, `2x` y `4x`.
- Rediseño del menú principal.
- Rediseño del flujo de nueva carrera.
- Rediseño de guardados, configuraciones y créditos.
- Mejoras de estabilidad al iniciar y navegar por una carrera.
- KPIs visuales en el dashboard.
- Menú contextual de jugadores.
- Salary cap relativo a los ingresos del club.
- Mayor modularización de `CareerService`.
- Ampliación de pruebas automatizadas.
- Validación completa de datos antes de la release.

---

# 🚀 Estado actual del proyecto

Chilean Footballito se encuentra en desarrollo activo.

Actualmente dispone de:

- ✅ Motor de simulación minuto a minuto.
- ✅ Modo Carrera.
- ✅ Match Center interactivo.
- ✅ Decisiones tácticas en vivo.
- ✅ Sustituciones manuales.
- ✅ Sustituciones controladas por IA.
- ✅ Pausa y reanudación de partidos.
- ✅ Velocidades `1x`, `2x` y `4x`.
- ✅ IA táctica.
- ✅ Mercado de fichajes.
- ✅ Finanzas.
- ✅ Salary cap relativo a ingresos.
- ✅ Dashboard con KPIs.
- ✅ Desarrollo juvenil.
- ✅ Desarrollo de jugadores.
- ✅ Noticias y comunicaciones.
- ✅ Estadísticas de partido.
- ✅ xG.
- ✅ Momentum.
- ✅ Valoraciones dinámicas.
- ✅ Persistencia de carreras.
- ✅ Suite de validación.
- ✅ Pruebas automatizadas.
- ✅ Interfaz Win32.
- ✅ Frontend CLI.

La release `v0.1.2.0-alpha` fue verificada con:

- Build completo correcto.
- 100% de los tests configurados aprobados.
- 5 divisiones validadas.
- 90 equipos revisados.
- 2200 jugadores auditados.
- 0 errores.
- 0 advertencias.

---

# ⭐ Características principales

## 🏟️ Gestión del Club

El Modo Carrera permite gestionar distintos aspectos deportivos e institucionales.

Entre ellos:

- Plantilla.
- Formación.
- Tácticas.
- Convocatorias.
- Desarrollo de jugadores.
- Desarrollo juvenil.
- Mercado de fichajes.
- Contratos.
- Presupuesto.
- Finanzas.
- Salary cap.
- Objetivos.
- Noticias.
- Comunicaciones del staff.
- Historial de temporada.
- Informes deportivos.
- Reputación del manager.
- Riesgo de despido.

El dashboard centraliza información relevante para la toma de decisiones semanales:

- Estado físico del plantel.
- Moral y tensión del vestuario.
- Flujo de caja.
- Deuda.
- Contratos próximos a vencer.
- Necesidades de plantilla.
- Rotación.
- Indicadores económicos.
- Señales visuales de prioridad.

---

# ⚽ Motor de Simulación

El motor de simulación ha sido desarrollado específicamente para este proyecto.

Entre los sistemas que intervienen se encuentran:

- Simulación minuto a minuto.
- Match Context.
- Fases de partido.
- Eventos dinámicos.
- Resolución de acciones.
- Estadísticas.
- xG.
- Fatiga.
- Moral.
- Lesiones.
- Tarjetas.
- Sustituciones.
- Cambios tácticos.
- Momentum.
- Valoraciones individuales.
- Timeline de partido.

## Flujo simplificado

```text
Minuto
  │
  ▼
Actualizar contexto
  │
  ▼
Evaluar fase del partido
  │
  ▼
Generar eventos
  │
  ▼
Resolver acciones
  │
  ▼
Actualizar estadísticas
  │
  ▼
Actualizar Momentum y valoraciones
  │
  ▼
Notificar Match Center
```

---

# 📺 Match Center interactivo

El Match Center permite seguir y gestionar el partido mientras la simulación está en curso.

No es únicamente una pantalla informativa: desde `v0.1.2.0-alpha` funciona también como interfaz de decisiones en vivo.

## Información mostrada

- Marcador.
- Cronómetro.
- Timeline.
- Eventos recientes.
- Estadísticas.
- Posesión.
- Momentum.
- Comentarios dinámicos.
- Valoraciones.
- Sustituciones.
- Cambios tácticos.

## Controles durante el partido

El jugador puede:

- Cambiar mentalidad.
- Cambiar instrucciones tácticas.
- Aplicar mentalidad e instrucción juntas.
- Realizar sustituciones manuales.
- Pausar el partido.
- Reanudar el partido.
- Cambiar la velocidad a `1x`, `2x` o `4x`.
- Confirmar cortes de decisión mediante `CONTINUAR`.

Las instrucciones incompatibles se muestran deshabilitadas.

Cuando un cambio de mentalidad invalida la instrucción activa, el sistema mantiene una alternativa coherente.

Los cortes de decisión permanecen detenidos hasta que el usuario confirma explícitamente la continuación del partido.

## Sustituciones manuales

El sistema de sustituciones permite:

- Seleccionar el jugador que sale.
- Seleccionar el jugador que entra.
- Revisar posición.
- Revisar estado físico.
- Revisar media.
- Respetar el máximo de sustituciones permitido.
- Mantener sincronizados XI y banca.
- Registrar el cambio como evento del partido.

Las sustituciones realizadas por la IA también se muestran dentro del Match Center.

---

# 🧠 Inteligencia Artificial

El proyecto incorpora distintos sistemas de IA especializados.

Actualmente incluye:

- IA táctica.
- IA para sustituciones.
- IA de gestión del partido.
- IA de planificación de plantilla.
- IA de fichajes.
- IA basada en Momentum.
- Evaluación de contexto deportivo.

Durante un partido, la IA puede considerar:

- Resultado.
- Minuto.
- Estado físico.
- Tarjetas.
- Diferencia de goles.
- Momentum.
- Cambios disponibles.
- Rendimiento del equipo.

Con esta información puede:

- Modificar presión.
- Cambiar ritmo.
- Cambiar mentalidad.
- Adaptar instrucciones.
- Realizar sustituciones.
- Ajustar el comportamiento táctico.

---

# 🏆 Modo Carrera

El Modo Carrera representa el núcleo de la experiencia de gestión.

Permite gestionar un club a largo plazo mediante sistemas deportivos, económicos y estratégicos.

## Flujo general

```text
Nueva semana
   │
   ▼
Calendario
   │
   ▼
Entrenamiento
   │
   ▼
Preparación
   │
   ▼
Partido / Simulación
   │
   ▼
Actualización del club
   │
   ▼
Finanzas
   │
   ▼
Noticias y comunicaciones
   │
   ▼
Mercado
   │
   ▼
Siguiente semana
```

---

# 🧩 CareerService y modularización

Una parte importante del desarrollo reciente se ha centrado en reducir la lógica concentrada en grandes flujos de carrera.

`CareerService` y servicios relacionados gestionan progresivamente responsabilidades como:

- Finanzas semanales.
- Contratos.
- Ofertas de transferencias.
- Actualizaciones físicas.
- Simulación de divisiones.
- Simulación de liga activa.
- Copa.
- Comunicaciones del staff.
- Noticias de plantilla.
- Narrativas semanales.
- Eventos de carrera.
- Reputación del manager.
- Despido del manager.

El objetivo es que GUI y CLI consuman APIs estructuradas y que las reglas de negocio permanezcan en servicios testeables.

---

# 💰 Finanzas

La economía constituye uno de los pilares del Modo Carrera.

El sistema administra:

- Presupuesto.
- Salarios.
- Ingresos.
- Gastos.
- Balance.
- Flujo de caja.
- Deuda.
- Situación económica.

## Salary cap

El proyecto incorpora un límite salarial basado en los ingresos reales del club.

El sistema considera:

- Ingresos semanales.
- Masa salarial actual.
- Tope recomendado.
- Referencias por división.
- Porcentaje de utilización.
- Alertas de riesgo.

---

# 🔄 Mercado de Fichajes

El mercado permite:

- Comprar jugadores.
- Vender jugadores.
- Recibir ofertas.
- Realizar negociaciones.
- Evaluar valores.
- Gestionar salarios.
- Considerar roles prometidos.
- Evaluar necesidades de plantilla.
- Incorporar talentos.

La IA también utiliza lógica de evaluación para decidir objetivos y prioridades de mercado.

---

# 📈 Desarrollo de Jugadores

La evolución de un futbolista puede depender de:

- Edad.
- Potencial.
- Rendimiento.
- Minutos disputados.
- Entrenamiento.
- Estado físico.
- Moral.

El proyecto también incluye sistemas de desarrollo juvenil y generación de nuevos talentos.

---

# 🏗 Arquitectura del Proyecto

La base de código se organiza por dominios.

```text
Juego/
│
├── assets/
├── data/
├── docs/
├── include/
├── saves/
├── src/
├── tests/
├── tools/
│
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── CHANGELOG.md
├── TODO.md
└── INDEX.md
```

## Principales módulos

```text
src/
├── ai/
├── career/
├── competition/
├── development/
├── engine/
├── finance/
├── gui/
├── io/
├── simulation/
├── transfers/
├── ui/
├── utils/
└── validators/
```

## Responsabilidades generales

### `engine/`

- Estado central.
- Controladores.
- Flujo principal.
- Runtime de carrera.
- Orquestación de alto nivel.

### `simulation/`

- Match Engine.
- Match Context.
- Eventos.
- Resolución.
- Estadísticas.
- Momentum.
- Valoraciones.
- Runtime de partido.

### `career/`

- CareerService.
- Temporadas.
- Semana de carrera.
- Liga y copa.
- Manager.
- Narrativas.
- Comunicaciones.
- Noticias.

### `ai/`

- Gestión del partido.
- Sustituciones.
- Adaptación táctica.
- Planificación de plantilla.
- Evaluación de fichajes.

### `finance/`

- Proyecciones.
- Flujo financiero.
- Masa salarial.
- Salary cap.

### `transfers/`

- Negociaciones.
- Valoraciones.
- Ofertas.
- Roles.
- Estrategia de mercado.

### `io/`

- Carga de datos.
- Guardado.
- Carga de partidas.
- Serialización.
- Manejo seguro de rutas.

### `gui/`

- Interfaz Win32.
- Dashboard.
- Match Center.
- Flujos visuales.

### `validators/`

- Integridad de datos.
- Calendarios.
- Guardados.
- Estructura de ligas.
- Auditorías.

---

# 🔄 Integración del Match Center

La integración del Match Center es bidireccional.

```text
Match Engine
    │
    ├──► Match Context
    ├──► Eventos
    ├──► Estadísticas
    ├──► Momentum
    ├──► Valoraciones
    ├──► IA táctica
    │
    ▼
Live Match Runtime
    ▲
    │
    └──── Win32 Match Center
```

El GUI observa el mismo runtime utilizado por el motor.

No existe una simulación paralela exclusiva de la interfaz.

El bridge interactivo permite enviar decisiones desde la GUI al partido en curso.

---

# 💻 Tecnologías utilizadas

| Tecnología | Uso |
|---|---|
| C++17 | Lenguaje principal |
| STL | Estructuras y utilidades |
| Win32 | Interfaz gráfica |
| CMake | Sistema de compilación |
| GCC / MinGW | Toolchain compatible |
| Ninja | Build system compatible |
| Git | Control de versiones |
| GitHub | Repositorio y releases |
| CTest | Ejecución de pruebas |

---

# 🔨 Compilación del Proyecto

Chilean Footballito utiliza **CMake**.

## Requisitos

- CMake 3.16 o superior.
- Compilador compatible con C++17.
- Git.
- En Windows, un toolchain GCC/MinGW o MSYS2 compatible.

El repositorio incluye:

- `CMakeLists.txt`
- `CMakePresets.json`
- `build.bat`

---

# 📥 Obtener el Proyecto

```bash
git clone https://github.com/felipe-urtubia/Juego.git
cd Juego
```

---

# ⚙️ Configuración

Puede utilizarse el preset incluido en el repositorio:

```bash
cmake --preset Juego-UCRT64-Ninja
```

También puede mantenerse un árbol independiente para desarrollo o CI.

---

# 🏗️ Compilación

Con `build-ci` previamente configurado:

```powershell
cmake --build .\build-ci
```

Targets principales:

- `FootballManager`
- `FootballManagerCLI`
- `FootballManagerTests`

---

# 🧪 Sistema de Pruebas

Para ejecutar la suite:

```powershell
ctest --test-dir .\build-ci --output-on-failure
```

En `v0.1.2.0-alpha`:

- **100% de los tests configurados aprobaron.**

## Áreas cubiertas

- Motor de simulación.
- Match Center.
- Decisiones combinadas de mentalidad e instrucción.
- Sustituciones manuales.
- Sustituciones de IA.
- Runtime interactivo.
- IA táctica.
- Validadores.
- Persistencia.
- Calendario.
- Economía.
- Desarrollo.
- Momentum.
- Valoraciones.
- Servicios de carrera.

---

# ✅ Validación de datos

La CLI incluye un modo de auditoría general:

```powershell
.\build-ci\bin\FootballManagerCLI.exe --validate
```

Resultado verificado para `v0.1.2.0-alpha`:

```text
Divisiones: 5
Equipos revisados: 90
Jugadores crudos: 2200
Errores: 0
Advertencias: 0
Resultado: sin fallas
```

---

# 📦 Paquete de Release

La release para Windows contiene únicamente lo necesario para jugar.

Estructura:

```text
FootballManager-v0.1.2.0-alpha/
├── assets/
├── data/
├── saves/
├── FootballManager.exe
└── LEEME.txt
```

No se incluyen:

- Tests.
- Ejecutable CLI.
- Código fuente compilado auxiliar.
- Guardados personales.
- Archivos de desarrollo.

---

# 🧭 Filosofía de Desarrollo

Cada feature debe seguir, idealmente, este flujo:

```text
Implementar
    │
    ▼
Compilar
    │
    ▼
Ejecutar pruebas
    │
    ▼
Validar
    │
    ▼
Corregir
    │
    ▼
Actualizar documentación
    │
    ▼
Commit
    │
    ▼
Integrar
```

Principios:

- Separar responsabilidades.
- Evitar lógica de negocio en GUI.
- Reducir duplicación.
- Mantener módulos testeables.
- Validar accesos e índices.
- Preferir APIs estructuradas.
- Documentar cambios relevantes.
- Verificar antes de publicar.

---

# 🗺️ Roadmap

El desarrollo posterior a `v0.1.2.0-alpha` parte de una base donde el Match Center interactivo, salary cap, KPIs y una parte importante de la modularización de carrera ya están implementados.

## Motor de Simulación

- Repeticiones y highlights.
- Clima dinámico.
- Árbitros.
- VAR.
- Eventos especiales.
- Fases y transiciones más profundas.
- Mayor balance estadístico.

## Match Center

- Más visualizaciones en vivo.
- Comparativas entre jugadores.
- Mayor feedback táctico.
- Mejoras del timeline.
- Accesibilidad y escalado.
- Más cobertura automática.
- Posible mini-juego de penales en el futuro.

## Inteligencia Artificial

- Entrenadores con personalidad más marcada.
- Adaptación táctica avanzada.
- Mejor rotación CPU.
- Estrategias según competición.
- Mejor planificación a largo plazo.

## Modo Carrera

- Conferencias de prensa.
- Patrocinadores.
- Relaciones con la directiva.
- Historial ampliado de entrenadores.
- Academia juvenil más profunda.
- Mayor separación de servicios semanales.

## Transferencias y Economía

- Negociaciones más profundas.
- Consecuencias de promesas.
- Mejor valoración de necesidades.
- Más herramientas de planificación financiera.

## Calidad

- Mantener CTest como requisito de release.
- Mantener `FootballManagerCLI --validate` como gate.
- Añadir simulaciones largas de balance.
- Añadir smoke tests del paquete final.
- Mantener README, CHANGELOG, TODO, INDEX y arquitectura sincronizados.

---

# 📚 Documentación

La documentación principal se organiza de la siguiente forma:

- `README.md` — descripción general y estado actual.
- `CHANGELOG.md` — cambios por versión.
- `TODO.md` — historial de desarrollo y verificaciones.
- `INDEX.md` — índice general.
- `docs/ARCHITECTURE.md` — arquitectura actual.
- `docs/ROADMAP.md` — prioridades futuras.
- `CODEBASE_ANALYSIS.md` — análisis técnico.
- `docs/superpowers/specs/` — diseños.
- `docs/superpowers/plans/` — planes de implementación.

Los siguientes archivos corresponden a auditorías históricas:

- `BUG_SUMMARY.md`
- `BUG_ANALYSIS_DETAILED.md`
- `BUG_FIXES_GUIDE.md`

Sus números de línea y estados no deben considerarse vigentes automáticamente sin volver a comprobar el código actual.

---

# 🤝 Contribuciones

Antes de integrar cambios se recomienda:

1. Compilar el proyecto.
2. Ejecutar todas las pruebas.
3. Ejecutar la validación cuando corresponda.
4. Mantener la arquitectura modular.
5. Añadir pruebas para nuevas funcionalidades.
6. Actualizar la documentación.
7. Revisar `git diff --check`.
8. Integrar únicamente después de verificar.

---

# 📝 Convenciones de commits

Ejemplos:

```text
feat(match): agregar nueva interacción
feat(ai): mejorar adaptación táctica
fix(simulation): corregir cálculo de estadísticas
refactor(career): extraer lógica a servicio
docs(readme): actualizar documentación
release: preparar nueva versión
```

---

# 📄 Licencia

La licencia del proyecto deberá definirse o actualizarse conforme evolucione el desarrollo.

Hasta entonces, consulta el repositorio para conocer las condiciones de uso y distribución.

---

# 🙌 Proyecto

Chilean Footballito es un proyecto en evolución continua.

Cada nueva versión busca mejorar:

- Realismo.
- Profundidad.
- Estabilidad.
- Mantenibilidad.
- Modularidad.
- Calidad de la interfaz.
- Capacidad de expansión futura.