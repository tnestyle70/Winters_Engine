# MobaZero 논문 정복 인덱스

작성일: 2026-06-06

목표:
- NYPC Competition ML Lab과 Winters LoL AI를 잇는 배경 지식을 정복한다.
- 논문 전문을 복사 저장하지 않고, 공식 원문 링크와 한국어 연구 노트로 관리한다.
- 각 논문을 구현 가능한 단위로 쪼개 `Mushroom Lab -> Winters MOBA AI`로 연결한다.

저작권 원칙:
- PDF 전문 원문과 전체 한국어 번역은 이 폴더에 저장하지 않는다.
- 각 문서에는 공식 원문 링크, 핵심 요약, 알고리즘 구조, 구현 과제, 읽기 체크리스트만 저장한다.
- 원문을 읽을 때는 각 문서의 `원문 링크`를 사용한다.

## 정복 순서

1. [MCTS Survey](06_MCTS_Survey.md)
   - 탐색의 공통 언어. AlphaZero, MuZero, NYPC search expert의 바닥.

2. [AlphaZero](01_AlphaZero.md)
   - rules + self-play + policy/value + MCTS의 기준.

3. [PPO](05_PPO.md)
   - OpenAI Five와 실시간 self-play RL을 이해하기 위한 정책경사 기초.

4. [OpenAI Five / Dota 2](03_OpenAI_Five_Dota2.md)
   - MOBA형 장기 지평, 팀 보상, 대규모 self-play의 핵심 사례.

5. [AlphaStar](04_AlphaStar.md)
   - league training, imitation, counter-strategy, multi-agent RTS 사례.

6. [MuZero](02_MuZero.md)
   - rules를 직접 알지 않는 learned model 기반 planning.

7. [CFR](08_CFR.md)
   - 불완전정보 게임의 regret minimization 기초.

8. [DeepStack](07_DeepStack.md)
   - poker에서 depth-limited solving과 learned value를 결합한 사례.

9. [OpenSpiel](09_OpenSpiel.md)
   - 범용 game/state/search/RL 프레임워크 구조 참고.

10. [PettingZoo](10_PettingZoo.md)
    - multi-agent RL 환경 API 표준 참고.

11. [RLCard](11_RLCard.md)
    - 카드/불완전정보 게임 실험 플랫폼 참고.

## 큰 지도

```text
MCTS Survey
  -> AlphaZero
      -> MuZero
  -> Mushroom Search Expert

PPO
  -> OpenAI Five
  -> AlphaStar league/self-play 이해

CFR
  -> DeepStack
  -> Yacht Auction / Poker / hidden-info game 준비

OpenSpiel / PettingZoo / RLCard
  -> NYPC Lab API 설계 참고
  -> Winters BotDecisionContext / TeamBlackboard 설계 참고
```

## Winters / NYPC 적용 목표

NYPC Mushroom:
- Complete Game Model
- legal action mask
- alpha-beta / MCTS expert
- feature dataset
- imitation/value 학습
- baked C++ 제출 코드

Winters MOBA:
- BotDecisionContext
- BattleState 1v1/2v2 simulator
- FOW 기반 partial observation
- champion/minion/objective value
- TeamBlackboard 5인 협동
- self-play league
- AI Debug / replay viewer

