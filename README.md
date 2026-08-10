# PolyglyphConnect

Unreal Engine editor plugin that connects a project to the **Polyglyph** localization
service. It gathers source text, pushes it for AI translation plus human review, and
pulls translations back into the engine's native localization. Pulls are approved-only
by default. **Include unapproved drafts** in Project Settings can be enabled for testing;
draft translations have not been reviewed and should not ship.

Developed against **Unreal Engine 5.7** (the engine used for the rest of the ToaGames
plugin ecosystem). Editor-only: nothing ships into the packaged game runtime.

## How it works

Polyglyph registers as a **Localization Service Provider**, so it appears in the service
picker of Unreal's own Localization Dashboard rather than adding a separate panel. Select
it, and every target's toolbar gains four actions:

| Action | What it does |
|---|---|
| **Push Source** | Gathers the target's manifest and uploads its source strings. Dialogue keys are enriched automatically (below). |
| **Translate** | Starts an AI translation job for every enabled language. |
| **Pull Translations** | Downloads translations, imports them into the culture archive, and compiles `.locres`. No separate Compile Text step. |
| **Open in Polyglyph** | Opens the web dashboard for review, glossary, and coverage. |

The loop is: **Push Source** → **Translate** → review and approve in Polyglyph →
**Pull Translations**.

**Who owns what.** Unreal owns source strings; they only ever flow up. Polyglyph owns
translations and the language roster; they only ever flow down. Archives and `.locres`
are generated artifacts: commit them, but regenerate by pulling rather than hand-editing.
Editing translations in Unreal's Translation Editor will not upload, and the next pull
overwrites them.

## Approved translations and drafts

Pull takes **approved translations only** by default, which is what you want for anything
that ships. A translation sitting in Polyglyph's review queue is not approved yet, so a
culture with only drafts imports nothing and the plugin tells you exactly that, including
how many are waiting.

To pull drafts as well, enable **Include unapproved drafts** in
`Project Settings > Plugins > Polyglyph`. Useful for seeing real text in-game before
review finishes. Draft translations have not been reviewed and should not ship.

## Dry runs (no AI cost)

`-mock` on the sync commandlet round-trips the whole pipeline with placeholder text
instead of calling a model, so it spends nothing. Use it to validate a new setup or as a
CI smoke test:

```text
UnrealEditor-Cmd <Project>.uproject -run=PolyglyphSync -push -translate -pull -mock
```

This is currently commandlet-only; the Localization Dashboard's **Translate** button
always runs a real, billed job.

## Headless / CI

`UPolyglyphSyncCommandlet` runs the same pipeline without the editor UI. The **gather**
stays a separate step, since Unreal owns it:

```text
UnrealEditor-Cmd <Project>.uproject -run=GatherText -config="Config/Localization/Game_Gather.ini"
UnrealEditor-Cmd <Project>.uproject -run=PolyglyphSync -push -translate -pull
```

Switches: `-push -translate -pull -wait -mock -mode=sync|batch|auto`. Supply the API key
via the `POLYGLYPH_API_KEY` environment variable (never on the command line in CI);
`-BaseUrl=` and `-ProjectSlug=` override the project settings.

`UPolyglyphEnrichCommandlet` imports a `namespace,key,character,gender,register,maxLength,context`
CSV for translator context that the manifest cannot carry. `-strict` exits non-zero when a
row matches no key, which makes a useful CI gate.

## Translator context

Polyglyph translates better when it knows who is speaking. Two channels feed it:

- **Automatic.** Native `DialogueWave` assets already carry speaker, gender, plurality,
  and voice direction in their manifest metadata. Push reads it and sends it with no
  extra step.
- **Manual.** For custom dialogue systems, UI length limits, or anything DialogueWave
  does not cover, use the enrich commandlet's CSV.

Character voice and description are authored in the Polyglyph dashboard, not here.

## Setup

1. Enable the plugin for your project (Plugins browser, or add it to the `.uproject`).
2. In the Polyglyph dashboard, create an API key and note your project slug.
3. `Edit > Project Settings > Plugins > Polyglyph`: set the API base URL, project
   slug, and (if you use a non-default target) the localization target name. These are
   shared, project-wide settings committed to source control.
4. `Edit > Editor Preferences > Plugins > Polyglyph`: paste your API key. This is a
   per-user setting stored outside the project and never committed.
5. `Tools > Polyglyph > Test Connection` to confirm the link.

Design notes, the full endpoint map, and the build-out plan live in the Brain:
`Knowledge/Brain/Plugins/PolyglyphConnect/`.

## Locale mappings

The plugin ships a complete Unreal Engine 5.7 mapping catalog at
`Resources/LocaleMappings/Unreal-5.7.json`. Each entry keeps Unreal's exact external
culture code alongside Polyglyph's canonical BCP 47 tag and an English display name.

The Localization Dashboard target only selects entries from that catalog. It does not
silently guess regional or script variants: `pt` never becomes `pt-PT`, and `zh` never
picks a script for you. A project can add or replace entries with a partial JSON mapping
file through **Project Settings > Plugins > Polyglyph > Locale Mapping Overrides File**.
Relative paths are resolved from the project directory.

A language invented for your game has no standard code, so it uses a private-use tag:
`x-` followed by a label you choose. That is a real BCP 47 identity no registry defines,
which is exactly what an in-game language needs. Give it a display name and it moves
through the pipeline like any other locale.

Use this format for an override file:

```json
{
  "schemaVersion": 1,
  "integration": "unreal",
  "mappings": [
    {
      "externalCode": "DRACONIC",
      "localeTag": "x-draconic",
      "displayName": "Draconic"
    }
  ]
}
```

To regenerate the complete catalog for a new engine version, run:

```text
UnrealEditor-Cmd.exe <Project>.uproject -run=PolyglyphLocaleMap -Output=<path-to-json>
```

## License

PolyForm Internal Use License 1.0.0, see [LICENSE.md](LICENSE.md). In short: use and
modify it freely inside your own studio, for your own projects. You may not
redistribute it, sublicense it, or resell it, modified or not, to anyone outside your
company.
