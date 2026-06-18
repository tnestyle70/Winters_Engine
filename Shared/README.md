# Shared

원자: GameplayTruth, protocol contract, replay/schema contract

Shared는 Client와 Server가 공유해야 하는 실행 계약을 소유한다.

소유:
- `Shared/GameSim/`: deterministic gameplay truth
- `Shared/Network/`: command, snapshot, event protocol
- `Shared/Replay/`: replay record contract
- `Shared/Schemas/`: shared data/schema contract

소유하지 않음:
- renderer, UI, ImGui, DX type
- Client visual feel
- Server process ownership
- Backend account state
- cooked art/audio asset

구조 규칙:
- 기존 `Shared/` 폴더명과 구조를 유지한다.
- GameSim은 화면을 몰라야 한다.
- Network와 Replay는 truth를 만들지 않고 전달/재현 계약만 가진다.
