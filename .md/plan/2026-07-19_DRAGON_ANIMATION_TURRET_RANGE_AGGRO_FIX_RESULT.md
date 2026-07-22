# 1. 예측 vs 실측

- 적중: 런타임 WMesh는 2 submesh·107 bones·6,347 vertices, WSkel은 107 bones이며 해시 `0xb70d94d05260c98c`가 일치했다. `flying_run.wanim`도 같은 스켈레톤 해시, 106 channels·22,260 keys·1,000 TPS였고 zeroed root track은 없었다. 에셋 재가공 없이 클라이언트 샘플링 주기만 수정한다는 판정이 유지됐다.
- 적중: Server/GameSim Debug 빌드와 Client Debug x64 빌드가 성공했다. 통합 probe의 기존 projectile 4항목과 신규 `turret_range=1`, `remote_aggro=1`, `aggro_exit=1`이 모두 통과했다.
- 기준선 유지: 통합 probe 전체 exit code는 수정 전부터 실패하던 타 세션 범위의 `passive_pre_command=0` 때문에 1이다. 이번 범위의 기존·신규 포탑/투사체 항목에는 회귀가 없다.
- 미실측: 실제 F5 카메라 거리·프레임률에서 드래곤 날갯짓의 최종 체감과 inner/nexus turret의 화면상 사거리 표시는 수동 플레이 캡처가 필요하다.

# 2. 판결

수정 반영 — 손상 없는 30 Hz 원본을 8 Hz로 끊던 드래곤 전용 업데이트 병목을 제거했고, 포탑의 후보 선택·어그로 부여·어그로 유지에 동일한 7.75 중심점 XZ 사거리 계약을 적용했으며 자동 회귀 검증이 통과했다.

# 3. ⑤ 갱신

- 드래곤 1개체는 화면에 보일 때 프레임마다 최대 107-bone pose를 평가할 수 있어 기존 8 Hz보다 비용이 늘지만, 다른 정글 몬스터의 6개 업데이트 예산은 별도 카운터로 보존했다. 다수 드래곤을 동시에 표시하는 모드가 생기면 이 선택을 다시 측정해야 한다.
- 포탑 사거리는 충돌 반경을 더한 edge-to-edge가 아니라 authored `attackRange=7.75`의 중심점 XZ 거리다. 향후 edge-to-edge 설계로 바꾸려면 데이터 계약과 사거리 표시를 함께 명시적으로 변경해야 한다.
- 사거리 밖으로 나간 공격자에게 이미 발사된 투사체는 유지하고 새 발사만 차단한다. 기획이 기존 투사체까지 소멸시키는 것으로 바뀌면 projectile lifecycle 규칙을 별도 변경해야 한다.
