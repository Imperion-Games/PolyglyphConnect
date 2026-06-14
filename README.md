# PolyglyphConnect

Unreal Engine editor plugin that connects a project to the **Polyglyph** localization
service. It gathers source text, pushes it for AI translation plus human review, and
pulls approved translations back into the engine's native localization.

Developed against **Unreal Engine 5.7** (the engine used for the rest of the ToaGames
plugin ecosystem). Editor-only: nothing ships into the packaged game runtime.

## Status

Early scaffold. What exists:

- Plugin descriptor + Editor module that registers a **Polyglyph** menu under
  `Tools` in the level editor.
- `UPolyglyphSettings` (Editor Preferences) holding the API base URL, project slug,
  and per-user API key (the key is stored in per-user editor settings and is never
  committed to the project).
- `FPolyglyphClient`, an HTTP client wrapping the Polyglyph plugin API. Implemented:
  `TestConnection` (GET `/api/plugin/status`) and `PushStrings` (POST `/api/plugin/push`).
- Menu actions: **Test Connection** and **Push Source Strings** (the latter currently
  pushes a small probe payload until the localization gather is wired in).

## Not yet built (see the Brain notes)

- Gather real source text from the UE Localization pipeline (manifest / string tables)
  to feed `PushStrings`.
- Pull and import translations (`/api/plugin/pull`, `/api/plugin/export?format=po`).
- Trigger translation jobs (`/api/plugin/translate`) and poll status.
- A Slate panel for project/language selection and progress.
- A commandlet for headless CI runs.

## Setup

1. Enable the plugin for your project (Plugins browser, or add it to the `.uproject`).
2. In the Polyglyph dashboard, create an API key and note your project slug.
3. `Edit > Editor Preferences > Plugins > Polyglyph`: set the API base URL, project
   slug, and paste your API key.
4. `Tools > Polyglyph > Test Connection` to confirm the link.

Design notes, the full endpoint map, and the build-out plan live in the Brain:
`Knowledge/Brain/Plugins/PolyglyphConnect/`.
