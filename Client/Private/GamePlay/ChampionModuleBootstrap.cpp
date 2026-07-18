#include "GamePlay/ChampionModuleBootstrap.h"
#include "GamePlay/SkillRegistry.h"

extern void Irelia_KeepAlive();
extern void Ezreal_KeepAlive();
extern void Fiora_KeepAlive();
extern void Jax_KeepAlive();
extern void Annie_KeepAlive();
extern void Ashe_KeepAlive();
extern void Viego_KeepAlive();
extern void Yone_KeepAlive();
extern void Yasuo_KeepAlive();
extern void Kalista_KeepAlive();
extern void Garen_KeepAlive();
extern void Zed_KeepAlive();
extern void Riven_KeepAlive();
extern void Kindred_KeepAlive();
extern void LeeSin_KeepAlive();
extern void MasterYi_KeepAlive();
extern void SylasKeepAlive();

void BootstrapChampionModules()
{
    Irelia_KeepAlive();
    Ezreal_KeepAlive();
    Fiora_KeepAlive();
    Jax_KeepAlive();
    Annie_KeepAlive();
    Ashe_KeepAlive();
    Viego_KeepAlive();
    Yone_KeepAlive();
    Yasuo_KeepAlive();
    Kalista_KeepAlive();
    Garen_KeepAlive();
    Zed_KeepAlive();
    Riven_KeepAlive();
    Kindred_KeepAlive();
    LeeSin_KeepAlive();
    MasterYi_KeepAlive();
    SylasKeepAlive();
    (void)CSkillRegistry::Instance().AuditDataDrivenContracts();
}
