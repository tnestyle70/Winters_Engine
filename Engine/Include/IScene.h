#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"

//이거 공개하는 게 맞음?
class WINTERS_ENGINE IScene abstract //객체 생성 불가 명시!
{
public:
	virtual ~IScene() = default;

	virtual bool OnEnter() = 0;
	virtual void OnExit() = 0;
	virtual void OnUpdate(f32_t dt) = 0;
	virtual void OnLateUpdate(f32_t dt){}
	virtual void OnRender() = 0;
	virtual void OnImGui() {}
};