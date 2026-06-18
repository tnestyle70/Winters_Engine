# CFR

원문:
- NeurIPS: https://papers.nips.cc/paper/3306-regret-minimization-in-games-with-incomplete-information
- PDF: https://papers.nips.cc/paper_files/paper/2007/file/08d98638c6fcd194a4b1e6992063e944-Paper.pdf

한 줄:
- 불완전정보 extensive-form game에서 counterfactual regret을 줄여 근사 Nash equilibrium을 찾는 방법.

## 한국어 핵심

CFR은 poker 같은 게임에서 강력한 기본기다. 핵심은 “내가 어떤 정보집합에서 다른 행동을 골랐다면 얼마나 후회했을까”를 누적하고, 후회가 큰 행동의 확률을 높이는 방식이다. 완전정보 minimax와 달리, 정보집합과 mixed strategy가 중심이다.

## 알고리즘 구조

핵심 개념:
- information set
- strategy
- regret
- counterfactual value
- regret matching
- average strategy

간단한 루프:

```text
for iteration:
  traverse game tree
  compute counterfactual values
  update regrets
  update average strategy
```

## MobaZero 적용 포인트

NYPC:
- Yacht Auction처럼 동시 선택/입찰이 있는 게임에 중요하다.
- hidden/private information이 있으면 minimax만으로 부족하다.

Winters MOBA:
- 직접 CFR을 full MOBA에 적용하기는 어렵다.
- 다만 “상대도 나를 속이고 섞어야 한다”는 mixed strategy 관점은 유용하다.
- Baron bait, fog trap, fake recall 같은 전략은 deterministic policy만으로 약할 수 있다.

## 구현 과제

1. Rock-Paper-Scissors CFR toy.
2. Kuhn Poker CFR toy.
3. Yacht bid phase에 regret matching 적용.
4. MOBA에서는 macro deception policy의 이론 배경으로 보존.

## 읽기 체크리스트

- information set이 무엇인지 설명할 수 있는가?
- regret matching이 왜 mixed strategy를 만드는가?
- minimax와 CFR의 차이를 설명할 수 있는가?
- Yacht Auction에서 CFR이 필요한 구간을 찾았는가?

