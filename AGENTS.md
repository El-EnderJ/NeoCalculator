# NumOS: instrucciones del proyecto

## Emulador siempre disponible (petición permanente del usuario, 2026-10-02)

- Mantén **en todo momento**, también mientras trabajas, la última versión
  funcional en `C:\.piobuild\numOS\emulator_pc\program.exe`, con sus DLL necesarias.
  El usuario debe poder ejecutar `.\scripts\run-emulator-windows.ps1` y probarla
  sin esperar a que termines.
- Al iniciar una tarea, comprueba esa ruta. Si falta, restaura primero una versión
  ya validada con sus runtimes y comprueba su arranque antes de continuar.
- Esa carpeta contiene la **versión publicada**, no archivos de compilación.
  Nunca la borres, vacíes, limpies, muevas ni uses como salida de PlatformIO.
  Tampoco uses `C:\.piobuild\numOS` ni ningún antecesor de la versión publicada
  como `build_dir`: el auto-clean de PlatformIO puede borrarlo entero al cambiar
  la configuración o añadir una cabecera.
- Compila en `C:\.piobuild\numOS-build` (configuración predeterminada) o en otra
  carpeta aislada que no contenga la versión publicada. Revisa también cualquier
  `PLATFORMIO_BUILD_DIR` heredado y las configuraciones `.ini` temporales.
- Prueba cada candidato en su carpeta aislada. Publícalo únicamente después de
  compilar y validar, mediante `scripts/publish-emulator-windows.ps1
  -BuiltExecutable <ruta-al-candidato>`. El script comprueba los runtimes y el
  cálculo 2+3, sustituye el ejecutable de forma atómica y conserva una copia previa.
- Si falla una compilación, prueba o publicación, conserva la versión funcional.
  Si Windows bloquea la sustitución porque está abierta, no cierres el emulador
  del usuario: conserva la versión activa y reintenta la publicación cuando se
  cierre. Nunca crees un intervalo sin `program.exe` para forzar la actualización.
- Antes de terminar, prueba el lanzador habitual y comunica si ya está publicada
  la versión nueva o si sigue disponible la anterior por estar en uso.

## Preservación del trabajo

Conserva los cambios existentes del usuario y de otras tareas. Las instrucciones
de arquitectura C++/ESP32 de las instrucciones globales también se aplican aquí.

## Commit attribution

Applies to this repository and every worktree of it, with no exceptions:

- Never add a `Co-Authored-By:` trailer to any commit, for Claude or any other model.
- Never add "Generated with Claude Code" or any similar line to commit messages, PR descriptions or code comments.
- Every commit has exactly one author and one committer: the git identity already configured in this repo (`git config user.name` / `user.email`). Never set, override or invent an identity; if none is configured, stop and ask.
