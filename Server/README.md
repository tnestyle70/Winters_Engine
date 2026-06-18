# Server

원자: AuthorityExecution

Server는 command를 받아 gameplay truth를 실행하고 결과를 배포한다.

소유:
- command intake
- room/session authority
- GameSim tick
- snapshot/event/cue emission
- replay record emission
- bot command generation
- lag compensation, anti-cheat gate

소유하지 않음:
- Client visual success
- UI, animation, camera, local feel
- account, payment, store, profile backend state
- 기획 원본 수치 편집

구조 규칙:
- 기존 `Server/` 폴더명과 구조를 유지한다.
- gameplay 결과는 Client presentation이 아니라 Server authority와 GameSim truth로 증명한다.
