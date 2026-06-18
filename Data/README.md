# Data

원자: GameDesignSource, PresentationDesignSource, ArtSource, AudioSource, RuntimeAsset, LiveOpsContract, PublishingSource

Data는 하나의 원자가 아니다. 사람이 고치는 원본과 런타임이 로드하는 산출물을 구분한다.

현재 의미:
- `Gameplay/`: GameDesignSource
- `GameModes/`: GameDesignSource
- `LoL/FX/`: PresentationDesignSource
- `*.dat`, `*.navgrid`: RuntimeAsset 후보

추가 후보:
- `Art/`: art source가 repo 원본으로 들어올 때만 추가
- `Audio/`: audio source가 repo 원본으로 들어올 때만 추가
- `Publishing/`: marketing/public asset 원본이 repo에 들어올 때만 추가

구조 규칙:
- 기존 `Data/` 폴더명과 구조를 유지한다.
- 새 폴더는 실제 원자가 기존 구조에 담기지 않을 때만 추가한다.
- C++ fallback table을 Data와 다른 두 번째 truth로 만들지 않는다.
