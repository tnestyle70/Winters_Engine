ID: 2026-07-14_account_storefront_banpick_ownership
상태: Handoff
Agent: Codex
Owner: Desktop
Branch: main
Base: shared dirty worktree; S030 account/shop implementation is Handoff
Owned paths: Client/Private/Scene/Scene_Login.cpp; Client/Private/Scene/Scene_MainMenu.cpp; Client/Private/Scene/Scene_Shop.cpp; Client/Public/Scene/Scene_Shop.h; Client/Private/Scene/Scene_BanPick.cpp; Client/Public/Scene/Scene_BanPick.h; Client/Public/GamePlay/ChampionCatalog.h; Client/Private/GamePlay/ChampionCatalog.cpp; Client/Public/ClientShell/ClientShellDataStore.h; Client/Private/ClientShell/ClientShellDataStore.cpp; Services/internal/auth/service.go; Services/internal/auth/service_test.go; Services/internal/shop/repository.go; this packet; report doc
Read-only paths: Services auth/shop/profile routes and migration 000008; Client backend HTTP clients; champion visual definitions and portrait resources; unrelated dirty worktree paths
Validation: gofmt/go test ./...; numeric-ID register/login + storefront/purchase E2E; Client Debug x64 build; git diff --check; portrait/content-key coverage audit
Report: .md/build/2026-07-14_ACCOUNT_STOREFRONT_BANPICK_OWNERSHIP_REPORT.md
Handoff notes: Implemented and verified. ID `1` exists with 1000 RP and 17 products. Auth 8081 and Shop 8086 were restarted on the new code; Profile 8084 remains healthy. BanPick ownership is presentation-only so the all-champion validation flow remains selectable.
