

struct ShadowCloneComponent
{
	EntityID owner = NULL_ENTITY;
	eChampion ownerChampion = eChampion::END;
	f32_t fRemainingSec = 0.f;
	bool_t bCanCastMirrorSkill = false;
	bool_t bCanSwapPosition = false;
};