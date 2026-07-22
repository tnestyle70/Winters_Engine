# 2026-07-19 정글 오브젝트 종료 압력·버프·F4 Balance Lab 결과

## 1. 예측 vs 실측

### 수치와 권위 경계

| 계약 | 계획값 | 실측/반영 |
|---|---:|---|
| Baron/Elder 기본 내구도 | HP 10,000, Armor/MR 30 | `SpawnObjectGameplayDefs.json`과 생성 pack 일치 |
| Baron/Elder 팀 보상 | 5명 × 2,000 gold, 각 +3 level | 결정적 EntityID 순회로 5명 전원 지급, 18레벨 clamp |
| 특수 버프 지급 | 처치 시 살아 있는 팀원만 | 죽은 팀원은 gold/level만 받고 buff component 없음 |
| Baron/Elder/Blue/Red 지속 | 300초 | 같은 종류 재획득 시 stack 없이 expire tick만 갱신, 죽으면 즉시 제거 |
| Flash | 20초 | Summoner JSON/생성 pack 쿨다운 20초 |
| Elder 공격력 | 최종 총 AD ×1.7 | StatSystem 최종 계산 뒤 배율 적용, 만료/죽음 시 원복 |
| Elder 화상 | 3초, 1초마다 대상 max HP 1% | 같은 source/type은 중첩하지 않고 duration/next tick 갱신 |
| Elder 처형 | 유효 피해 후 HP 20% 이하 | 두 번째 DamageRequest 없이 같은 DamageResult를 execute로 승격, kill/reward 1회 |
| Baron 귀환 | 기본 시간 ×0.5 | authoritative recall duration query에 반영 |
| Baron 미니언 | 12m, HP×3, AD×2, 표시 크기×2 | lane minion만 적용, 진입/이탈 시 HP 비율 보존 원복, collider/path scale 불변 |
| Blue/Red | mana +10/s, HP +10/s | resource cap을 넘지 않는 tick regen |
| Red 기본 공격 화상 | 3초, 1초마다 magic 10 | 기본 공격에만 부착, 같은 source/type refresh |
| 일반 정글 보상 | killer 80 gold/240 XP | Baron/Dragon 제외 일반 camp에 killer-only 적용 |

서버 권위 흐름은 `DamageQueueSystem -> ExperienceSystem reward -> ObjectiveBuffComponent -> snapshot/event -> client visual`로 닫았다. Elder 처형은 기존 피해 결과를 승격해 kill feed, score, 보상 hook이 두 번 도는 경로를 만들지 않았다. Baron minion 강화는 authoritative HP/AD만 바꾸고, 2배 크기는 snapshot의 `visualScaleMultiplier`를 소비하는 render matrix에만 적용해 충돌 반경과 내비게이션을 건드리지 않았다.

### 반영 구조

- 데이터: Economy에 objective 18개 필드, 일반 camp 80/240, Flash 20초를 두고 Spawn JSON에 Baron/Dragon 10000/30/30을 반영했다. generator, schema, runtime overlay가 같은 필드를 읽는다.
- GameSim: objective buff/burn/Baron minion 상태를 keyframe 등록 컴포넌트로 만들고 Buff, Damage, Experience, Stat, Recall 경로에 연결했다. death cleanup은 같은 tick에 실행된다.
- Server/F4: 활성 `ChampionTuner`가 Champions/Skills/Spawn/Economy 네 JSON을 한 번에 검증·원자 저장·hot reload한다. Objectives 탭에서 camp HP/AD/Armor/MR와 모든 objective 수치를 조절한다. `Refill HP`, `Reset at Camp`, `Clear Objective Buffs`는 별도 practice command로 서버에 전달된다.
- reset 의미: refill은 살아 있는 대상 HP만 채운다. reset은 같은 entity를 JSON stat/anchor/AI 초기 상태로 돌리며 처치 보상이나 버프를 새로 지급하지 않는다. clear는 모든 특수 버프·DoT·강화 미니언을 원복한다.
- 네트워크/시각: Snapshot에 append-only `objectiveStateFlags`와 `visualScaleMultiplier`를 추가했다. Client는 full snapshot/rebase/death에서 persistent cue를 upsert/prune한다. Baron/Elder/Blue/Red 표식, Baron minion 보라 표식, Elder execute breath/burst WFX 6개를 추가했다.
- hot reload: 강화 미니언을 먼저 HP 비율 보존 원복하고 새 base definition을 적용한 뒤 aura를 재평가한다. 정글 몬스터 stat도 현재 HP 비율을 보존하며, refill만 전량 회복한다.

### Dragon 애니메이션 감사

`dragon_air_textured.wskel`과 `sru_dragon_flying_attack1.wanim`을 WINT 바이너리 계약으로 읽은 결과는 다음과 같다.

```text
channels=106
events=0
skeletonHash=0xb70d94d05260c98c (mesh skeleton과 clip trailer 일치)
ticksPerSecond=1000
durationTicks=1966.667 (약 1.967초)
runtime mapping=sru_dragon_flying_attack1
hitReactionClip=false
deathClip=false
```

판정은 “공격 클립 베이크와 BasicAttack 연결은 정상”이다. 단, 클립에 타격 이벤트가 없으므로 실제 피해 시점은 서버 windup이 정하며, 현재 Dragon asset에는 피격 리액션과 죽음 전용 clip이 없다. 따라서 “피격 시 반응 애니메이션까지 정상”이라고는 판정할 수 없다. 이를 `Test-BasicAttackTimingContract.py`의 exact path/skeleton/mapping 계약으로 고정했다.

### 검증 실측

| 검증 | 결과 |
|---|---|
| definition pack `--check` | PASS, hash `0x07259E27` |
| `Test-F4BalanceContracts.py` | PASS |
| `Test-BasicAttackTimingContract.py` | PASS, champion 17 + Dragon bake |
| objective WFX JSON/name/texture/beam-burst 검사 | PASS, 6/6 |
| GameSim Debug x64 build | PASS |
| Server Debug x64 build | PASS |
| Client Debug x64 build | PASS, 기존 C4251/C4275 warning만 존재 |
| SimLab Debug x64 build | PASS |
| `SimLab --objective-buffs-only` | PASS, exit 0 |
| `git diff --check` | PASS, line-ending 안내만 존재 |
| 전체 `Verify-LoLDataDrivenPipeline.ps1` | 네 프로젝트 build까지 PASS, 일반 SimLab에서 Ezreal FormulaData 1건으로 exit 1 |
| `SimLab --f4-balance-only` | wire/minion/cooldown reload PASS, 같은 Ezreal FormulaData 때문에 전체 exit 1 |

Ezreal 실패는 objective table 오염이 아니다. 현 작업 트리의 `skill.ezreal.q` rank 3이 `flat=200`, `totalAd=5.0`으로 변경되어 probe의 기존 기대값 `70`, `1.3`과 다르다. 이 세션 범위 밖의 사용자 변경이므로 되돌리거나 수치를 임의 교정하지 않았다.

실제 F5 화면 캡처는 수행하지 못했다. 따라서 지속 표식의 화면 크기, Elder breath의 모델 접점, Baron minion의 보라색 가독성, Dragon 공격 모션과 서버 피해 프레임의 체감 정렬은 미검증이다.

### 사용자 프롬프트 냉정한 비평

좋았던 점은 “끝나지 않는 경기를 끝내는 오브젝트”라는 제품 목적을 먼저 찾았고, F4에서 refill/reset하며 밸런스를 반복 측정해야 한다는 운영 요구까지 제시했다는 것이다. 이 덕분에 단순 버프 추가가 아니라 `게임 종료 압력 + 반복 가능한 밸런스 랩`으로 설계할 수 있었다.

문제는 최초 프롬프트가 아이디어, 확정 수치, 시각 연출, 툴 요구, 이전 버그를 한 덩어리로 섞었다는 점이다. 특히 `10000골드/2000씩`의 roster 의미, Dragon 모델과 authoritative Elder identity의 관계, 화상 damage/tick, aura 반경·대상, 죽음/재획득/중첩 규칙, refill과 respawn의 차이, WFX 합격 화면이 불명확했다. 그대로 구현했다면 서로 다른 에이전트가 각각 다른 정답을 만들 가능성이 높았다. “Dragon 모델이 Elder다”도 현재 자산 식별에는 충분하지만, 향후 일반 Dragon 추가 시 `subKind=Dragon == Elder`라는 기술 부채가 된다.

더 좋은 프롬프트는 다음 순서다: ① 반드시 통과해야 할 gameplay acceptance matrix, ② 확정 수치와 아직 tuning인 수치 분리, ③ authority/대상/죽음/refresh/reset 규칙, ④ F4 조작 목록, ⑤ 시각 reference와 캡처 합격 기준, ⑥ 이번 세션에서 제외할 이전 버그. 이번 후속 답변은 ①~③을 많이 보완했고 구현 가능성을 크게 높였다.

## 2. 판결

**수정 반영.** 계획의 서버 권위 gameplay, 데이터/F4, snapshot/WFX, 빌드 범위는 반영했고 objective 전용 회귀는 통과했다. 추가로 F4 burn interval을 양수로, Elder burn/execute ratio를 `[0,1]`로 제한했다. 전체 회귀의 Ezreal 1건과 실제 화면 캡처는 objective 구현 성공과 분리해 미통과/미검증으로 남긴다.

## 3. ⑤ 갱신

- `subKind=Dragon`을 항상 Elder로 해석하는 비용은 지금은 최소지만 일반/원소 Dragon이 생기는 순간 설계가 틀린다. 그때는 visual model이 아니라 별도 authoritative objective kind와 spawn schedule로 분리해야 한다.
- 5명 roster에 2,000씩 지급한다는 계약은 현재 5v5에는 맞지만 인원 가변 모드에서는 총 10,000을 보장하지 않는다. 총액 고정이 요구되면 `teamGoldTotal / eligibleRosterCount` 정책으로 바꿔야 한다.
- persistent WFX를 snapshot reconciliation으로 복구해 late join에는 강해졌지만, 표식이 많을 때 emitter 비용과 화면 노이즈가 늘어난다. 실측 GPU/가독성 문제가 생기면 단일 저비용 decal 또는 champion HUD icon으로 낮춰야 한다.
- F4는 canonical 네 JSON을 실제 저장한다. 빠른 실험성이 커진 대신 잘못된 수치도 source truth가 될 수 있으므로 dirty/stale/validation/atomic save 계약을 유지해야 한다. burn interval 0과 ratio 1 초과는 이번 실측에서 선제 차단했다.
- Command/Snapshot 변경은 append-only라 현재 호환성을 지키지만, 구버전 Client/Server 혼합 배포를 지원하려면 schema version negotiation과 legacy default 검증이 추가로 필요하다.
- headless 회귀는 수치와 상태 전이를 보장하지만 WFX의 미감, Dragon의 접촉 프레임, aura 가독성은 보장하지 않는다. 이 설계의 시각 완료 조건은 실제 F5 캡처에서 21% 비처형/20% 처형, buff 사망 제거, aura 진입/이탈 원복을 확인하는 것이다.
- 현 작업 트리의 Ezreal Q 데이터와 FormulaData probe 불일치는 전체 회귀를 붉게 만든다. objective 작업과 별도 세션에서 “수치 변경이 의도인지, probe가 낡았는지”를 결정하기 전에는 전체 회귀 PASS를 주장하면 안 된다.
