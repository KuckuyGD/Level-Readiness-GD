# Level Readiness

Añade un botón **?** a las pantallas de niveles de Geometry Dash.

- **Niveles oficiales:** el botón aparece arriba a la derecha de cada página del selector de niveles principales.
- **Niveles de la comunidad:** el botón intenta colocarse junto al nombre del nivel.
- Al pulsarlo, muestra un **índice de preparación de 1/100 a 100/100**.

## Qué usa la v0.1

El cálculo usa datos locales del juego:

- estrellas totales;
- demons completados;
- niveles online completados;
- niveles oficiales completados;
- intentos y saltos globales;
- dificultad/estrellas del nivel;
- subtipo Demon cuando está disponible;
- progreso normal y práctica en ese nivel;
- intentos hechos en ese nivel.

El número es un **índice heurístico de preparación**, no una probabilidad matemática garantizada de completar el nivel.

## Limitaciones de v0.1

- No analiza todavía la geometría interna del nivel, timings, velocidades, gamemodes ni chokepoints.
- Los niveles sin rating tienen menos información, así que la confianza mostrada es menor.
- En niveles Platformer se reduce la confianza porque el progreso no se interpreta exactamente igual que en niveles clásicos.

## Compilación

Con Geode SDK y Geode CLI configurados:

```sh
geode build
```

El paquete `.geode` aparecerá en la carpeta de build correspondiente.
