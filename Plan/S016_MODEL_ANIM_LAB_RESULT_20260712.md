Session - Model & Animation Lab(F5) 반영 결과와 인게임 검증 경계를 고정한다.

## 1. 결론

- **디자이너 루프의 D2/D3 고리가 닫혔다.** F5 한 키로 열리는 Model & Animation Lab에서: 월드의 아무 챔피언(로컬/연습 스폰 적 포함)을 표에서 선택 → 그 모델의 **전체 원시 애니메이션 클립 목록**을 보고 클릭으로 재생(루프/역재생/0.05~4x 배속) → Pause 후 **틱 단위 스크럽**으로 프레임을 앞뒤로 탐색 → **서브메시 분해**(파트별 체크박스/Solo/All/None, 이름+머티리얼 인덱스 표시). 지금까지 키보드 단축키에 묶여 있던 애니메이션 확인이 완전한 마우스 툴 워크플로로 전환됐다.
- **루프 폐쇄**: F10 Spawn Champion(S015)으로 배치 → F5로 검사/분해 → F7 WFX로 이펙트 부착 확인 → S014 Pause/S015 Rewind와 병용 — "모델 검사 → 배치 → 인게임 테스트"가 한 번의 빌드·실행 안에서 성립.
- **"인게임 vs 에디터" 판정**: P0는 인게임(연습 모드) — Pause/Spawn/이펙트와의 결합이 본질이기 때문. Unity 포폴식 **격리 궤도 뷰어 창은 P1**(`Scene_AssetPreview`: 자체 CWorld+궤도 카메라 — 라이브 월드 내 클라 격리는 2026-05-14 gotcha 위반 위험이라 별도 씬이 정도).
- 부산물: `FindAnimationIndex`의 부분 문자열 매칭 함정(이름 재생 오염 가능)을 우회하는 인덱스 기반 재생 API가 Engine에 생겼고, 서브메시 이름/머티리얼 열거가 수출되어 **파트 인덱스 발견 → 네임드 마스크 박제**(Yone_MeshGroups 패턴)가 클릭으로 가능해졌다.

## 2. 자동 검증 결과 (2026-07-12)

- Engine(+UpdateLib) → Client Debug x64 빌드 **첫 시도 PASS** (반복 0회).
- SimLab 전체 회귀 exit 0 — `[SimLab][Keyframe] PASS` 포함 전 프로브 유지 (Engine 헤더 변경의 결정론 비영향 확인).
- `git diff --check` 이상 없음.
- 반영 규모: Engine 3파일(헤더 인라인 세터 2 + ModelRenderer 패스스루 5종) + Client 신규 2파일 + 등록 4파일(vcxproj/filters 포함, packet Always-Lock 소유 하에).

## 3. 인게임 검증 절차 (사용자 게이트)

S016 SESSION §2의 7항 체크리스트. 핵심 3개:
1. **F5 → 이렐리아 선택 → 클립 클릭 재생 → Pause → 스크럽** (키보드 없이 전 과정).
2. **F10 Spawn Champion으로 적 Yone 배치 → F5에서 선택 → GhostKatana Solo → Restore Default**.
3. F7 이펙트 스폰과 병용 — 모델+이펙트+애니 보간을 한 화면에서 튜닝.

실패 채증: 클립 재생이 즉시 원복되면(로코모션 경합) 해당 챔피언의 이동/액션 상태와 함께 보고 — 하드 오버라이드 태그(P1 옵션)로 전환 판단.

## 4. 남은 경계 (P1)

- `Scene_AssetPreview` 격리 뷰어 + 궤도 카메라 + Cinematic `CSequencePlayer` 타임라인(Anim+Fx 트랙 Seek — 미사용 모듈 활성화).
- 본 라인 오버레이(본 이름 호버 — F7 Bone 앵커 피커 겸용), castFrame/recoveryFrame 마커를 스크럽 바에 표시(SkillTimingPanel 통일).
- 클립 크로스페이드 블렌딩 프리뷰(애니메이터 실작업용)는 프리뷰 스테이지 이후.
- 미니언/정글 대상 확장(매니저 소유 렌더러라 별도 열거 필요), 비주얼 스왑 시 편집 자동 재적용.
- 전부 미커밋 — S014/S015와 함께 인게임 게이트 통과 후 checkpoint commit.
