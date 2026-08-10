# PolyglyphConnect

Unreal Engine editor plugin that connects a project to the **Polyglyph** localization
service. It gathers source text, pushes it for AI translation plus human review, and
pulls translations back into the engine's native localization. Pulls are approved-only
by default. **Include unapproved drafts** in Project Settings can be enabled for testing;
draft translations have not been reviewed and should not ship.

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

## Locale mappings

The plugin ships a complete Unreal Engine 5.7 mapping catalog at
`Resources/LocaleMappings/Unreal-5.7.json`. Each entry keeps Unreal's exact external
culture code alongside Polyglyph's canonical BCP 47 tag and an English display name.

The Localization Dashboard target only selects entries from that catalog. It does not
silently guess regional or script variants. A project can add or replace entries with a
partial JSON mapping file through **Project Settings > Plugins > Polyglyph > Locale
Mapping Overrides File**. Relative paths are resolved from the project directory.

Use this format for an override file:

```json
{
  "schemaVersion": 1,
  "integration": "unreal",
  "mappings": [
    {
      "externalCode": "STUDIO_ELVISH",
      "localeTag": "x-studio-elvish",
      "displayName": "Studio Elvish"
    }
  ]
}
```

To regenerate the complete catalog for a new engine version, run:

```text
UnrealEditor-Cmd.exe <Project>.uproject -run=PolyglyphLocaleMap -Output=<path-to-json>
```
