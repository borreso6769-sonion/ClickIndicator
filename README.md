# Click Indicator (Geode)

Graba tus clicks en **modo práctica** y luego te muestra dónde clickear:

- **Línea blanca** → dónde empezar el click.
- **Barra verde** → cuánto tiempo mantener el click (hold).

No es un bot: no juega por ti, solo dibuja guías.

## Uso

1. Entra a un nivel, activa el **modo práctica** y pausa.
2. Pulsa la tecla **O** dentro del nivel (otra vez para cerrar), o el botón **Click Indicator**
   (abajo a la derecha del menú de pausa; es la única forma en móvil).
3. **Empezar** → reanuda y juega en práctica. El botón cambia a **Detener**.
   - Si mueres, los clicks posteriores al checkpoint se descartan automáticamente.
4. Al terminar (o con **Detener**), abre el menú y pulsa **Guardar** → pon un nombre.
5. **Guardados** (botón de abajo) → lista de grabaciones de ese nivel; toca una para seleccionarla.
6. **Usar** → muestra las líneas en el nivel (vuelve a pulsarlo para ocultarlas).
7. **Borrar** → elimina la grabación seleccionada.

Las grabaciones se guardan en `<carpeta de guardado del mod>/recordings/` como `.txt`.

## Compilar

Necesitas el [Geode SDK](https://docs.geode-sdk.org/getting-started/) y la CLI:

```
geode build
```

O sube la carpeta a un repositorio de GitHub: el workflow de `.github/workflows/build.yml`
compila para Windows, Android, macOS e iOS y te deja el `.geode` en *Actions → Artifacts*.

Antes de publicar, cambia `developer` e `id` en `mod.json`.
