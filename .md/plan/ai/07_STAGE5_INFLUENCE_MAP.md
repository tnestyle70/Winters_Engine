# Stage 5 — Influence / Threat / Opportunity / Vision Map

## 목표

맵을 그리드로 분할해 **공간 기반 의사결정**에 쓰이는 여러 레이어 점수 관리.

## 왜 Map 시스템인가

- "어디로 가야 안전한가?", "어디가 갱 기회인가?" 같은 질문은 좌표 기반 평가 필요
- Utility 공식에 넣을 수 있는 연속적 입력 제공
- Warcraft III / StarCraft II / Civilization 등 업계 표준 기법

## 그리드 구조

```cpp
// 소환사의 협곡 ≈ 140m × 140m, 셀 1m 기준 140×140 = 19,600 셀
// 과하면 2m 셀 70×70 = 4,900 셀로 축소
constexpr i32_t MAP_GRID_W = 140;
constexpr i32_t MAP_GRID_H = 140;
constexpr f32_t CELL_SIZE  = 1.0f;  // meters

class CGridMap
{
public:
    void Set(i32_t x, i32_t y, f32_t value);
    f32_t Get(i32_t x, i32_t y) const;
    
    // 가우시안 블러 (영향력 확산)
    void Propagate(f32_t decay, i32_t iterations);

    // 월드 좌표 → 셀
    std::pair<i32_t, i32_t> WorldToCell(const Vec3& worldPos) const;

private:
    std::array<f32_t, MAP_GRID_W * MAP_GRID_H> m_data{};
};
```

## 레이어 종류

### 1. Team Influence Map (아군/적군 영향력)

각 챔피언 위치에서 거리 감쇠 가우시안. 아군 = +값, 적군 = -값.

```cpp
void UpdateTeamInfluence(CGridMap& map, const std::vector<EntityID>& champions)
{
    map.Clear();
    for (auto ch : champions) {
        const auto& pos = GetPosition(ch);
        f32_t sign = IsAlly(ch) ? +1.f : -1.f;
        f32_t strength = GetEffectivePower(ch);  // 레벨/아이템/체력

        auto [cx, cy] = map.WorldToCell(pos);
        for (i32_t dy = -RADIUS; dy <= RADIUS; ++dy)
            for (i32_t dx = -RADIUS; dx <= RADIUS; ++dx) {
                f32_t d = std::sqrt(dx*dx + dy*dy);
                if (d > RADIUS) continue;
                f32_t falloff = std::exp(-d * d / (2 * SIGMA * SIGMA));
                map.Set(cx+dx, cy+dy, map.Get(cx+dx, cy+dy) + sign * strength * falloff);
            }
    }
}
```

### 2. Threat Map (위협도)

적이 **도달/공격 가능한** 영역. 시야 + 사거리 + CC + 소환사 주문 고려.

```
Threat(cell) = Σ over enemies:
    · baseThreat(enemy)
    · reachability(enemy.pos → cell, with_flash, with_dash)
    · enemyCCAvailable
    · 1 if cell in enemy_vision else 0.5
```

### 3. Opportunity Map (기회)

오브젝트 근처 + 고립된 적 근처 + 미니언 밀림 예정 라인.

```
Opportunity(cell) = 
    · dragonPit  if dragonUp && timeToSpawn < 30
    · baronPit   if baronUp
    · gankPoint  if enemy_isolated(cell)
    · wavePushPoint if our_minions_approaching_enemy_tower
```

### 4. Vision Map (시야)

시야 있는 셀 = 1, 안개 = 0. 와드/상대 시야/지형 고려.

```
Vision(cell) = 
    max over my_team:
        1 if distance(source, cell) < visionRadius(source) && no_wall_blocking else 0
```

### 5. Terrain Map (정적)

지형 높이, 벽, 부쉬, 이동 불가 영역. 한 번 precompute.

## 레이어 합성 (Map Fusion)

봇이 목적지 선정 시 여러 레이어 가중 합.

```cpp
f32_t EvaluateMoveDestination(i32_t cellX, i32_t cellY)
{
    f32_t score = 0.f;
    score += 0.4f * opportunityMap.Get(cellX, cellY);
    score -= 0.5f * threatMap.Get(cellX, cellY);
    score += 0.2f * visionMap.Get(cellX, cellY);     // 시야 있는 곳 선호
    score += 0.3f * teamInfluenceMap.Get(cellX, cellY);  // 아군 영향권 선호
    return score;
}
```

## 성능 최적화

### 업데이트 주기

- **Team Influence**: 200ms (챔피언 이동 속도 고려)
- **Threat Map**: 200ms
- **Opportunity Map**: 500ms (오브젝트 타이머 기반)
- **Vision Map**: 100ms (빠른 변화)
- **Terrain Map**: 게임 시작 시 한 번

### GPU Compute 활용

그리드 셀 수 × 레이어 수 × 챔피언 수 = 부담. Compute Shader 로 병렬화.

```hlsl
// Shaders/AI/InfluenceMap.hlsl
[numthreads(8, 8, 1)]
void UpdateInfluence(uint3 id : SV_DispatchThreadID)
{
    int2 cell = int2(id.xy);
    float total = 0;
    for (int i = 0; i < g_championCount; ++i) {
        float3 pos = g_championPositions[i];
        float2 cellWorld = CellToWorld(cell);
        float d = distance(cellWorld, pos.xz);
        total += g_championInfluence[i] * exp(-d*d / (2 * g_sigma * g_sigma));
    }
    g_influenceOutput[id.y * GRID_W + id.x] = total;
}
```

ECS 시스템에서 ConstantBuffer 에 챔피언 위치/영향력 업로드 → Dispatch.

### 감쇠 블러 (빈번 업데이트 대안)

매번 재계산 안 하고, 기존 값에 디케이 + 델타만 추가:
```
map[t+1] = map[t] * 0.95 + delta(player_moves)
```

## 영향력 확산 (Propagation)

단순 가우시안 말고 A* 기반 "도달 시간" 확산:
```
cell_influence = championInfluence × exp(-travelTime(champion → cell) / tau)
```

벽/부쉬 고려해 실제 이동 경로의 거리 씀. 더 현실적이지만 비용 높음.

## 시각화 (ImGui + DebugDraw)

- ImGui 창에 선택된 레이어 히트맵 (ImPlot 또는 ImGui::ImageButton 으로)
- DebugDraw 로 월드 공간에 셀 색칠 오버레이
- 최고 opportunity 셀 초록, 최고 threat 셀 빨강, 현재 봇 목적지 노랑

```cpp
void CInfluenceMapDebugWindow::Render()
{
    ImGui::Begin("Influence Map");
    
    const char* layers[] = { "Team", "Threat", "Opportunity", "Vision" };
    ImGui::Combo("Layer", &m_selectedLayer, layers, IM_ARRAYSIZE(layers));
    
    // 텍스처에 값 매핑 후 표시
    UploadGridToTexture(m_selectedLayer);
    ImGui::Image(m_textureSRV, ImVec2(280, 280));
    
    ImGui::End();
}
```

## ECS 통합

```cpp
// 싱글턴 리소스 (한 World 에 하나)
struct InfluenceMapResource
{
    CGridMap teamInfluence;
    CGridMap threat;
    CGridMap opportunity;
    CGridMap vision;
};

class CInfluenceMapSystem : public ISystem
{
public:
    void Execute(CWorld& world, f32_t dt) override
    {
        m_updateAcc += dt;
        if (m_updateAcc < 0.2f) return;
        m_updateAcc = 0.f;

        auto& res = world.GetResource<InfluenceMapResource>();
        UpdateTeamInfluence(res.teamInfluence, world);
        UpdateThreatMap(res.threat, world);
        UpdateOpportunityMap(res.opportunity, world);
        UpdateVisionMap(res.vision, world);
    }

private:
    f32_t m_updateAcc = 0.f;
};
```

봇은 `MapAwarenessComponent` 에 자주 쓰는 값 캐시 (매 프레임 전체 맵 쿼리 비용 절감).

## 구현 순서

1. `CGridMap` 기본 (Get/Set/Clear/WorldToCell)
2. 가우시안 전파 CPU 버전
3. Team Influence + Threat Map 먼저 (가장 중요)
4. Opportunity / Vision 추가
5. Compute Shader 로 이식 (성능 문제 발생 시)
6. ImGui 히트맵 디버거
7. Utility Stage 4 와 연동 (Goal 점수 함수에 맵 값 입력)
