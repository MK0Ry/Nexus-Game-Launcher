# Nexus Game Launcher

Nexus is a local-first Windows desktop app for organizing a personal PC game library and launching games from one place.

## Run & Operate

- `pnpm --filter @workspace/api-server run dev` — run the API server (port 5000)
- `pnpm run typecheck` — full typecheck across all packages
- `pnpm run build` — typecheck + build all packages
- `pnpm --filter @workspace/api-spec run codegen` — regenerate API hooks and Zod schemas from the OpenAPI spec
- `pnpm --filter @workspace/db run push` — push DB schema changes (dev only)
- Required env: `DATABASE_URL` — Postgres connection string
- Windows desktop setup, build, tests, and packaging instructions: `apps/nexus-launcher/README.md`
- Windows build: from `apps/nexus-launcher`, run `cmake --preset windows-msvc` then `cmake --build --preset windows-debug`
- Desktop tests: `ctest --preset windows-debug` from `apps/nexus-launcher`

## Stack

- Windows desktop app: C++20, Qt 6 Widgets, CMake, SQLite via Qt SQL
- pnpm workspaces, Node.js 24, TypeScript 5.9 (workspace template services)
- API: Express 5
- DB: PostgreSQL + Drizzle ORM
- Validation: Zod (`zod/v4`), `drizzle-zod`
- API codegen: Orval (from OpenAPI spec)
- Build: esbuild (CJS bundle)

## Where things live

- `apps/nexus-launcher/` — standalone Windows desktop app source, CMake project, tests, and build guide
- `apps/nexus-launcher/src/ui/` — main window and game entry dialog
- `apps/nexus-launcher/src/data/` — SQLite schema and game repository
- `apps/nexus-launcher/src/launcher/` — safe direct-process launching and play-session tracking
- `apps/nexus-launcher/src/app/` — JSON settings persistence

## Architecture decisions

- The desktop app does not depend on the workspace API or an account; its library and settings are local.
- Game executables are started with `QProcess` arguments, never through a shell.
- Removing a game removes its Nexus record only, not files from the installation folder.
- Nexus leaves running games open when it exits; playtime tracking ends when the launcher closes.

## Product

The initial milestone provides persistent appearance settings, manual library management with filters and JSON import/export, favorites, and direct game launching with process status and playtime history. Metadata providers, launcher-client detection, folder scanning, and performance monitoring remain future stages.

## User preferences

The initial delivery should follow the attached Project Nexus brief and implement stages 1–3 before the remaining stages.

## Gotchas

- The Qt desktop app is a native Windows project and does not run in the browser preview.
- Use `apps/nexus-launcher/README.md` as the source of truth for Qt/MSVC dependency setup and packaging.

## Pointers

- See the `pnpm-workspace` skill for workspace structure, TypeScript setup, and package details
