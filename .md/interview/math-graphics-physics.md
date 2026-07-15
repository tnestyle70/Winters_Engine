# 게임 수학 (물리/렌더링/AI/네트워크) — 기술면접 대비

Winters 엔진(DX11 LoL 스타일 클라/서버/Shared 시뮬)의 실제 코드에 근거해 정리한 수학 면접 개념 질문 + 모범답변 모음. 각 답변은 정의/수식 → 직관 → Winters 실전 적용 → 함정/꼬리질문 순서로 구성.

## 목차

1. 선형대수 기초 & 좌표계
2. 회전 수학 (오일러/쿼터니언/yaw)
3. 렌더링 변환 파이프라인 수학
4. 셰이딩 / 라이팅 수학
5. 텍스처링 / 샘플링 수학
6. 애니메이션 / 스키닝 수학
7. 내비게이션 / 패스파인딩 수학
8. 네트워크 / 스냅샷 보간 수학
9. AI 탐색 / 의사결정 수학
10. 충돌 / 피킹 / 물리 적분 수학
11. 비전 / 시야(FOV) 수학

---

## 선형대수 기초 & 좌표계

### Q. 내적(dot product)의 정의와 기하학적 의미를 설명하고, 게임플레이 코드에서 어디에 쓰는지 예를 들어보세요.

**정의:** a·b = ax*bx + ay*by + az*bz = |a||b|cosθ. 스칼라를 반환하며 교환법칙이 성립한다.

**기하학적 의미:** 두 벡터 사이 각도의 코사인에 길이를 곱한 값이다. 한 벡터가 단위벡터면 내적은 그 방향으로의 **정사영 길이**가 된다. 부호만으로도 정보가 있다 — 양수면 같은 방향(90도 미만), 0이면 수직, 음수면 반대 방향. 그래서 "타겟이 내 앞에 있는가"는 dot(forward, toTarget) > 0 한 줄로 판정된다.

**Winters 적용:** `EngineSDK/inc/WintersMath.h`의 `Vec3::Dot`이 투영·각도·전후 판정의 기반이고, `LengthSq`/`DistanceSqXZ`도 내적(자기 자신과의 내적)으로 거리 제곱을 구한다. `DistanceSqPointToSegmentXZ`의 스킬샷 판정도 내적 기반 투영이다.

**함정/꼬리질문:** cosθ를 얻으려면 두 벡터를 정규화해야 한다는 점을 빠뜨리기 쉽다. 또 "각도를 구하라"는 질문에 acos부터 꺼내면 감점 — 대부분의 판정은 acos 없이 내적 부호나 임계 cos값 비교로 끝나며, acos은 비싸고 입력이 [-1,1]을 부동소수점 오차로 벗어나면 NaN이 난다(clamp 필요).

### Q. 외적(cross product)은 무엇을 주고, 결과 벡터의 방향은 무엇이 결정합니까?

**정의:** a×b = (ay*bz − az*by, az*bx − ax*bz, ax*by − ay*bx). 크기는 |a||b|sinθ = 두 벡터가 이루는 평행사변형의 면적, 방향은 두 벡터에 모두 수직.

**기하학적 의미:** 면의 법선을 만들거나(삼각형 두 변의 외적), 회전 방향/좌우 판정을 한다. XZ 평면 이동 게임에서는 cross(forward, toTarget).y의 부호로 "타겟이 왼쪽인가 오른쪽인가"를 판정한다 — 사실상 2D 외적(스칼라)이다.

**Winters 적용:** `WintersMath.h`의 `Vec3::Cross`가 표준 3D 외적으로 법선/회전방향 산출에 쓰인다.

**함정/꼬리질문:** 외적은 **비가환**(a×b = −b×a)이고, 결과의 방향은 **좌표계 handedness**에 따라 해석이 달라진다 — 왼손 좌표계에서는 왼손 법칙으로 감아야 한다. "삼각형 와인딩 순서와 법선 방향의 관계"가 단골 꼬리질문: LH+시계방향 와인딩이 DirectX 기본 전면(front face)이다.

### Q. 벡터 정규화 시 주의할 수치적 문제와, 거리 비교에서 sqrt를 피하는 이유를 설명하세요.

**정의:** normalize(v) = v / |v|, |v| = sqrt(v·v).

**핵심 문제:** 길이가 0(또는 매우 작은) 벡터를 정규화하면 0-나눗셈으로 NaN/Inf가 전파된다. "이동 방향 = 목표 − 현재"에서 두 점이 같으면 바로 터지므로, epsilon보다 작으면 기본 방향을 반환하거나 정규화를 건너뛰는 방어가 필요하다.

**sqrt 회피:** 거리 **비교**는 단조성 때문에 제곱 상태로 해도 결과가 같다 — distSq < range*range. sqrt는 비싸고 매 프레임 수백 유닛의 사거리 판정에 곱해지면 무시 못 한다.

**Winters 적용:** `WintersMath.h`의 `LengthSq`/`DistanceSqXZ`가 이 패턴이고, 스킬 사거리·타겟 선택은 제곱거리로 통일했다. `NormalizeRadians`도 비유한값(NaN/Inf) 입력을 0으로 처리해 각도 파이프라인에 오염이 전파되지 않게 한다.

**함정:** "정규화된 벡터"라고 가정하고 받은 API에 미정규 벡터를 넣는 경우, 내적이 cosθ가 아니게 되어 판정이 조용히 틀어진다 — 계약을 주석/어서트로 못 박아야 한다.

### Q. 동차좌표(homogeneous coordinates)에서 w 성분의 역할은 무엇이고, 점과 방향벡터의 변환이 왜 달라야 합니까?

**정의:** 3D 점을 (x, y, z, 1), 방향을 (x, y, z, 0)으로 확장한다. 4x4 행렬의 4행(행벡터 규약 기준)에 이동(translation)이 들어가는데, 이동은 선형변환이 아니라 3x3 행렬로는 표현 불가 — w=1이 이동 행과 곱해지며 더해지는 구조로 **아핀 변환을 하나의 행렬 곱**으로 통일한다.

**기하학적 의미:** 점은 위치이므로 이동의 영향을 받고, 방향은 "어디를 향하는가"만 의미하므로 이동하면 안 된다. w=0이 이동 성분을 자동으로 소거한다.

**Winters 적용:** `WintersMath.h:297`의 `Mat4::TransformPoint`는 `XMVector3TransformCoord`(w=1, 이동 포함), `TransformDirection`은 `XMVector3TransformNormal`(w=0, 회전/스케일만)로 명시적으로 분리해 놓았다. 이 구분을 API 이름으로 강제하는 게 핵심 규약이다.

**함정:** 방향벡터를 TransformPoint로 변환하는 실수 — 오브젝트가 원점 근처에 있을 땐 티가 안 나다가 월드 좌표가 커지면 방향이 왜곡되는, 위치 의존적 버그가 된다. 꼬리질문으로 "w가 1도 0도 아닌 경우"가 나오면 원근 투영 후 w=z 기반 값이 되고 **perspective divide**(x/w, y/w, z/w)로 NDC를 얻는다고 답한다.

### Q. 왼손 좌표계와 오른손 좌표계의 차이는 무엇이고, Winters는 어느 쪽을 왜 선택했습니까?

**정의:** +X 오른쪽, +Y 위일 때 +Z가 화면 안쪽이면 왼손(LH), 화면 바깥쪽이면 오른손(RH). 수학적으로는 기저의 방향(orientation), 즉 det의 부호 관점 차이이며 외적 결과의 해석 방향이 반대가 된다.

**실무 영향:** view/projection 행렬 팩토리의 부호(카메라가 +Z를 보느냐 −Z를 보느냐), 삼각형 와인딩·컬링 방향, 외적 기반 좌우 판정 부호가 전부 handedness에 묶인다. 어느 쪽이 우월한 건 아니고 **한 코드베이스 안에서 일관성**이 전부다.

**Winters 적용:** `WintersMath.h:281`의 LookAt/Perspective/Orthographic 팩토리가 전부 `XMMatrixLookAtLH`·`XMMatrixPerspectiveFovLH`·`XMMatrixOrthographicLH`를 사용해 LH로 통일했다. DirectX 전통 규약과 맞추고, 뷰포트도 MinDepth=0/MaxDepth=1(`CDX11Device.cpp:772`)로 DirectX식 [0,1] 깊이를 쓴다.

**함정:** 외부 에셋(FBX 등)은 RH+Z-up인 경우가 많아 임포트 시 축 변환이 필요하다. Winters에서도 raw Riot FBX 챔피언 메시의 forward 축이 엔진 규약과 달라 별도 yaw 보정 규약이 생겼다(아래 질문 참조). "LH↔RH 변환은 어떻게?"라는 꼬리질문에는 한 축 반전 + 와인딩 순서 뒤집기(또는 컬링 모드 반전)를 답한다.

### Q. row-major와 column-major의 차이를 설명하고, DirectXMath에서 world = local * parent 순서로 곱하는 이유를 설명하세요.

**두 가지 별개 개념을 구분해야 한다:**
1. **메모리 레이아웃**(row-major vs column-major storage) — 행렬 원소가 메모리에 행 단위로 놓이는가 열 단위로 놓이는가.
2. **벡터 규약**(행벡터 v*M vs 열벡터 M*v) — 이것이 곱셈 순서를 결정한다.

DirectXMath는 row-major 저장 + **행벡터** 규약이다. 행벡터 규약에서는 v' = v * M1 * M2 순서로 왼쪽에서 오른쪽으로 변환이 적용되므로, "먼저 적용될 변환이 왼쪽"이다. 반면 GLSL/OpenGL 관례는 열벡터라 M2 * M1 * v로 오른쪽부터 적용된다 — 코드만 보고 순서를 착각하기 쉬운 지점.

**Winters 적용:** `Engine/Private/ECS/Systems/TransformSystem.cpp:88`에서 worldMat = local * parent — 자식의 로컬 변환이 먼저, 부모 변환이 나중에 적용되는 행벡터 규약 그대로다. 또 이동 성분이 행렬의 4행에 있으므로 `GetWorldPosition`은 `_41/_42/_43`을 판독한다(열벡터 규약이었다면 4열).

**함정:** HLSL 상수버퍼는 기본이 **column-major 저장**이라, C++에서 row-major로 채운 행렬을 그대로 올리면 전치돼 보인다 — 그래서 셰이더 업로드 전에 Transpose하거나 `row_major` 지정자를 쓴다. "왜 상수버퍼 넣기 전에 Transpose 하냐"는 단골 꼬리질문.

### Q. SRT(Scale→Rotate→Translate) 순서로 변환을 합성하는 이유는 무엇입니까? 순서를 바꾸면 어떻게 됩니까?

**정의:** local = S * R * T (행벡터 규약, 왼쪽이 먼저 적용). 행렬 곱은 **비가환**이므로 순서가 결과를 바꾼다.

**기하학적 의미:** 스케일과 회전은 원점 기준 연산이다. 이동을 먼저 하면 (T * R) 오브젝트가 원점에서 떨어진 상태로 회전해 **공전**하듯 궤도를 돌고, 스케일보다 이동이 먼저면 이동량까지 스케일된다. "제자리에서 크기 바꾸고, 제자리에서 돌고, 그다음 위치로 보낸다"가 SRT의 직관이다.

**Winters 적용:** `TransformSystem.cpp:88`이 `XMMatrixAffineTransformation(scale, rot, pos)`으로 로컬 SRT를 만들고, `CTransform.cpp:21`의 s*r*t와 동일 규약이다. 계층 구조에서는 world = local * parent로 확장되어 "자식 SRT → 부모 SRT" 순서로 적용된다.

**함정:** 비균등 스케일이 계층에 섞이면 부모의 회전과 자식의 스케일이 상호작용해 **전단(shear)**이 생길 수 있다 — 많은 엔진이 이를 금지하거나 스케일 상속을 분리하는 이유다. 꼬리질문: "피벗 기준 회전은?" → T(−pivot) * R * T(pivot) 샌드위치로 답한다.

### Q. 뷰 변환과 투영 변환이 각각 하는 일, 그리고 NDC와 perspective divide를 파이프라인 순서대로 설명하세요.

**파이프라인:** local → (world 행렬) → world → (view 행렬) → view/camera 공간 → (projection 행렬) → clip 공간 → (w로 나눔 = perspective divide) → NDC → (뷰포트 변환) → 스크린.

**뷰 행렬:** 카메라의 월드 변환의 **역행렬**이다. "카메라를 원점으로 옮기는" 기저변환으로, LookAt은 eye/at/up에서 카메라의 right·up·forward 직교정규기저를 만들어 역변환을 직접 구성한다. **투영 행렬:** 절두체(frustum)를 클립 공간 큐브로 사상하며, 원근 투영은 z를 w에 실어 divide 후 멀수록 작아지게 만든다.

**NDC 범위:** DirectX는 x,y ∈ [-1,1], **z ∈ [0,1]**, OpenGL 전통은 z ∈ [-1,1]. 이 차이가 투영 행렬의 z행 구성과 깊이 정밀도에 영향을 준다.

**Winters 적용:** `WintersMath.h`의 `XMMatrixLookAtLH`/`XMMatrixPerspectiveFovLH` 팩토리 + `CDX11Device.cpp:772`의 MinDepth=0/MaxDepth=1 뷰포트로 DirectX식 [0,1] 깊이 파이프라인을 구성한다.

**함정:** 원근 투영의 z는 **비선형**(near 근처에 정밀도 몰림)이라 far/near 비율이 크면 z-fighting이 난다 — near를 키우는 게 far 줄이기보다 효과적이다. 꼬리질문 대비: 마우스 픽킹은 이 파이프라인의 **역방향**(스크린→NDC→unproject→월드 레이)이다.

### Q. 법선(normal)을 변환할 때 world 행렬 대신 inverse-transpose를 쓰는 이유를 수식과 함께 설명하세요.

**유도:** 법선 n은 표면 접선 t와 수직: n·t = 0. 접선은 world 행렬 M으로 변환된다(t' = tM). 변환 후에도 수직을 유지하려면 n' = n * G에 대해 n'·t' = 0이어야 하고, 이를 만족하는 G = (M⁻¹)ᵀ, 즉 **역행렬의 전치**다.

**직관:** 비균등 스케일은 표면을 한 축으로 늘리는데, 법선을 같은 행렬로 변환하면 법선도 같이 늘어나 표면에 더 이상 수직이 아니게 된다(늘어난 경사면을 상상). 역전치는 스케일을 "반대로" 적용해 수직성을 보존한다.

**Winters 적용:** `Engine/Private/Renderer/ModelRenderer.cpp:316`에서 worldInvTranspose = Transpose(Inverse(world))를 상수버퍼에 적재해 셰이더의 법선 변환에 쓴다.

**함정:** M이 회전+이동+**균등 스케일**만이면 (M⁻¹)ᵀ의 3x3부는 M의 3x3부와 (스케일 팩터 차이 빼고) 방향이 같아서 world를 그대로 써도 정규화 후 결과가 같다 — "그럼 왜 항상 역전치를 쓰냐"는 꼬리질문에는 "비균등 스케일 에셋이 하나라도 들어오는 순간 조용히 라이팅이 깨지므로 일반해를 기본으로 둔다"고 답한다. 변환 후 법선 재정규화도 잊지 말 것.

### Q. 오일러각, 회전행렬, 쿼터니언의 트레이드오프를 비교하고, 어떤 상황에 무엇을 쓰겠습니까?

**비교:**
- **오일러각**: 3 float, 인간이 읽기 쉬움(에디터/디버깅). 단점 — **짐벌락**(두 회전축이 정렬되어 자유도 상실), 합성이 축 순서 의존적, 보간이 부자연스러움.
- **회전행렬**: 합성(곱)과 벡터 변환이 직접적, GPU 파이프라인의 최종 형태. 단점 — 9 float, 누적 오차로 직교성이 깨지면 재직교화 필요, 보간 불가에 가까움.
- **쿼터니언**: q = w + xi + yj + zk, 4 float, 합성 저렴, **slerp**로 최단호 보간, 짐벌락 없음. 단점 — 직관성 낮음, q와 −q가 같은 회전(double cover)이라 보간 시 내적 부호 확인 필요.

**실무 답:** 저장/편집은 오일러, 합성/보간은 쿼터니언, GPU 전달 직전에 행렬 — 각 단계에서 표현을 바꾸는 게 표준이다.

**Winters 적용:** `TransformSystem.cpp:83`이 정확히 이 패턴이다 — 컴포넌트에는 오일러각을 저장하고, `XMQuaternionRotationRollPitchYaw(x,y,z)`로 쿼터니언 변환 후 `XMMatrixAffineTransformation`에 투입해 짐벌락/보간 문제를 합성 단계에서 회피한다.

**함정:** "쿼터니언이면 짐벌락이 무조건 없다"는 반쪽 답 — 오일러로 저장하고 매 프레임 오일러를 **누적**하면 여전히 짐벌락 경로를 탄다. 짐벌락 회피는 회전 상태를 쿼터니언(또는 행렬)으로 **누적**할 때 성립한다.

### Q. yaw 각도 하나로 XZ 평면의 방향벡터를 어떻게 얻고, 그 역변환은 무엇입니까? Winters에서 챔피언 바디에 +PI 오프셋을 두는 이유도 설명하세요.

**정의:** Winters 규약은 dir = (sin(yaw), 0, cos(yaw)) — yaw=0이 +Z 정면, yaw=+90도가 +X. 역변환은 yaw = atan2(dir.x, dir.z). 일반적인 수학 관례 atan2(y, x)와 인자 순서가 다름에 주의(기준축이 +Z이기 때문). XZ 회전은 2D 회전행렬 (x*c − z*s, x*s + z*c)로 처리한다.

**Winters 적용:** `WintersMath.h:154`의 `DirectionFromYawXZ`/`YawFromDirectionXZ`/`RotateXZ`가 이 규약의 단일 소스다. 그런데 **에셋의 forward 축은 엔진 규약과 무관**하다 — raw Riot FBX 챔피언 바디 메시는 모델의 정면이 반대 축을 보고 있어, `ChampionRuntimeDefaults.cpp:39`의 `GetDefaultChampionVisualYawOffset`이 해당 챔프에 +PI(180도)를 반환하고, `ResolveChampionVisualYawFromDirection`이 이동 방향→시각 yaw 산출 시 이 오프셋을 얹는다. 자체 제작 `.wmesh`로 고정된 챔프(Irelia, Yasuo, Viego)는 0을 쓴다 — **오프셋은 챔프 에셋 패밀리별 데이터이지 전역 상수가 아니다**.

**함정:** 실제로 겪은 사고 패턴이 "특정 챔프가 뒤로 걷는" 버그를 호출부마다 +PI 산발 패치로 막는 것 — 그러면 서버 권위 facing이 챔프별로 갈라진다. 보정은 방향→yaw 변환 파이프라인의 **한 지점**(오프셋 카탈로그 함수)에만 존재해야 하고, 투사체/FX 메시의 180도 보정은 바디 facing과 별도 경로로 분리한다.

### Q. 각도 정규화(wrap-around)는 왜 필요하고, 두 각 사이 최단 회전은 어떻게 구합니까? "연속 상태로서의 yaw"와 "wire 값으로서의 yaw"를 구분해야 하는 이유는요?

**정의:** 각도는 2π 주기라 θ와 θ+2π가 같은 방향이다. 정규화는 fmod로 [-π, π]에 래핑하는 것이고, 최단 회전 델타는 delta = Normalize(target − current), 최단 등가각은 nearest = ref + Normalize(x − ref)로 구한다.

**직관:** −179도에서 +179도로 갈 때 순진하게 빼면 358도를 도는 장회전이 나온다. 정규화된 델타는 −2도(짧은 쪽)를 준다.

**Winters 적용:** `WintersMath.h:188`의 `NormalizeRadians`(비유한값은 0 처리)와 `NearestEquivalentRadians`가 이 도구다. 핵심 규약은 **Transform에 저장되는 바디 yaw는 정규화하지 않는 연속 상태**라는 것 — 매 서버 이동 틱마다 [-π,π]로 정규화하면 빠른 우클릭 연타 시 +π/−π 경계를 반복적으로 재교차하며 캐릭터가 획획 도는 사고가 났다. 그래서 Transform yaw를 쓸 때는 `ResolveChampionVisualYawNear`로 현재 yaw에 가장 가까운 등가각을 저장하고, `NormalizeChampionVisualYaw`는 **wire 전송값, 로그, 델타 비교**에만 쓴다. 클라 쪽에서도 SnapshotApplier가 로컬 예측 yaw를 보호하다가 서버 yaw가 실제로 따라잡을 때만 넘겨준다 — ack 시퀀스 진행만으로 예측을 덮으면 클릭 직후 facing이 튄다.

**함정:** "정규화는 항상 좋다"는 가정이 함정이다. 정규화는 **비교와 직렬화의 규약**이고, 보간/누적되는 연속 상태에는 최단 등가각 유지가 맞다. 꼬리질문 대비: 이 문제의 일반화가 쿼터니언 보간의 q/−q 부호 선택(double cover)과 정확히 같은 구조다.

### Q. 직교정규기저(orthonormal basis)란 무엇이고, 카메라나 빌보드의 right·up·forward를 어떻게 재구성합니까?

**정의:** 서로 수직(직교)이고 각각 길이 1(정규)인 벡터 집합. 3D에서 right·up·forward 세 벡터가 직교정규기저를 이루면 그 자체가 회전행렬의 세 행(행벡터 규약)이 된다. 직교행렬은 **역행렬 = 전치**라는 성질이 있어 기저변환의 역이 공짜다.

**재구성 절차(LookAt/빌보드 공통):** forward = normalize(target − eye) → right = normalize(cross(worldUp, forward)) (LH 기준 순서) → up = cross(forward, right). 임시 worldUp은 정확히 수직일 필요가 없고, 외적 두 번으로 직교성이 복원된다.

**Winters 적용:** `WintersMath.h`의 LookAt 팩토리(`XMMatrixLookAtLH`)가 내부적으로 이 절차로 뷰 기저를 만들고, 뷰 행렬은 그 기저로의 **기저변환**(= 카메라 월드 변환의 역, 직교성 덕에 전치+이동 역산으로 구성)이다. FX 빌보드도 카메라 기저의 right/up을 빌려 쿼드를 세운다.

**함정:** forward가 worldUp과 평행해지는 순간(정수리에서 내려다보는 카메라) 외적이 0벡터가 되어 기저가 붕괴한다 — 특이 구성에서 대체 up을 쓰는 분기가 필요하다. 꼬리질문: "회전행렬이 누적 오차로 직교성을 잃으면?" → 그람-슈미트(위 절차 재실행)로 재직교화한다고 답한다.

### Q. 점과 선분 사이의 최단거리를 벡터 투영으로 구하는 방법을 설명하세요. 스킬샷 판정에 어떻게 쓰입니까?

**유도:** 선분 start→end, seg = end − start. 점 p의 투영 파라미터 t = dot(p − start, seg) / dot(seg, seg). 이 t를 [0,1]로 **clamp**한 뒤 closest = start + t*seg, 답은 |p − closest|² 이다.

**직관:** 내적으로 점을 선분의 축에 정사영시키는 것이다. clamp가 핵심 — clamp 없이는 **무한 직선**과의 거리가 되어, 선분 밖으로 투영되는 점은 끝점까지의 거리를 써야 한다. t=0/1로 잘리면 최근접점이 끝점이 된다.

**Winters 적용:** `WintersMath.h:207`의 `DistanceSqPointToSegmentXZ`가 정확히 이 구현이고, 직선형 스킬샷(투사체 경로 vs 유닛 위치) 히트 판정에 쓰인다. 제곱거리로 반환해 반경 비교 시 sqrt를 생략한다.

**함정:** dot(seg, seg)가 0인 퇴화 선분(start == end)의 0-나눗셈 방어가 필요하다 — 이때는 점-점 거리로 폴백. 꼬리질문 대비: 실제 스킬샷은 폭이 있으므로 distSq < (skillRadius + unitRadius)²으로 판정하고, 움직이는 투사체는 "선분 vs 점"을 틱마다 스윕 구간으로 확장한다(터널링 방지).

### Q. A* 휴리스틱의 admissibility란 무엇이고, Manhattan/Euclidean/Octile 중 8방향 그리드에 무엇이 적합합니까?

**정의:** 휴리스틱 h가 **admissible**하면 실제 최단 비용을 절대 과대평가하지 않고(h ≤ h*), 그때 A*는 최적해를 보장한다. **consistent**(h(n) ≤ cost(n,n') + h(n'))하면 노드 재확장 없이 효율적으로 동작한다.

**비교:** Manhattan = |dx|+|dy|는 4방향용 — 8방향 그리드에서는 대각 이동이 두 축을 동시에 줄이므로 **과대평가**가 되어 admissible이 깨진다. Euclidean = sqrt(dx²+dy²)는 admissible하지만 그리드 이동 비용보다 낙관적이라 탐색 노드가 늘어난다. **Octile** = max(|dx|,|dy|) + (√2 − 1) * min(|dx|,|dy|)는 대각 비용 √2인 8방향 그리드의 **정확한 하한**이라 admissible하면서 가장 조여진(tight) 휴리스틱이다.

**Winters 적용:** `Engine/Private/Manager/Navigation/Pathfinder.cpp:82`의 `OctileDistance`가 상수 kSqrt2m1 = √2 − 1을 두고 이 식 그대로 구현되어, f = g + h 우선순위 큐 탐색의 h로 쓰인다.

**함정:** "휴리스틱을 키우면 빨라진다"며 h에 가중치(w > 1)를 곱하면 admissibility가 깨져 경로가 최적이 아니게 된다 — 속도/최적성 트레이드오프임을 명시해야 한다. 또 경로 탐색 실패(빈 경로)를 조용히 삼키면 유닛이 제자리에 stuck된다 — Winters에서도 empty path에 직선 fallback + 사유 노출을 붙인 경험이 있다.

### Q. 4x4 아핀 변환 행렬의 역행렬을 일반 역행렬 공식보다 싸게 구하는 방법이 있습니까? 역행렬과 전치는 각각 언제 씁니까?

**핵심:** 아핀 변환 M = [R*S | 0; T | 1] 구조를 알면 일반 4x4 역행렬(가우스 소거/여인수)이 필요 없다. 회전만 있으면 R⁻¹ = Rᵀ(직교행렬)이고, 역변환은 "이동을 반대로 → 회전을 전치로" 순서 재조합으로 구성된다. 균등 스케일 s가 있으면 1/s만 추가된다.

**의미 구분:** **역행렬**은 "변환 취소" — 월드→로컬 변환, 뷰 행렬(카메라 월드의 역), 픽킹의 unproject에 쓴다. **전치**는 직교행렬의 역이라는 특수 성질 + 행/열 규약 전환(HLSL 업로드) + 법선 변환의 역전치 조합에 쓴다. 둘은 목적이 다르며 직교행렬에서만 우연히 일치한다.

**Winters 적용:** 뷰 행렬은 LookAt 팩토리가 기저 전치+이동 역산으로 직접 구성하는 형태이고, `ModelRenderer.cpp:316`의 Transpose(Inverse(world))는 두 연산을 조합한 법선 변환용이다. 계층 Transform에서 부모 월드의 역을 곱해 로컬을 복원하는 것도 같은 도구다.

**함정:** 스케일이 섞인 행렬에 "직교니까 전치가 역"이라고 적용하는 실수 — R*S는 직교가 아니다. 또 매 프레임 일반 XMMatrixInverse를 호출하는 것은 아핀 구조를 아는 경우 낭비이며, det ≈ 0(퇴화 스케일 0)인 행렬의 역은 NaN을 뿌린다 — 스케일 0 에셋 방어가 필요하다는 것까지 언급하면 좋다.

---

## 회전 수학 (오일러/쿼터니언/yaw)

### Q. 방향 벡터에서 yaw 각도를 뽑을 때 왜 atan2를 쓰는가? atan(z/x) 같은 단순 아크탄젠트로는 안 되는 이유는?

atan2(a, b)는 두 인자의 부호를 모두 보고 (-PI, PI] 전 범위의 각도를 돌려주는 함수다. 단일 인자 atan(a/b)는 결과가 (-PI/2, PI/2)로 접혀서 1·3사분면과 2·4사분면을 구분하지 못하고, b = 0에서 나눗셈이 터진다. 기하학적으로 atan2는 "원점에서 (b, a)를 바라보는 방향각"을 사분면 정보까지 보존해 반환하는 것이다. Winters에서는 `YawFromDirectionXZ`(Engine/Include/WintersMath.h:178)가 `atan2(dir.x, dir.z) + yawOffset`으로 이동 방향을 yaw로 바꾼다 — 인자 순서가 흔히 보는 atan2(y, x)가 아니라 atan2(x, z)인 이유는 이 엔진의 forward가 +Z축이라 "yaw = 0 → +Z를 바라봄"이 되도록 기준축을 z에 둔 것이다. 흔한 함정: 인자 순서를 관례대로 (z, x)로 쓰면 yaw = 0이 +X를 향하게 되어 모든 캐릭터가 90도 돌아가 보인다. 꼬리질문으로 "영벡터가 들어오면?"이 나오는데, XZ 평면 정규화 전에 길이 0 방어가 필요하다는 점까지 말하면 좋다.

### Q. yaw에서 방향 벡터를 복원하는 식을 쓰고, 방향→yaw 변환과 정확한 역함수 관계임을 어떻게 보장하는지 설명하라.

yaw가 +Z 기준일 때 단위 방향은 dir = (sin(yaw), 0, cos(yaw))다. 역함수 관계는 atan2(sin(yaw), cos(yaw)) = yaw라는 항등식으로 보장된다 — 즉 방향→yaw에서 atan2(x, z)를 썼다면 yaw→방향은 반드시 (sin, 0, cos) 순서여야 왕복 변환이 무손실이다. Winters에서는 `DirectionFromYawXZ`(WintersMath.h:154)가 이 역할을 하고, SnapshotApplier/MoveSystem의 GameplayForwardFromVisualYaw가 서버에서 받은 yaw로 게임플레이 forward를 복원할 때 사용한다 — 단, 저장된 yaw에는 에셋별 modelYawOffset이 더해져 있으므로 **먼저 오프셋을 빼고** 역변환해야 한다. 흔한 실수: forward 규약이 다른 코드(예: +X forward의 (cos, 0, sin))를 복사해 와서 90도 뒤틀리거나, 시각 yaw와 게임플레이 yaw를 구분하지 않고 오프셋이 낀 값을 그대로 역변환해 정반대 방향의 forward를 얻는 것이다.

### Q. 임의의 라디안 각도를 (-PI, PI]로 정규화하는 방법과 fmod를 쓸 때의 함정은?

기본 아이디어는 `r = fmod(x + PI, 2PI); if (r < 0) r += 2PI; return r - PI;`다. fmod의 함정은 C/C++의 fmod가 피제수의 부호를 따르기 때문에 음수 입력에서 결과가 음수로 나온다는 것 — 음수 보정 분기를 빼먹으면 정규화 범위가 절반만 맞는다. 또 NaN/Inf 입력은 fmod를 그대로 통과해 오염을 전파하므로 방어가 필요하다. Winters의 `NormalizeRadians`(WintersMath.h:188)가 정확히 이 구현이며 NaN/Inf 방어까지 포함하고, 결과를 "표준(canonical) yaw"라고 부른다. 핵심 함정은 용도 제한이다: 이 표준값은 와이어 전송·로그·델타 비교 전용이고, Transform의 바디 회전에 매 틱 저장하면 +PI/-PI 경계 근처에서 값이 +3.13 → -3.13으로 점프해 캐릭터가 순간적으로 360도 도는 튐이 생긴다(헤더 주석에 금지 명시). "정규화는 항상 좋은 것"이라는 선입견이 실전에서 깨지는 대표 사례다.

### Q. XZ 평면에서 벡터를 각도 t만큼 회전시키는 식을 유도하고, 탑다운 게임에서 어디에 쓰이는가?

2D 회전행렬 R(t) = [[cos t, -sin t], [sin t, cos t]]를 (x, z)에 적용하면 x' = x·cos t - z·sin t, z' = x·sin t + z·cos t다. 유도는 기저벡터의 회전 결과를 열로 세우면 된다: (1,0) → (cos, sin), (0,1) → (-sin, cos). 기하학적으로는 원점 기준 반시계(축 방향 규약에 따라) 회전이며, 탑다운 MOBA에서는 대부분의 게임플레이 회전이 사실상 이 2D 문제로 환원된다. Winters의 `RotateXZ`(WintersMath.h:147)가 이 식 그대로이며 y는 0으로 유지하고, 부채꼴 스킬의 방향 산개(기준 방향을 ±각도로 여러 개 회전) 등에 쓰인다. 꼬리질문 대비: 3D 회전행렬의 y축 회전에서 x·z 블록만 뽑은 것과 동일하다는 점, 그리고 sin 부호 하나만 뒤집혀도 회전 방향이 반대가 되므로 좌표계 handedness와 함께 검증해야 한다는 점.

### Q. 짐벌락(gimbal lock)은 왜 발생하고, 쿼터니언은 어떻게 이를 회피하는가?

오일러 각은 회전을 세 축의 순차 회전(예: yaw→pitch→roll)으로 표현하는데, 가운데 축이 ±90도가 되면 첫 축과 셋째 축의 회전 평면이 겹쳐 두 자유도가 하나로 퇴화한다 — 이것이 짐벌락이다. 수학적으로는 오일러 각→회전의 매핑이 그 지점에서 야코비안 랭크를 잃는(특이점) 것이고, 기하학적으로는 짐벌 링 두 개가 같은 평면에 눕는 것이다. 쿼터니언은 회전을 세 각의 합성이 아니라 단위 4차원 벡터(3-구면 S3 위의 점) 하나로 표현하므로 파라미터화 자체에 특이점이 없어 짐벌락이 원천적으로 없다. Winters는 TransformComponent에 회전을 오일러 라디안 Vec3로 저장하지만, 탑다운 MOBA 특성상 바디 회전이 사실상 yaw(.y) 단일 축이라 짐벌락 위험이 실질적으로 없고, 행렬화는 쿼터니언 경유(TransformSystem.cpp:83)로 처리한다. 꼬리질문 대비: "그럼 오일러는 버려야 하나?" — 아니다. 단일 축 회전, 디자이너 편집 UI, 저장 포맷에는 오일러가 직관적이고 충분하며, 문제는 3축 합성 보간/누적 시에만 터진다는 트레이드오프를 말해야 한다.

### Q. 쿼터니언의 정의, 곱셈, 켤레(conjugate)로 벡터를 회전시키는 방법을 설명하라. 왜 정규화가 필요한가?

쿼터니언은 q = w + xi + yj + zk (i² = j² = k² = ijk = -1)이고, 축 n·각 t 회전은 q = (cos(t/2), n·sin(t/2))로 표현된다. 벡터 v의 회전은 v' = q v q*, 여기서 q* = (w, -x, -y, -z)는 켤레이며 단위 쿼터니언에서는 역원과 같다. 곱셈 q1·q2는 회전 합성에 대응하고 비가환이다(회전 순서가 중요하므로 당연). 각이 t/2인 이유는 q v q*로 양쪽에서 곱하며 회전이 두 번 적용되는 구조이기 때문이다. 정규화가 필요한 이유: 회전을 나타내는 것은 |q| = 1인 단위 쿼터니언뿐인데, 부동소수점 곱을 반복 누적하면 노름이 1에서 드리프트해 회전에 스케일 성분이 섞인 채 행렬화된다 — 그래서 곱 누적 경로에서는 주기적 재정규화(q / |q|)가 필수다. Winters에서는 쿼터니언을 직접 누적하는 대신 매 프레임 오일러에서 XMQuaternionRotationRollPitchYaw로 새로 만들어(TransformSystem.cpp:83) 드리프트 누적 자체를 회피하는 구조다. 꼬리질문 대비: q와 -q가 같은 회전을 나타낸다는 이중피복(double cover)까지 언급하면 좋다.

### Q. 축-각(axis-angle) 표현과 로드리게스 회전 공식을 설명하고, 쿼터니언과의 상호 변환을 말하라.

축-각은 회전을 단위축 n과 각 t의 쌍으로 표현한다. 로드리게스 공식: v' = v·cos t + (n × v)·sin t + n·(n·v)·(1 - cos t). 기하학적 의미는 v를 축에 평행한 성분(회전 불변)과 수직 성분으로 분해한 뒤, 수직 성분만 그 평면에서 cos/sin으로 2D 회전시키는 것이다. 쿼터니언 변환은 직접적이다: q = (cos(t/2), n·sin(t/2)), 역으로 t = 2·acos(w), n = (x,y,z)/sin(t/2) (t ≈ 0일 때 sin(t/2) 나눗셈 특이점 주의). Winters의 바디 yaw는 축이 항상 (0,1,0)으로 고정된 축-각의 특수 케이스라 각도 스칼라 하나로 상태를 관리할 수 있고, 이 덕분에 `NearestEquivalentRadians` 같은 스칼라 각도 연속화 기법이 성립한다 — 일반 3D 회전이었다면 쿼터니언 double cover 처리로 풀어야 했을 문제다. 흔한 함정: t = 0 근처에서 축 복원이 불안정하다는 것, 그리고 축-각을 성분별로 lerp하면 회전 보간이 아니게 된다는 것(보간은 slerp나 각도 보간으로).

### Q. slerp와 lerp+정규화(nlerp)의 차이는 무엇이고, 각각 언제 쓰는가? double cover 처리도 함께 설명하라.

slerp(q1, q2, t) = q1·(q1* q2)^t, 전개하면 (sin((1-t)Ω)·q1 + sin(tΩ)·q2) / sin Ω (Ω는 두 쿼터니언 사잇각)이다. 4차원 단위구면 위의 대원(great circle)을 따라 **일정 각속도**로 보간한다. nlerp는 성분 lerp 후 정규화 — 경로는 같은 대원의 최단호를 지나지만 각속도가 중간에서 빨라졌다 느려지는 비등속이다. 트레이드오프: slerp는 sin/acos 비용이 있고 Ω ≈ 0에서 0/0 방어(lerp 폴백)가 필요하지만 등속이 보장되고, nlerp는 싸고 안정적이지만 큰 각도에서 속도 왜곡이 보인다. 두 방식 모두 보간 전에 dot(q1, q2) < 0이면 한쪽 부호를 뒤집어야 한다 — q와 -q는 같은 회전(double cover)이므로 부호를 안 맞추면 최단 경로 대신 "먼 길"로 돌아가는 보간이 된다. Winters에서는 스켈레탈 애니메이션 키프레임 보간에서 회전을 XMQuaternionSlerp, 위치/스케일을 XMVectorLerp로 처리한다(Animation.cpp:133) — 본 회전은 키 간격이 크면 각속도 왜곡이 관절 움직임의 뚝뚝 끊김으로 보이기 때문에 slerp를 택한 것이다. 꼬리질문 대비: 키프레임 간격이 촘촘하면 nlerp로도 시각 차이가 거의 없어 최적화 후보가 된다는 실무 판단까지.

### Q. SRT 행렬 합성 순서를 설명하고, row-major vs column-major, 좌곱 vs 우곱 규약이 왜 자주 혼동되는지 말하라.

로컬 행렬은 Scale → Rotate → Translate 순으로 적용돼야 한다: 스케일을 먼저 해야 회전축이 왜곡되지 않고, 이동을 마지막에 해야 회전이 이동량을 돌려버리지 않는다. DirectXMath는 row-vector 규약(v' = v·M)이라 코드상 곱셈이 M = S·R·T로 적용 순서 그대로 왼쪽에서 오른쪽으로 읽히고, column-vector 규약(OpenGL 스타일, v' = M·v)에서는 M = T·R·S로 뒤집혀 보인다 — 같은 수학인데 표기 규약 때문에 곱 순서가 반대로 보이는 것이 혼동의 근원이다. 여기에 메모리 배치(row-major/column-major)는 또 별개의 축이라, "수학 규약"과 "저장 순서"를 분리해서 말할 수 있어야 한다. Winters는 DirectXMath row-major·row-vector 규약이며 TransformSystem이 XMMatrixAffineTransformation(scale, 0, rotQuat, pos)으로 SRT를 한 번에 합성한다(TransformSystem.cpp:83). 흔한 실수: 셰이더(HLSL 기본 column-major packing)로 행렬을 올릴 때 transpose 누락, 그리고 부모-자식 계층 합성에서 local·parent와 parent·local을 뒤바꾸는 것.

### Q. 회전을 오일러로 저장하고 행렬화할 때만 쿼터니언을 경유하는 하이브리드 설계의 장단점은? XMQuaternionRotationRollPitchYaw의 회전 순서도 설명하라.

장점: 저장 상태가 사람이 읽고 편집하기 쉬운 오일러(yaw 하나 건드리기 쉬움)이면서, 행렬 합성은 쿼터니언 경유라 수치적으로 깔끔하다. 단점: 3축을 동시에 크게 쓰면 짐벌락과 보간 문제가 오일러 쪽에서 되살아나므로, 이 설계는 "실질적으로 단일 축(yaw) 지배" 도메인에서만 안전하다. XMQuaternionRotationRollPitchYaw는 인자(pitch, yaw, roll)를 받아 내재적(intrinsic) 순서로 합성하며, 회전 순서가 다르면 같은 세 각이라도 최종 자세가 달라진다 — 오일러 각은 "각 3개"가 아니라 "각 3개 + 순서 규약"이 한 세트라는 점이 핵심이다. Winters의 TransformComponent가 정확히 이 하이브리드다: m_LocalRotation(오일러 Vec3, 바디 yaw = .y) 저장 → XMQuaternionRotationRollPitchYaw → XMMatrixAffineTransformation(TransformSystem.cpp:83). 꼬리질문 대비: "그럼 왜 쿼터니언으로 저장하지 않았나?" — 탑다운 MOBA는 바디 회전이 yaw 스칼라 하나라 연속 각도 상태(NearestEquivalent 기법)로 다루는 편이 네트워크 복제·보간·디버깅 모두에서 단순하기 때문이라고 답하면 된다.

### Q. 비균등 스케일이 있는 월드 행렬로 법선을 변환하면 왜 라이팅이 깨지고, inverse-transpose는 왜 이를 고치는가?

법선은 "점"이 아니라 "표면에 수직인 방향"이다. 표면의 접선 t는 월드 행렬 M으로 변환되지만, 수직 조건 n·t = 0을 변환 후에도 유지하려면 법선은 N = (M⁻¹)ᵀ로 변환해야 한다. 유도: n'·t' = 0이 되려면 n'ᵀ(M t) = 0, 즉 n' = (M⁻¹)ᵀ n이면 nᵀ M⁻¹ M t = nᵀ t = 0으로 조건이 보존된다. 기하학적 직관: 비균등 스케일로 표면이 한 축으로 늘어나면 법선은 오히려 그 축에서 **눌려야**(역방향 스케일) 수직이 유지된다 — 그냥 M을 곱하면 법선이 표면을 따라 같이 늘어나 수직이 깨지고 N·L 라이팅이 틀어진다. Winters의 ModelRenderer가 worldInvTranspose = Transpose(Inverse(world))를 상수버퍼로 셰이더에 전달한다(ModelRenderer.cpp:316). 꼬리질문 대비: M이 회전+균등 스케일뿐이면 (M⁻¹)ᵀ가 M의 스칼라배라 그냥 M으로 곱하고 재정규화해도 되는 최적화, 그리고 변환 후 법선 renormalize를 잊는 실수.

### Q. 왼손 좌표계와 오른손 좌표계의 차이, 그리고 +Z forward 규약이 회전 코드 전체에 어떤 영향을 주는가?

두 좌표계는 세 번째 축의 방향이 외적 규약으로 갈린다: 오른손은 x × y = +z(z가 화면 밖), 왼손은 z가 화면 안쪽이다. 회전의 "양의 방향"(축을 따라 봤을 때 시계/반시계)도 이에 따라 뒤집히므로, handedness는 뷰/투영 행렬만이 아니라 sin 부호, 외적 순서, 컬링 와인딩까지 관통하는 전역 규약이다. DirectX 생태계는 전통적으로 LH·+Z forward를 쓰고, Winters도 Mat4 팩토리가 XMMatrixLookAtLH / XMMatrixPerspectiveFovLH 등 LH 계열로 통일돼 있다(WintersMath.h:281). 이 "+Z forward" 선택이 앞서 나온 atan2(x, z) 인자 순서와 dir = (sin, 0, cos) 공식의 근거다 — forward 규약 하나가 방향↔yaw 변환식의 형태를 결정한다. 흔한 함정: RH 기반 DCC 툴(Maya, Blender)이나 glTF 에셋을 LH 엔진에 들여올 때 축 반전/와인딩 반전을 임포트 단계에서 처리하지 않고 런타임 회전 코드에 -1을 흩뿌리는 것 — 다음 질문의 에셋별 yaw 오프셋 문제와 직결된다.

### Q. Winters에서 챔피언마다 body forward 축 보정 오프셋(+PI)이 다른 이유는? 이런 좌표 프레임 불일치를 코드로 어떻게 흡수했나?

에셋의 "모델 공간 forward"는 익스포터·리깅 규약에 따라 제각각이다. Winters의 raw Riot FBX 바디 메시는 -Z를 바라보게 익스포트돼 있어 +PI(180도) yaw 보정이 필요하고, 자체 파이프라인을 거친 고정 .wmesh(Irelia/Yasuo/Viego)는 이미 +Z forward라 보정이 0이다 — 즉 오프셋은 챔피언별 임의값이 아니라 **에셋 패밀리별 규약**이다. 이를 각 호출부의 일회성 +PI 패치로 흩뿌리는 대신 `GetDefaultChampionVisualYawOffset`(ChampionRuntimeDefaults.cpp:39) 한 곳에 카탈로그화하고, 모든 yaw 계산이 `ResolveChampionVisualYawFromDirection` 파사드를 통과하며 오프셋을 더하도록 라우팅했다. 레거시 미니언은 아예 다른 규약으로, `FaceMoveDirection`이 yaw = atan2(-dir.x, -dir.z)로 이동 방향의 **음수**를 향한다(Minion_Manager.cpp:103) — 챔피언 규약을 전역 적용하면 미니언이 전부 뒤돌아 걷는다. 교훈이자 꼬리질문 포인트: "모든 메시의 forward가 같다"고 가정한 전역 yaw 수정은 반드시 사고를 내며, 프레임 불일치는 검증 후 단일 소스(오프셋 함수)에 박제하고 파사드로 강제해야 한다. 또한 projectile/FX 메시의 180도 보정은 바디 facing과 별개 경로로 유지한다.

### Q. yaw를 매 틱 (-PI, PI]로 정규화해 저장하면 어떤 버그가 생기는가? 최단호(shortest-arc) 등가각 기법으로 어떻게 해결하나?

yaw가 +PI 경계 근처에서 진동하는 상황(예: -Z 방향으로 이동하며 빠른 우클릭)을 생각하면, 정규화 저장은 +3.13과 -3.13 사이를 오가게 된다. 각도로는 거의 같은 방향이지만 값으로는 약 2PI 차이라, 이 값을 그대로 보간하거나 스무딩하면 캐릭터가 반대 방향으로 360도 도는 튐이 생긴다. 해법은 목표각을 현재각 기준의 등가각으로 옮기는 것: nearest = reference + NormalizeRadians(target - reference). 델타만 (-PI, PI]로 접고 기준각에 다시 더하므로, 결과는 target과 2PI 배수 차이 나는 등가각 중 reference에 가장 가까운 값 — 항상 최단호 회전이 보장되고 저장값은 (-PI, PI] 밖으로 자유롭게 나간다(연속 상태). Winters의 `NearestEquivalentRadians`(WintersMath.h:199)가 이 구현이고, `MakeChampionVisualYawNear`가 래핑해 Transform 바디 yaw의 모든 쓰기에 사용된다. 실제로 gotchas에 "서버 move tick마다 정규화하면 빠른 우클릭 시 ±PI 경계를 재교차하며 튄다"는 사고가 기록돼 있다. 꼬리질문 대비: 이는 쿼터니언 slerp의 dot 부호 뒤집기(최단경로)와 같은 문제의 스칼라 버전이라는 대응 관계.

### Q. Winters는 "연속 yaw"와 "정규화 yaw"를 왜 분리해서 관리하는가? 각각의 소유 경로를 설명하라.

동일한 yaw라도 용도가 두 가지로 갈린다. (1) **연속 상태**: Transform 바디 회전처럼 프레임 간 연속성이 중요한 값 — 정규화하면 ±PI 경계 재교차 튐이 생기므로, 모든 쓰기가 `ResolveChampionVisualYawNear`/`MakeChampionVisualYawNear`(현재 yaw 기준 최단호 등가각)를 거친다. (2) **표준(canonical) 값**: 네트워크 wire 전송, 로그, 델타 비교처럼 "같은 방향 = 같은 값"이어야 하는 곳 — 여기서만 `NormalizeChampionVisualYaw`/`ResolveChampionVisualYawFromDirection`으로 (-PI, PI]에 접는다. 즉 정규화는 비교·직렬화의 정규형(canonical form)이고, 연속값은 시뮬레이션의 적분 상태라는 역할 분리다. CommandExecutor/MoveSystem/SnapshotApplier가 모든 yaw 쓰기를 이 두 갈래 파사드(ChampionRuntimeDefaults.cpp:56)로 라우팅해 서버 권위 facing을 일관화했다. 흔한 실수: 수신한 wire yaw(정규화값)를 Transform에 직접 대입하는 것 — 반드시 현재 연속 yaw 기준 NearestEquivalent로 옮긴 뒤 저장해야 하고, 반대로 연속값을 정규화 없이 델타 비교/로그에 쓰면 2PI 배수만큼 어긋난 오탐이 난다.

### Q. 서버 권위 게임에서 클라이언트가 예측한 yaw와 서버 스냅샷의 yaw가 충돌할 때, 순간적인 180도 반전 튐을 어떻게 막는가?

클라이언트는 클릭 즉시 예측 yaw로 캐릭터를 돌리지만, 서버 스냅샷은 왕복 지연만큼 과거 상태라 빠른 연속 우클릭 시 stale한 이전 방향이 도착해 예측을 덮어쓰면 캐릭터가 한 프레임 반대를 봤다가 돌아오는 반전 튐이 생긴다. Winters의 방어는 두 겹이다. (1) `IsYawHalfTurn`(MoveSystem.cpp:34, SnapshotApplier.cpp:99에도 동일 헬퍼): |NormalizeChampionVisualYaw(delta)|가 PI에 근접(±0.35rad)하면 반바퀴 판정으로 걸러, 서버가 실제로 반대 방향을 확정하기 전까지 예측 yaw를 보호한다 — 여기서 델타 비교엔 정규화 yaw를 쓰는 것이 앞 질문의 역할 분리와 정합적이다. (2) 예측 보호 해제 조건을 신중히 잡는다: gotchas에 따르면 lastAckedCommandSeq가 진행됐다는 이유만으로 보호를 풀면 안 되고, 서버 yaw가 실제로 따라잡거나 액션 락이 걸릴 때까지 유지해야 한다. 추가로 클라이언트의 로컬 NavigationSystem이 SnapshotApply와 SyncFromECS 사이에서 복제된 yaw를 덮어쓰는 사고가 있어, 복제 챔피언에는 로컬 이동 시스템을 아예 돌리지 않는다. 꼬리질문 대비: "그럼 진짜 뒤도는 입력은?" — 반바퀴 필터는 서버 확정 전 **일시 보호**일 뿐, 서버가 지속적으로 반대 yaw를 보내면 수렴하도록 설계해야 하며, 임계값(0.35rad)은 오탐/미탐 트레이드오프 튜닝 값이라는 점.

### Q. 빠른 연속 우클릭 시 이동 명령 큐가 yaw에 일으키는 문제와, 명령 코얼레싱(coalescing)이 회전 품질에 왜 중요한가?

서버가 클라이언트 명령을 큐에 쌓아 순차 실행하면, 빠른 우클릭 연타 시 이미 의미 없어진 중간 클릭들이 각각 한 틱씩 facing을 바꿔 캐릭터가 지그재그로 스티어링하듯 도는 시각적 노이즈가 된다 — 각 Move가 개별적으로는 올바른 최단호 회전이어도, stale한 목표 자체가 문제다. Winters의 해법은 같은 세션의 pending Move를 최신 Move로 **교체**(코얼레싱)하고 non-move 명령만 순서를 보존하는 것이다(gotchas 2026-05-20 Move coalescing). 이는 회전 수학 단독으로는 못 푸는 문제라는 점이 포인트다: NearestEquivalentRadians가 "어떻게 돌지"를 보장해도 "무엇을 향해 돌지"는 명령 파이프라인 설계가 결정한다. 같은 맥락에서 클라이언트는 보정된 좌표가 아닌 raw 클릭 XZ를 명령에 실어 보내 서버 nav가 목표를 해석하게 하고(원 의도 보존), 직선 이동 가능한 클릭은 경로 첫 waypoint가 아니라 raw 목표 방향으로 초기 yaw를 잡는다 — 첫 waypoint가 클릭 의도와 반대 방향이면 순간 역회전이 보이기 때문이다. 꼬리질문 대비: 이런 "수학 + 파이프라인" 결합 설계를 물으면, yaw 튐 버그는 각도 수식·정규화 시점·명령 큐·스냅샷 적용 순서 네 층 중 어디서든 나올 수 있어 층별로 분리 계측(현재 yaw/목표 yaw/델타를 디버그 오버레이로 노출)하는 것이 정석이라고 답한다.

---

## 렌더링 변환 파이프라인 수학

### Q. 정점 하나가 모델 로컬 좌표에서 화면 픽셀이 되기까지의 전체 변환 흐름을 단계별로 설명해보세요.

**정의**: v_local → (World) → v_world → (View) → v_view → (Projection) → v_clip → (÷w, perspective divide) → v_ndc → (Viewport) → 픽셀 좌표. 정점 셰이더가 책임지는 구간은 clip space까지이고, w-나눗셈과 뷰포트 변환은 래스터라이저의 고정 기능입니다.

**직관**: World는 "물체를 씬에 배치", View는 "카메라 기준 좌표계로 재해석", Projection은 "시야 절두체를 정육면체로 찌그러뜨릴 준비", w-나눗셈은 "멀수록 작게"라는 원근 축소의 실제 실행, 뷰포트는 정규화 좌표를 실제 해상도로 확장하는 단계입니다.

**Winters**: `Shaders/Mesh3D.hlsl:146`의 VS가 `worldPos = mul(float4(pos,1), g_matWorld)` 후 `SV_POSITION = mul(worldPos, g_matViewProj)`로 로컬→월드→클립 2단계 곱을 명시적으로 수행하고, worldPos.xyz는 라이팅용으로 별도 출력합니다. View/Proj는 `CCamera`의 Ready()/SetPerspective()에서 `Mat4::LookAt`/`Mat4::Perspective`로 세팅됩니다.

**함정**: "VS 출력이 NDC"라고 답하면 감점입니다. SV_POSITION은 clip space이고 나눗셈은 그 뒤입니다. 꼬리질문 대비: 클리핑이 나눗셈 전에 일어나는 이유(아래 별도 질문)를 준비하세요.

### Q. 행벡터(row-vector) 규약과 열벡터(column-vector) 규약의 차이는 무엇이고, 행렬 곱 순서에 어떤 영향을 줍니까?

**정의**: 열벡터 규약은 v' = M·v로 변환이 오른쪽→왼쪽(P·V·W·v), 행벡터 규약은 v' = v·M으로 왼쪽→오른쪽(v·W·V·P)입니다. 같은 변환이라면 두 규약의 행렬은 서로 전치 관계입니다.

**직관**: 수학적으로 동치이고 "변환 적용 순서를 코드에서 어느 방향으로 읽을 것인가"의 선택입니다. 단, 메모리 저장 방식(row-major/column-major layout)과 수학 규약(행벡터/열벡터)은 **별개의 축**이며 이 둘을 혼동하는 것이 가장 흔한 사고입니다.

**Winters**: `Engine/Private/Renderer/CCamera.cpp:40`의 GetViewProjection()이 `m_ViewMatrix * m_ProjMatrix` 순서로 곱합니다. DirectXMath 행벡터 규약이라 정점이 v' = v·World·View·Proj로 좌→우 진행하고, HLSL cbuffer를 `row_major matrix`로 선언해 CPU 규약과 일치시켰습니다(`Mesh3D.hlsl`).

**함정**: 교재의 P·V·M·v 표기와 곱 순서가 반대로 보이는 이유를 설명 못 하면 규약을 이해 못 한 것입니다. 또 HLSL 기본 패킹은 column-major라서 `row_major` 지정이나 CPU측 transpose 중 하나를 반드시 해야 하며, 둘 다 하거나 둘 다 안 하면 화면이 뒤틀립니다.

### Q. 뷰 행렬이 왜 "카메라 월드행렬의 역행렬"입니까? LookAt 행렬을 직접 유도해보세요.

**정의**: 카메라를 월드에 배치하는 행렬이 C = R·T라면, 뷰 변환은 세상 전체를 반대로 움직여 카메라를 원점에 두는 변환이므로 View = C⁻¹ = T⁻¹·R⁻¹. R이 정규직교이므로 R⁻¹ = Rᵀ이고, 이동 성분은 (-eye·right, -eye·up, -eye·forward)가 됩니다.

**유도(LH)**: forward = normalize(at - eye), right = normalize(cross(up, forward)), camUp = cross(forward, right)로 정규직교 기저를 만들고, 회전부는 이 기저의 전치, 이동부는 각 축에 eye를 투영한 음수 값입니다.

**Winters**: `Engine/Include/WintersMath.h:281`에서 `Mat4::LookAt = XMMatrixLookAtLH`로 이 유도를 그대로 사용합니다. 카메라 역행렬을 일반 역행렬 계산(가우스 소거)으로 구하지 않고 직교성으로 전치+투영만 하는 것이 핵심입니다.

**함정**: up 벡터와 forward가 평행해지면(수직 아래를 보는 카메라) cross가 0이 되어 기저가 붕괴합니다. 꼬리질문: "카메라가 SRT 스케일을 가지면?" — 스케일이 섞이면 R⁻¹ ≠ Rᵀ이므로 진짜 역행렬이 필요하다고 답해야 합니다.

### Q. 왼손 좌표계와 오른손 좌표계의 차이는 무엇이고, 엔진 전체에서 이 규약이 어디까지 영향을 미칩니까?

**정의**: LH는 +Z가 화면 안쪽(forward), RH는 +Z가 화면 바깥. cross product 결과 방향, 회전의 양의 방향, winding order 해석이 전부 뒤집힙니다. 변환하려면 한 축 부호 반전 + 컬 페이스(winding) 반전이 필요합니다.

**직관**: 좌표계 선택 자체는 임의지만, **한 번 정하면 수학 유틸리티·에셋 파이프라인·게임플레이 코드까지 전파**된다는 점이 중요합니다.

**Winters**: 전 파이프라인이 LH입니다(`WintersMath.h:281`의 LookAtLH/PerspectiveFovLH/OrthographicLH). 이 규약이 게임플레이까지 일관되어 챔피언 forward = (sin yaw, 0, cos yaw), yaw = atan2(x, z)로 방향↔각도를 왕복합니다(`WintersMath.h:178`, `Scene_InGameRender.cpp:135`). yaw=0일 때 +Z를 보므로 atan2 인자가 (x, z) 순서인 것이 포인트입니다.

**함정**: RH 툴(Blender, 일부 FBX 익스포트)에서 온 에셋을 LH 엔진에 넣을 때 축 변환을 빠뜨리면 메시가 좌우 반전되거나 뒤를 봅니다. Winters에서도 raw Riot FBX 바디 메시와 자체 변환 `.wmesh`의 forward 축이 달라 챔피언별 시각 yaw 오프셋이 필요했습니다(뒤 질문 참조).

### Q. 원근 투영 행렬을 FOV, aspect, near, far로부터 유도해보세요. D3D와 OpenGL의 NDC 깊이 범위 차이도 설명하세요.

**정의**: 닮은 삼각형에서 투영 후 y_proj = y_view·cot(fovY/2)/z_view. h = cot(fovY/2), w = h/aspect로 두면 LH 행벡터 규약에서 x' = x·w, y' = y·h, z' = z·far/(far-near) - near·far/(far-near), w' = z_view. 나눗셈 후 z_ndc = far/(far-near)·(1 - near/z_view)로, z=near에서 0, z=far에서 1이 됩니다.

**직관**: 원근의 본질은 "w'에 view-space z를 실어두고 나중에 나눈다"입니다. 행렬 자체는 선형이고, 원근 축소라는 비선형성은 전부 나눗셈이 담당합니다. D3D는 z_ndc ∈ [0,1], OpenGL은 [-1,1]로 매핑하도록 **투영 행렬의 3행 자체가 다릅니다** — API 설정이 아니라 행렬 유도의 차이입니다.

**Winters**: `Mat4::Perspective = XMMatrixPerspectiveFovLH`(`WintersMath.h:281`)로 D3D 0..1 규약을 쓰고, 클립공간 컬링 부등식도 0 ≤ z ≤ w로 이 규약을 따릅니다(`Model.cpp:433`).

**함정**: fovY와 fovX 혼동(aspect 곱하는 방향이 반대), aspect = width/height 정의, near = 0은 z' 유도에서 분모 붕괴로 불가능하다는 것. 꼬리질문: "FOV를 줄이는 것과 카메라를 뒤로 빼는 것(dolly vs zoom)의 차이" — FOV 변경은 원근감 자체를 바꾸고, dolly는 구도만 바꿉니다.

### Q. 직교 투영 행렬은 원근과 무엇이 다르고, 언제 씁니까?

**정의**: view volume이 절두체가 아닌 직육면체이고, 선형 매핑만 합니다. LH에서 z' = z/(far-near) - near/(far-near), w' = 1. w가 항상 1이므로 나눗셈이 항등이 되어 원근 축소가 없습니다.

**직관**: "거리와 무관하게 같은 크기" — UI, 2D, directional light 그림자맵(태양은 무한 원점이므로 평행 투영이 물리적으로 맞음)에 사용합니다. 깊이도 view z에 **선형**이라 원근 투영의 깊이 정밀도 문제가 없습니다.

**Winters**: `Mat4::Orthographic = XMMatrixOrthographicLH`(`WintersMath.h:281`)로 동일 LH 규약을 유지합니다.

**함정**: "직교에서는 perspective divide를 안 한다"는 틀린 답입니다. 하드웨어는 항상 나누지만 w=1이라 결과가 안 변할 뿐입니다. 이 구분이 뒤의 unproject 질문(직교/원근 공통 코드로 역투영 가능한 이유)과 연결됩니다.

### Q. 점은 float4(pos, 1), 법선은 float4(normal, 0)으로 변환하는 이유는 무엇입니까?

**정의**: 동차좌표에서 w 성분이 이동(translation) 행의 기여를 결정합니다. w=1인 점은 이동을 받고, w=0인 벡터는 4x4 행렬의 이동 행이 곱해져도 0이 되어 회전/스케일만 받습니다.

**직관**: 위치는 "어디"이므로 원점 이동에 영향받지만, 방향은 "어느 쪽"이므로 평행이동해도 변하지 않아야 합니다. affine 변환을 하나의 4x4 곱으로 통일하기 위한 장치입니다.

**Winters**: `WintersMath.h:297`이 이 의미 차이를 API로 분리합니다 — TransformPoint = XMVector3TransformCoord(w=1, 원근 w-나눗셈 포함), TransformDirection = XMVector3TransformNormal(w=0, 이동 무시). 셰이더의 `float4(pos,1)` / `float4(normal,0)`(`Mesh3D.hlsl:146,148`)과 정확히 대응합니다.

**함정**: 방향 벡터를 w=1로 변환해 이동이 섞이는 버그가 대표적입니다. 또 TransformNormal은 정규화를 하지 않으므로 스케일이 있으면 길이가 변하고, 비균등 스케일이면 방향 자체가 틀어져 inverse-transpose가 필요합니다(별도 질문).

### Q. 뷰포트 변환의 수식을 쓰고, NDC와 스크린 좌표의 Y축 방향 문제를 설명하세요.

**정의**: sx = (ndc.x + 1)/2 · width + topLeftX, sy = (1 - ndc.y)/2 · height + topLeftY (NDC는 Y-up, 스크린/텍스처는 Y-down이라 반전), depth = MinDepth + ndc.z·(MaxDepth - MinDepth).

**직관**: 파이프라인의 마지막 선형 스케일링입니다. MinDepth/MaxDepth로 NDC z를 깊이버퍼 값으로 재매핑할 수 있어 스카이박스 강제 far(1.0) 같은 트릭도 가능합니다.

**Winters**: `CDX11Device.cpp:772`에서 Viewport MinDepth=0, MaxDepth=1로 D3D 깊이 범위를 확정하고, 매 프레임 깊이를 1.0으로 클리어(line 983), Less/LessEqual 비교(line 193)를 씁니다. Y-down 문제는 GTAO 역투영에서 `ndc.y` 부호 반전으로 나타납니다(`GTAO_CS.hlsl:20`).

**함정**: 텍스처 UV(Y-down)로부터 NDC(Y-up)를 만들 때 부호 반전을 빠뜨리면 화면이 상하 반전된 위치를 샘플링합니다. 꼬리질문: D3D11 픽셀 중심은 (x+0.5, y+0.5)이며 D3D9의 half-pixel offset 문제는 사라졌다는 것.

### Q. 클리핑과 컬링 판정을 왜 NDC가 아니라 clip space(w-나눗셈 전)에서 합니까?

**정의**: clip space 내부 조건은 D3D 기준 -w ≤ x ≤ w, -w ≤ y ≤ w, 0 ≤ z ≤ w의 선형 부등식입니다. 나눗셈 후 NDC 조건(-1 ≤ x ≤ 1 등)과 수학적으로 같아 보이지만, w ≤ 0인 점에서는 동치가 아닙니다.

**직관**: 카메라 뒤(w<0)의 점을 나누면 부호가 뒤집혀 **화면 앞쪽으로 접혀 들어온 것처럼** 보입니다. NDC에서 판정하면 카메라 뒤 오브젝트를 보이는 것으로 오판합니다. 또 clip space는 선형 공간이라 선분 보간·평면 교차 계산이 안전합니다.

**Winters**: `Engine/Private/Resource/Model.cpp:433`의 IsBoundsVisibleInClip이 AABB 8코너를 localToClip으로 변환한 뒤 동차 부등식으로 6평면 바깥 여부를 판정합니다. w ≤ 0.0001로 near plane에 걸친 경우는 보수적으로 visible을 반환합니다 — 잘못 컬면 눈에 띄는 팝핑이지만, 잘못 그리면 약간의 GPU 낭비일 뿐이라는 비대칭 때문입니다.

**함정**: "모든 코너가 **같은 한 평면**의 바깥"일 때만 컬 수 있습니다. 코너들이 서로 다른 평면 바깥에 흩어진 경우는 프러스텀을 대각선으로 지나는 것일 수 있어 컬하면 안 됩니다.

### Q. 프러스텀 컬링을 월드공간 6평면 추출 방식과 클립공간 판정 방식으로 각각 설명하고 트레이드오프를 비교하세요.

**정의**: (1) Gribb/Hartmann — M = View·Proj의 행/열 조합(행벡터 규약이면 4번째 열 ± i번째 열)으로 월드공간 평면 6개를 뽑아 정규화 후, 구는 dot(center, plane) > -radius, AABB는 p-vertex 판정. (2) 클립공간 — 오브젝트별 local→clip 행렬로 바운딩 코너를 변환해 동차 부등식 판정.

**트레이드오프**: 월드 평면 방식은 평면 추출 1회 후 오브젝트당 dot 몇 번이라 대량 오브젝트·공간분할(BVH/옥트리)과 결합하기 좋습니다. 클립 방식은 평면 추출/정규화 코드가 필요 없고 행렬 하나로 판정이 끝나 구현이 단순하며, near=0 같은 API별 깊이 규약이 부등식에 자연히 반영됩니다.

**Winters**: 클립공간 방식을 채택 — `ModelRenderer.cpp:448`에서 matLocalToClip = world·viewProj를 CPU에서 합성해(VS가 하는 곱을 컬링에서 재현) 서브메시별 BuildClipVisibilityMask(`Model.cpp:724`)에 전달합니다. 추가로 `Model.cpp:476`에서 w>0인 코너만 invW = 1/w로 나눠 NDC bbox를 구하고, extent가 0.0035 미만이면 화면상 몇 픽셀도 안 되는 미소 서브메시를 컬합니다 — perspective divide의 실사용 예입니다.

**함정**: 8코너 판정은 보수적(false positive 가능)이지만 잘못 컬지는 않습니다. 꼬리질문: "미소객체 컬은 무엇을 희생하나?" — 화면 기여가 거의 없는 대신, 그림자 캐스터나 피킹 대상이면 별도 경로에서 제외하면 안 된다는 점을 짚으면 좋습니다.

### Q. 깊이 버퍼가 비선형인 이유를 수식으로 보이고, z-fighting과 near/far 배치, Reversed-Z를 설명하세요.

**정의**: z_ndc = A + B/z_view (A = far/(far-near), B = -near·far/(far-near)). 즉 z_ndc는 z_view가 아니라 **1/z_view에 선형**입니다. 정밀도가 near 근처에 집중되고 far 쪽은 극단적으로 뭉칩니다.

**직관**: w' = z_view로 나누는 원근 나눗셈의 부산물입니다. near/far 비율이 정밀도를 지배하므로, far를 반으로 줄이는 것보다 **near를 두 배로 미는 것**이 원거리 정밀도 개선에 훨씬 효과적입니다. z-fighting은 근접한 두 표면의 z_ndc 차이가 깊이버퍼 양자화 간격 아래로 떨어질 때 발생합니다.

**Reversed-Z**: near→1, far→0으로 매핑을 뒤집으면, float의 "0 근처에 지수적으로 조밀한" 표현 분포가 1/z의 far 쪽 뭉침과 상쇄되어 준균등 정밀도를 얻습니다. 투영행렬 수정 + GreaterEqual 비교 + clear 0.0의 3종 세트를 함께 바꿔야 합니다.

**Winters**: 표준 Z 구성입니다 — 깊이 1.0 클리어와 Less/LessEqual 비교(`CDX11Device.cpp:983, :193`), 클립 부등식 0 ≤ z ≤ w. Reversed-Z 전환 시 이 컬링 부등식(`Model.cpp:433`)까지 함께 뒤집어야 한다는 점을 말할 수 있으면 실제로 만져본 사람의 답이 됩니다.

**함정**: Reversed-Z는 float32 깊이 포맷에서만 이득이 크고, 24-bit UNORM에서는 분포가 균등해 효과가 거의 없습니다.

### Q. 법선을 월드로 변환할 때 왜 world 행렬이 아니라 inverse-transpose를 씁니까? 균등 스케일이면 왜 (float3x3)World로 충분합니까?

**정의**: 접선 t가 t' = t·M으로 변환될 때, 법선과의 수직 조건 n'·t' = 0을 유지하려면 n' = n·(M⁻¹)ᵀ이어야 합니다. 유도: n·t = 0에서 n·M⁻¹·M·t = 0이므로 법선 쪽 변환은 (M⁻¹)ᵀ.

**직관**: 비균등 스케일은 표면을 한 축으로 늘리는데, 법선까지 같이 늘리면 표면에 수직이 아니게 됩니다(늘어난 구가 타원이 될 때 법선은 오히려 눌리는 축 쪽으로 꺾여야 함). 균등 스케일 M = R·sI면 (M⁻¹)ᵀ = R·(1/s)로 방향은 회전과 동일하고 크기 차이는 normalize가 흡수하므로 상단 3x3만으로 충분합니다.

**Winters**: 두 경로가 공존합니다 — 정식 경로는 `ModelRenderer.cpp:316`의 UpdateTransform이 worldInvTranspose = transpose(inverse(world))를 CBPerObject에 채우고 셰이더가 `mul(float4(normal,0), g_matWorldInvTranspose)`(`Mesh3D.hlsl:148`)로 변환. 반면 `Default3D.hlsl:47` 디버그 셰이더는 `normalize(mul(v.normal, (float3x3)g_World))` 축약형에 "균등 스케일 가정" 주석을 명시했습니다. 가정을 코드에 문서화한 것 자체가 좋은 관행입니다.

**함정**: w=0으로 넣어 이동 성분을 제거해야 하고(inverse-transpose는 이동의 전치가 4번째 **열**로 옮겨가므로 w=1이면 오염됨), 음수 스케일(미러)에서는 법선과 winding이 함께 뒤집혀 별도 처리가 필요합니다.

### Q. 스크린 좌표 + depth로부터 월드 좌표를 복원(unproject)하는 과정을 설명하세요. 마우스 피킹 레이는 어떻게 만듭니까?

**정의**: 전방 파이프라인의 역연산입니다. uv → ndc.x = u·2-1, ndc.y = -(v·2-1) (텍스처 Y-down 보정), 동차점 h = (ndc.x, ndc.y, depth, 1)·(View·Proj)⁻¹, world = h.xyz / h.w. 마지막 나눗셈이 원근을 복원하는 핵심입니다.

**직관**: 투영은 정보를 잃지 않고 clip space에 접어둔 것이므로, depth만 있으면 역행렬 + w-나눗셈으로 완전히 펼 수 있습니다. 피킹 레이는 같은 uv에 depth=0(near)과 depth=1(far)을 넣어 두 월드점을 역투영하고 그 차를 방향으로 삼으면 됩니다.

**Winters**: `Shaders/SSAO/GTAO_CS.hlsl:20`의 ReconstructWorldPos가 정확히 이 수식으로 픽셀 depth에서 월드좌표를 재구성하며, ViewProjInv는 CBGTAO 상수버퍼로 전달됩니다. G-buffer에 월드좌표를 저장하는 대신 depth에서 재구성해 대역폭을 아끼는 표준 기법입니다.

**함정**: h.w 나눗셈을 빠뜨리면 원근 복원이 안 되어 카메라에서 멀수록 좌표가 틀어집니다. depth가 NDC z(비선형)인지 linear view z인지 반드시 구분해야 하고, Reversed-Z 도입 시 피킹 레이의 near/far depth 값(0과 1)이 뒤바뀝니다.

### Q. SRT(스케일·회전·이동) 월드행렬의 합성 순서를 설명하고, yaw ↔ 방향벡터 변환과 "에셋별 forward 축" 문제를 실제 경험 기반으로 이야기해보세요.

**정의**: 행벡터 규약에서 World = S·R·T, 즉 v' = v·S·R·T로 스케일이 가장 먼저 적용됩니다. 순서를 바꿔 T가 앞에 오면 이동량까지 스케일/회전되어 오브젝트가 엉뚱한 곳으로 갑니다. yaw ↔ 방향은 forward = (sin yaw, 0, cos yaw), yaw = atan2(dir.x, dir.z) — LH에서 yaw=0이 +Z이기 때문에 atan2 인자가 (x, z) 순서입니다.

**직관**: SRT는 "자기 원점에서 크기 잡고 → 돌리고 → 세상에 놓는다"는 물리적 순서 그대로입니다. 탑다운 게임은 회전이 yaw 하나로 수렴하므로 쿼터니언 없이도 각도↔방향 왕복이 잦고, 여기서 규약 불일치가 버그의 온상이 됩니다.

**Winters**: `Scene_InGameRender.cpp:145`의 FlushTransformForRender가 XMMatrixAffineTransformation(scale, quat=RollPitchYaw, pos)로 SRT를 빌드하고, GameplayForwardFromVisualYaw(:135)와 YawFromDirectionXZ(`WintersMath.h:178`)가 yaw↔방향을 왕복합니다. 실전 이슈: 자체 변환 `.wmesh`(Irelia, Yasuo, Viego)는 바디 forward 보정 0, raw Riot FBX 계열은 +PI로 **에셋 패밀리별 forward 축이 달라** 챔피언별 시각 yaw 오프셋(GetDefaultChampionVisualYawOffset)으로 중앙화했습니다. 또 yaw를 매 틱 정규화하면 빠른 우클릭 연타 시 ±PI 경계를 재교차하며 캐릭터가 반대로 도는 문제가 있어, Transform의 yaw는 연속 상태로 유지(ResolveChampionVisualYawNear)하고 정규화는 wire 전송·로그·비교 시점에만 적용하는 규약을 세웠습니다.

**함정**: "정규화는 항상 옳다"는 함정입니다 — 각도는 위상(topology)이 원형이라, 보간·연속성 관점의 표현(연속 누적 yaw)과 표준형 표현(-PI..PI)을 구분해야 합니다. 꼬리질문 대비: gimbal lock은 yaw/pitch/roll 3축 오일러에서 pitch = ±90°일 때 발생하지만, yaw 단일 축만 쓰는 탑다운에서는 원천적으로 문제가 없다는 것까지 말하면 좋습니다.

### Q. VS가 하는 곱셈(world·viewProj)을 CPU에서도 다시 계산하는 경우가 있습니까? 있다면 왜, 그리고 무엇을 조심해야 합니까?

**정의**: CPU 컬링·피킹·LOD 판정은 GPU와 동일한 변환 결과가 필요하므로 local→clip 합성 행렬(MVP)을 CPU에서 재현합니다. M_localToClip = World·View·Proj (행벡터 규약).

**직관**: "GPU가 그릴지 말지를 GPU에 물어보기 전에 CPU가 미리 알아야" 드로우콜 자체를 줄일 수 있습니다. 단, CPU와 GPU가 **같은 행렬·같은 규약·같은 깊이 범위**를 봐야 판정이 일치합니다.

**Winters**: `ModelRenderer.cpp:448`의 RenderFrustumCulled가 matLocalToClip = matWorld · matViewProj를 합성해 BuildClipVisibilityMask에 전달하고, Append/NormalPass 경로(571, 692)도 동일 패턴을 씁니다. 셰이더의 `mul(worldPos, g_matViewProj)`와 정확히 같은 곱을 CPU에서 수행하는 구조입니다.

**함정**: CPU 컬링 행렬과 GPU 상수버퍼 행렬이 한 프레임이라도 어긋나면(업데이트 순서, 카메라 지연) 화면 가장자리에서 오브젝트가 깜빡이며 사라지는 버그가 납니다. 규약 분기(행벡터 CPU vs column-major HLSL 패킹)를 transpose 한 번으로 흡수하는 지점이 어디인지 항상 명확히 해야 합니다.

---

## 셰이딩 / 라이팅 수학

### Q. 램버트 확산광에서 왜 N·L(법선과 광원 방향의 내적)을 쓰는가? saturate는 왜 필요한가?

**정의**: 램버트 확산은 diffuse = albedo * lightColor * max(0, N·L). N은 표면 법선, L은 표면에서 광원을 향하는 단위 벡터다.

**직관**: N·L = cos(theta)는 단위 면적이 받는 광속(빛다발)의 밀도다. 빛이 표면에 비스듬히 들어올수록 같은 양의 빛이 더 넓은 면적에 퍼지므로 cos만큼 어두워진다(포토미터의 코사인 법칙). 확산 반사는 표면 아래에서 산란되어 모든 방향으로 균일하게 나가므로 뷰 방향 V와 무관하다.

**Winters**: `Shaders/BRDF/BRDF_GGX.hlsli`의 `BRDF_CookTorrance`가 최종적으로 `(diffuse + specular) * NoL`을 반환하고, 스타일라이즈드 경로인 `Mesh3D.hlsl`의 `StylizedMapRamp`도 입력이 결국 dot(N, L)이다 — 어떤 셰이딩 모델이든 기하 항의 뿌리는 N·L이다.

**함정**: saturate(clamp to [0,1]) 없이 음수 N·L을 그대로 쓰면 광원 반대편 면이 "음의 빛"으로 색을 빼앗아 간다. 꼬리질문 대비: "N·L을 스펙큘러에도 곱하는 이유는?" — 스펙큘러도 결국 표면이 받은 방사조도에 비례하기 때문이며, BRDF 정의상 렌더링 방정식의 cos 항은 BRDF 바깥에 있다.

### Q. 법선을 월드 공간으로 변환할 때 왜 월드 행렬이 아니라 inverse-transpose를 써야 하는가?

**정의**: 법선 변환 행렬은 (M^-1)^T. 위치는 M으로, 법선은 transpose(inverse(M))으로 변환한다.

**수학적 이유**: 법선의 본질은 "접평면에 수직"이라는 관계다. 접선 T에 대해 N·T = 0이 변환 후에도 유지되려면 N' = (M^-1)^T * N이어야 한다. 유도: N^T * T = 0에서 T' = M*T이므로 N'^T * M * T = 0을 만족하는 N'은 N' = (M^-1)^T * N. 회전+균등 스케일만 있으면 M의 회전부와 (M^-1)^T가 (스케일 배수 차이 빼고) 같아서 문제가 안 보이지만, 비균등 스케일에서는 법선이 표면에서 "기울어진다".

**Winters**: `Engine/Private/Renderer/ModelRenderer.cpp:316-317`에서 CPU가 `XMMatrixTranspose(XMMatrixInverse(world))`로 `g_matWorldInvTranspose`를 만들어 CBPerObject로 업로드하고, `Mesh3D.hlsl:148`, `Mesh3D_PBR.hlsl:72`, `SSAO/NormalOnly.hlsl:32`가 이 행렬로 법선을 변환한다. 반면 `Default3D.hlsl:47`은 "균등 스케일 가정"을 주석으로 명시하고 `(float3x3)g_World`를 그대로 쓴다 — 가정을 코드에 문서화해 둔 케이스.

**함정**: (1) inverse-transpose 후에도 스케일 성분 때문에 길이가 변하므로 셰이더에서 normalize가 여전히 필요하다. (2) 보간(래스터라이저) 후 픽셀 셰이더에서 다시 normalize해야 한다 — 단위 벡터의 선형 보간은 단위 벡터가 아니다. (3) 역행렬 계산을 픽셀마다 하지 말고 Winters처럼 CPU에서 오브젝트당 1회 계산해 상수 버퍼로 올리는 것이 정답.

### Q. 탄젠트 공간과 TBN 행렬은 무엇이고, 노멀맵은 왜 탄젠트 공간에 저장하는가?

**정의**: TBN은 정점의 탄젠트 T, 바이탄젠트 B = cross(N, T)(부호 주의), 법선 N을 행(또는 열)으로 갖는 3x3 기저 행렬. 탄젠트 공간 노멀맵의 텍셀 n_ts를 월드로 옮기려면 n_ws = normalize(n_ts.x*T + n_ts.y*B + n_ts.z*N).

**직관**: 탄젠트 공간은 "표면에 붙어 다니는 로컬 좌표계"다. Z축(파란색)이 항상 기하 법선을 향하므로 노멀맵이 (0,0,1) 근방 값 — 그래서 노멀맵이 파랗다 — 을 저장하게 되고, 같은 텍스처를 어떤 방향의 표면, 어떤 스킨 변형에도 재사용할 수 있다. 오브젝트 공간 노멀맵은 메시 하나에 종속되고 스키닝과 충돌한다.

**Winters**: `Mesh3D.hlsl`의 `VS_INPUT`에 `float3 vTangent : TANGENT`가 정점 포맷으로 예약되어 있다. 현재 라이팅 경로는 스타일라이즈드 diffuse 중심이라 TBN 노멀맵핑을 풀로 쓰진 않지만, PBR 확장 시 정점 포맷 변경 없이 붙일 수 있게 파이프라인이 준비된 상태 — 면접에서는 "포맷은 있고 셰이딩 경로는 아트 방향(LoL풍) 때문에 미사용"이라고 솔직하게 구분해 말하는 편이 좋다.

**함정**: 텍셀 값은 [0,1]로 저장되므로 n = tex*2-1 디코드가 필요하다. 미러링된 UV에서는 B의 부호(handedness, 보통 tangent.w)가 뒤집혀야 하며, 이를 무시하면 좌우 대칭 부위의 요철이 반전된다. T와 N은 보간 후 직교성이 깨지므로 Gram-Schmidt 재직교화(T = normalize(T - N*dot(N,T)))가 관례.

### Q. Blinn-Phong 스펙큘러에서 half-vector H는 무엇이고, 반사 벡터 R을 쓰는 Phong보다 왜 선호되는가?

**정의**: H = normalize(L + V). Blinn-Phong 스펙큘러는 pow(max(0, N·H), shininess). Phong은 pow(max(0, R·V), shininess), R = reflect(-L, N).

**직관**: H는 "빛을 V 방향으로 거울 반사시키려면 마이크로 표면 법선이 향해야 할 방향"이다. 즉 N·H는 "미세면 중 정확히 반사에 기여하는 방향을 향한 비율"의 프록시다. 이 해석 덕분에 Blinn-Phong은 마이크로패싯 이론과 자연스럽게 연결되고, grazing angle에서 하이라이트가 길게 늘어나는 실측 현상을 Phong보다 잘 재현한다. 계산상으로도 방향광이면 H를 픽셀마다 reflect 계산 없이 싸게 얻는다.

**Winters**: 이 half-vector 개념이 그대로 승격된 것이 `BRDF_GGX.hlsli:45`의 `H = normalize(V + L)`이고, `D_GGX(NoH, roughness)`의 NoH가 Blinn-Phong의 N·H와 같은 자리에 있다 — Blinn-Phong의 pow 로브를 물리 기반 NDF로 교체한 구조라고 설명하면 연결이 깔끔하다.

**함정**: shininess 지수는 러프니스와 반비례하는 비선형 관계이고 에너지 보존이 없어서, 지수를 올리면 하이라이트가 좁아지기만 하고 밝기 총량이 보존되지 않는다. 정규화 계수 (n+8)/(8*PI)를 곱하는 normalized Blinn-Phong이 그 보정이라는 것까지 말하면 가산점.

### Q. 라이팅은 왜 선형 공간에서 해야 하는가? Winters는 하드웨어 sRGB 포맷 대신 어떤 방식을 택했고 트레이드오프는 무엇인가?

**정의**: sRGB는 인간 지각에 맞춘 비선형 인코딩(근사 pow 2.2)이다. 라이팅 연산(덧셈, 곱셈, 보간)은 물리량인 방사휘도에 대한 선형 연산이므로, 텍스처를 pow(c, 2.2)로 선형화 → 선형 공간에서 라이팅 → 출력 직전 pow(c, 1/2.2)로 재인코딩해야 한다.

**직관**: 감마 공간에서 0.5 + 0.5를 하면 물리적으로는 0.5^2.2 + 0.5^2.2 ≈ 0.43의 빛을 더한 것이라 하이라이트가 뭉개지고 중간톤이 탁해진다. "빛 두 개를 켜면 두 배 밝아야 한다"가 선형 공간에서만 성립한다.

**Winters**: `eTexColorSpace::ShaderLocalSRGB` 정책(`Engine/Public/Resource/Texture.h:10`)으로, 텍스처를 `DDS_LOADER_IGNORE_SRGB` / `WIC_LOADER_IGNORE_SRGB`로 R8G8B8A8_UNorm 그대로 올리고(`Texture.cpp:177-208`, `RHITextureLoader.cpp:138`) 셰이더의 `SrgbToLinearApprox`(pow 2.2, `Mesh3D.hlsl:51-59`)로 디코드, `LinearToSrgbApprox`로 인코드한다. **트레이드오프**: 하드웨어 `_SRGB` 포맷은 (1) 정확한 조각별 sRGB 커브, (2) 텍스처 필터링이 선형화 이후에 수행됨(bilinear 보간이 올바른 공간에서 일어남), (3) 공짜라는 장점이 있다. 셰이더 국소 변환은 필터링이 감마 공간에서 일어나는 부정확성과 ALU 비용을 감수하는 대신, 포맷 분기 없이 모든 로더/RTV 경로가 UNORM 하나로 통일되고 어떤 텍스처가 색 텍스처인지 정책을 셰이더 코드에서 명시적으로 볼 수 있다는 파이프라인 단순성을 얻은 선택이다.

**함정**: 노멀맵, 러프니스, 마스크 같은 데이터 텍스처에 sRGB 디코드를 걸면 안 된다 — 이것들은 색이 아니라 수치다. 또한 pow 2.2는 근사일 뿐 진짜 sRGB는 저휘도 구간이 선형인 조각 함수라는 것, 알파 채널은 감마 인코딩 대상이 아니라는 것이 단골 꼬리질문.

### Q. Cook-Torrance 스펙큘러 BRDF의 D, G, F 각 항은 무엇을 의미하고, GGX NDF가 Blinn-Phong보다 선호되는 이유는?

**정의**: specular = (D * F * G) / (4 * N·V * N·L).
- D(NDF): 미세면 법선이 H를 향하는 통계적 밀도. GGX(Trowbridge-Reitz)는 D = a^2 / (PI * ((N·H)^2 * (a^2 - 1) + 1)^2), a = roughness^2(Disney 규약).
- G(기하항): 미세면끼리의 셀프 섀도잉/마스킹으로 빛이 차단되는 비율. Smith 분리형 G = G1(N·V) * G1(N·L), Schlick-GGX 근사 G1(x) = x / (x*(1-k) + k).
- F(프레넬): 입사각에 따른 반사율 증가. Schlick 근사 F = F0 + (1 - F0) * (1 - V·H)^5.
- 분모 4*N·V*N·L: 미세면 공간과 매크로 표면 공간 사이의 야코비안(측도 변환) 보정.

**GGX가 선호되는 이유**: GGX는 분포의 꼬리가 다항식적으로 길어서, 하이라이트 중심은 또렷하면서 주변으로 은은하게 번지는 "긴 꼬리(long tail)" 하이라이트를 만든다. Beckmann이나 Blinn-Phong의 지수적 감쇠는 하이라이트가 칼같이 끊겨 플라스틱처럼 보인다. 실측 MERL 데이터와의 일치도도 GGX가 우수하다.

**Winters**: `Shaders/BRDF/BRDF_GGX.hlsli:7-63`에 D/G/F가 함수 단위로 분리 구현되어 있고, G의 k = (r+1)^2 / 8은 직접광용 Karis(UE4) 리매핑, 분모에는 `+ 1e-5` 엡실론으로 N·V나 N·L이 0일 때의 0-나눗셈을 방지한다. 항이 분리되어 있어 각 항을 단독 시각화하며 디버깅할 수 있는 구조라는 점을 어필할 수 있다.

**함정**: k 리매핑은 직접광(analytic light)용이고 IBL에서는 k = a^2/2를 쓴다는 것, roughness를 제곱해 a로 쓰는 것은 지각적 선형성을 위한 Disney 규약이라 다른 엔진 에셋과 러프니스 값을 그대로 교환하면 안 된다는 것이 실무형 꼬리질문.

### Q. 에너지 보존이란? kD = (1-F)(1-metallic)와 Lambert 디퓨즈를 PI로 나누는 이유를 설명하라.

**정의**: 표면이 반사하는 총 에너지가 입사 에너지를 초과하면 안 된다. 입사광은 반사(스펙큘러, 비율 F)와 굴절(디퓨즈로 재방출) 중 하나이므로 디퓨즈 몫은 kD = 1 - F. 금속은 굴절된 빛을 전부 흡수하므로 추가로 (1 - metallic)을 곱한다. Lambert BRDF는 albedo / PI — 반구 전체에 대해 cos 가중 적분 integral(cos * dOmega) = PI이므로, 나누지 않으면 albedo 1인 표면이 받은 것보다 PI배 많은 에너지를 내놓는다.

**직관**: PI 나눗셈은 "한 점에 들어온 빛이 반구의 모든 방향으로 분배된다"는 정규화다. kD = (1-F)는 "프레넬이 가져간 나머지만 표면 속으로 들어간다"는 회계 장부다. 이걸 지키면 아티스트가 라이트 강도를 올려도 재질 간 상대 밝기가 무너지지 않는다.

**Winters**: `BRDF_GGX.hlsli:51-62`에서 F0 = lerp(0.04, albedo, metallic)로 유전체 기본 반사율 4% 규약을 쓰고, kD = (1-F)*(1-metallic), diffuse = kD * albedo / PI를 그대로 구현했다. metallic = 1이면 디퓨즈가 정확히 0이 되고 albedo가 F0(반사 색)로 역할이 바뀐다 — 메탈릭 워크플로우에서 albedo 텍스처 하나가 두 의미를 갖는 이유다.

**함정**: "라이트 강도에 PI를 미리 곱해 두는 관례(punctual light의 PI 상쇄)" 때문에 엔진마다 밝기가 PI배 차이 나는 사고가 흔하다. 또 kD에 스칼라 (1-metallic)만 곱하고 (1-F)를 빼먹으면 grazing angle에서 에너지가 초과된다. 0.04는 굴절률 1.5인 유전체의 수직 입사 반사율 ((n-1)/(n+1))^2에서 온 값이라는 유도까지 말하면 좋다.

### Q. 림 라이트의 pow(1 - N·V, p)와 Schlick 프레넬은 어떤 관계인가?

**정의**: Schlick 프레넬은 F = F0 + (1 - F0) * (1 - cos)^5로, 시선이 표면과 평행해질수록(grazing) 반사율이 1로 치솟는 물리 현상의 근사다. 림 라이트 rim = pow(1 - saturate(N·V), p)는 이 grazing 항 (1-cos)^n에서 물리 계수를 떼고 지수 p를 아트 파라미터로 열어 둔 스타일라이즈드 변형이다.

**직관**: 실루엣 근처에서는 N과 V가 수직에 가까워 N·V가 0으로 가고, 1 - N·V가 1로 가므로 윤곽선이 밝아진다. 물리적으로도 grazing에서 모든 물질의 프레넬 반사율이 1에 수렴하므로, 림 라이트는 "프레넬의 카툰화"라고 요약할 수 있다.

**Winters**: 같은 수식이 세 곳에서 목적을 바꿔 재사용된다 — `FxMesh.hlsl:76-81`의 `ComputeRim`은 이펙트 실루엣 발광(rimColor * rim * intensity 가산), `Mesh3D.hlsl:137`은 호버 아웃라인(pow 2.35 + smoothstep(0.28, 0.88) 마스크로 챔피언 선택 강조), `FxSprite.hlsl:70-74`는 3D 법선이 없는 스프라이트라 localUV의 사각 가장자리 거리로 N·V를 대체한 2D 림 변형이다. 한 수학이 물리(F_Schlick), 게임플레이 UI(호버), FX(림)로 갈라지는 좋은 사례.

**함정**: 림을 가산하면 어두운 씬에서 HDR 파이프라인 없이 화이트가 뜬다. 또 N·V 기반 림은 평평한 면이 카메라를 정면으로 볼 때 전부 어두워지고 구형 표면에서만 예쁘게 나온다 — 박스형 메시에 림을 걸면 면 단위로 껌뻑이는 이유를 물을 수 있다.

### Q. Reinhard 톤 매핑 x/(1+x)의 특성과 한계는? 감마 인코딩과의 순서는?

**정의**: HDR 라이팅 결과 x를 LDR로 압축하는 커브 mapped = x / (1 + x). x가 작으면 거의 선형(x에 근사), x가 무한대로 가면 1에 점근 — 절대 클리핑되지 않는 소프트 롤오프.

**직관**: 필름의 숄더(shoulder)를 흉내 내는 가장 단순한 유리 함수다. 어떤 밝기의 빛도 화면 [0,1]로 "구겨 넣는" 압축기이며, 미분이 항상 양수라 밴딩 없는 단조 매핑이다.

**Winters**: `Mesh3D_PBR.hlsl:129-130`(및 `Skinned3D_PBR.hlsl:148`)에서 PBR 라이팅 결과를 `color / (color + 1)`로 압축한 뒤 `pow(color, 1/2.2)`로 sRGB 인코딩한다. 순서가 핵심 — 톤 매핑은 물리량(선형 HDR)에 대한 연산이므로 반드시 감마 인코딩 **전에** 수행해야 한다.

**한계/꼬리질문**: Reinhard는 (1) 채널별로 적용하면 밝은 빛의 채도가 빠지며 흰색으로 탈색되고(desaturation), (2) 토(toe)가 없어 암부 대비가 밋밋하며, (3) 최대 밝기 제어(white point)가 없다. ACES/필믹 커브는 토+숄더의 S자 커브로 대비와 채도 반응이 필름에 가깝다 — "왜 요즘 엔진은 ACES를 기본으로 쓰나"에 대한 답. 확장 Reinhard x*(1 + x/W^2)/(1+x)로 white point W를 도입하는 개선안까지 말하면 좋다.

### Q. 알파 블렌딩 방정식을 쓰고, straight alpha / premultiplied alpha / additive의 차이와 프리멀티플라이가 유리한 이유를 설명하라.

**정의**:
- straight: out = src.rgb * src.a + dst.rgb * (1 - src.a) — D3D11로 SRC_ALPHA / INV_SRC_ALPHA.
- premultiplied: out = src.rgb + dst.rgb * (1 - src.a) — ONE / INV_SRC_ALPHA, 단 src.rgb에 알파가 미리 곱해져 있어야 함.
- additive: out = src.rgb * src.a + dst.rgb — SRC_ALPHA / ONE. 빛을 "더하기만" 하므로 어두워질 수 없다.

**프리멀티플라이가 유리한 이유**: (1) **필터링 정확성** — straight 텍스처에서 알파 0 텍셀의 RGB(보통 검정/쓰레기 값)가 bilinear 보간에 섞여 검은 테두리(dark fringe)가 생기는데, 프리멀티는 알파 0이면 RGB도 0이라 보간이 항상 옳다. (2) **합성의 결합법칙** — 프리멀티 오버 연산자는 결합법칙이 성립해 레이어를 미리 합쳐 캐시할 수 있다. (3) **한 스테이트로 두 모드** — 프리멀티 상태에서 알파를 0으로 두면 additive, 1로 두면 오버가 되어 한 드로우 배치 안에서 블렌드를 픽셀 단위로 보간할 수 있다.

**Winters**: `Engine/Private/RHI/DX11/BlendStateCache.cpp:28-69`가 `eBlendPreset`(AlphaBlend = SRC_ALPHA/INV_SRC_ALPHA, PremultipliedAlpha = ONE/INV_SRC_ALPHA, Additive = SRC_ALPHA/ONE)의 D3D11 스테이트 캐시를 소유하고, FX 셰이더는 스타일 모드에서 출력 직전 `finalRGB *= alpha`(`FxMesh.hlsl:161, 193`)로 **셰이더 측 프리멀티플라이**를 수행한다 — 텍스처를 굽는 대신 셰이더에서 곱하는 규약이라 에셋은 straight로 유지된다.

**함정**: 프리멀티 텍스처에 straight 블렌드 스테이트를 걸면 알파가 두 번 곱해져 반투명이 과하게 어두워진다(그 역은 밝은 테두리). 렌더 스테이트와 콘텐츠(텍스처/셰이더 출력)의 규약이 반드시 쌍으로 맞아야 하고, Winters처럼 스테이트를 프리셋 enum으로 중앙화하면 이 불일치를 리뷰에서 잡기 쉽다.

### Q. 반투명 렌더링은 왜 back-to-front 정렬과 depth write off가 필요한가? additive는 왜 순서에 독립인가?

**정의**: 오버 블렌딩 out = src*a + dst*(1-a)는 dst에 의존하는 **비가환 연산**이라 A over B ≠ B over A. 올바른 결과는 카메라에서 먼 것부터 그려야 나온다. depth write를 켠 채 반투명을 그리면 앞의 반투명이 z-buffer를 채워 뒤의 반투명/불투명이 depth test에서 잘려나간다.

**직관**: 반투명은 "유리를 겹쳐 보는" 물리라 겹침 순서가 곧 결과다. depth test(read)는 유지해 불투명 뒤에 숨은 파티클은 가리되, write는 꺼서 반투명끼리는 서로를 자르지 않게 한다.

**additive가 순서 독립인 이유**: out = src + dst는 덧셈이고 덧셈은 가환+결합이므로 어떤 순서로 더해도 합이 같다. dst 계수가 ONE이라 이전 프레임버퍼 값이 감쇠되지 않기 때문이다. 그래서 스파크, 마법 글로우 같은 "빛" 이펙트는 additive로 두면 정렬 비용을 통째로 생략할 수 있다.

**Winters**: FX 파이프라인이 `eBlendPreset::Additive`(SRC_ALPHA/ONE)를 이펙트에 쓰는 이유가 정확히 이것 — 다수의 겹치는 스킬 이펙트 쿼드를 파티클 단위 정렬 없이 던져도 결과가 결정적이다. 반면 연기 같은 흡광(오버) 이펙트는 정렬이 필요해서, 블렌드 프리셋 선택이 곧 정렬 정책 선택이 된다.

**함정**: additive는 겹칠수록 무한히 밝아져 HDR+톤 매핑이 없으면 흰색으로 포화된다. "순서 독립 반투명(OIT)을 아나?"가 꼬리질문 단골 — depth peeling, per-pixel linked list, weighted blended OIT를 개념 수준으로 언급할 수 있어야 한다.

### Q. clip() / discard는 early-Z에 어떤 영향을 주는가? 언제 알파 블렌드 대신 알파 테스트를 쓰는가?

**정의**: clip(x)는 x < 0이면 픽셀을 폐기한다. GPU는 원래 픽셀 셰이더 **전에** depth/stencil 테스트(early-Z)를 수행해 가려진 픽셀의 셰이딩을 건너뛰는데, 셰이더가 픽셀을 폐기할 수 있으면(discard 사용, 또는 SV_Depth 출력) "셰이더가 끝나야 그 픽셀이 depth를 쓸지 알 수 있으므로" depth **write**를 셰이더 이후로 미뤄야 하고, 그 드로우와 이후 겹치는 픽셀의 early-Z 효율이 떨어진다.

**직관**: early-Z는 "결과를 안 봐도 되는 픽셀은 계산하지 않는다"는 계약인데, discard는 "결과를 봐야 안다"로 계약을 깨는 것이다.

**Winters**: `Mesh3D.hlsl:158`의 `clip(texColor.a - 0.05f)` 알파 컷아웃, `FxMesh.hlsl`의 dissolve `clip(dissolved + edgeWidth)`와 erode 경로의 노이즈 임계값 미만 discard(`FxMesh.hlsl:171-176`)가 이 트레이드오프를 안고 있는 지점들이다. FX는 어차피 블렌딩 패스(depth write off)라 부담이 적지만, 불투명 메시의 알파 컷아웃은 early-Z 손해를 알고 쓰는 것이다.

**함정**: 알파 테스트는 depth write가 가능해 **정렬이 필요 없고** 픽셀이 "있다/없다"로 확정되는 장점(식생, 철망)이 있어 early-Z 손해와 맞바꾼다. 밉맵에서 알파가 뭉개져 멀리서 잎이 사라지는 문제(알파 커버리지 보정)와, 컷아웃 가장자리 앨리어싱(alpha-to-coverage로 완화)이 대표 꼬리질문.

### Q. 카메라를 향하는 빌보드의 월드 행렬을 카메라 기저 벡터로 어떻게 구성하는가? 구면 vs 원통 빌보드 차이는?

**정의**: 뷰 행렬의 역(카메라 월드 행렬)에서 camRight, camUp, camFwd를 뽑아, 쿼드의 로컬 축이 이 벡터들과 정렬되도록 회전 행렬의 행(row-major 기준)에 그대로 넣는다. "카메라를 향한다"는 회전을 삼각함수 없이 정규직교 기저 치환으로 얻는 것이다.

**Winters**: `Client/Private/GameObject/FX/FxSystem.cpp:385-397`에서 XZ 평면 쿼드를 기준으로 row0 = camRight * width, row1 = -camFwd(쿼드의 로컬 Y = 법선이 카메라를 향하도록 전방의 음수), row2 = camUp * height, row3 = 위치를 넣어 스케일 포함 기저 행렬을 직접 조립한다. 쿼드가 XY가 아니라 XZ 평면이라 두 번째 행이 up이 아닌 -fwd인 것이 포인트 — "로컬 기하가 어느 평면에 정의됐는지"가 행 배치를 결정한다. 비빌보드 데칼은 표준 SRT(Scale * RotY(fYaw) * Translation)로 분기한다.

**구면 vs 원통**: 구면(spherical)은 위처럼 카메라 기저를 통째로 쓰는 완전 페이싱 — 폭발, 글로우. 원통(cylindrical)은 up을 월드 Y로 고정하고 right = normalize(cross(worldUp, toCam)), fwd = cross(right, worldUp)로 yaw만 회전 — 나무, 세로로 선 캐릭터 스프라이트가 카메라가 내려다볼 때 눕지 않게 한다.

**함정**: (1) 카메라 **위치를 향하는**(point-at) 빌보드와 카메라 **평면에 평행한**(view-plane aligned) 빌보드는 다르다 — 화면 가장자리에서 전자는 미묘하게 비틀린다. Winters 방식은 view-plane aligned라 파티클 무리가 균일하게 보인다. (2) 스케일을 기저 벡터에 직접 곱했으므로 이 행렬로 법선을 변환하면 안 된다(비균등 스케일 문제로 회귀). (3) row-major/column-major와 mul 순서 규약을 틀리면 전치된 회전이 나온다.

### Q. Half-Lambert / wrap lighting은 무엇이고, LoL풍 스타일라이즈드 라이팅에서 왜 쓰는가?

**정의**: 표준 N·L은 [−1,1]을 [0,1]로 자르지만(saturate), wrap lighting은 wrapped = (N·L + w) / (1 + w)로 음영 경계를 광원 반대편으로 밀어 넣는다. Valve의 half-Lambert는 w = 1에 결과 제곱을 취한 특수형(N·L*0.5+0.5)^2. 툰 램프는 이 값을 smoothstep이나 룩업 텍스처로 재매핑해 밴드를 만든다.

**직관**: 물리적으로는 표면하 산란/바운스 광의 값싼 흉내로, terminator(명암 경계)를 부드럽게 하고 그림자면이 새까매지는 것을 막는다. 스타일라이즈드 게임에서 캐릭터 실루엣 가독성 = 게임플레이 가독성이므로, 물리 정확성보다 "그림자 속 정보 보존"을 산다.

**Winters**: `Mesh3D.hlsl:68-127`이 정확히 이 계열이다. `StylizedMapRamp`는 wrapped = (ndotl + 0.34)/(1 + 0.34) 후 smoothstep(0.06, 0.86)과 lerp(soft*soft, soft, 0.55)로 소프트 램프를 만들고, `ApplyStylizedDiffuse`는 이 key로 shadowTint(푸른 그림자)↔keyTint를 lerp, N.y 기반 상/하 앰비언트 그라디언트, pow(1-N·V, 4) grazing 라이트, 스크린 AO(lerp 0.52~1.0)를 합성한다 — 그림자를 "어둡게"가 아니라 "차가운 색으로" 처리하는 LoL풍 diffuse-only 규약. 포인트 라이트도 half-Lambert형 facing = smoothstep(dot(N,L)*0.5+0.5)에 0.08 스케일로 액센트만 얹는다(`Mesh3D.hlsl:89-95`). FX 쪽 셀 모드는 반대로 N·L을 1.0/0.55/0.18 두 단계 하드 컷으로 **양자화**한다(`FxMesh.hlsl:103-108`) — 같은 N·L 리매핑이 "부드럽게"와 "딱딱하게" 양방향으로 쓰인다. 스프라이트 셀 셰이딩은 법선이 없어 Rec.601 휘도 dot(rgb, (0.299, 0.587, 0.114))를 뽑아 smoothstep 셀 밴드로 대체한다(`FxSprite.hlsl:96-101`).

**함정**: wrap lighting은 에너지를 추가하는 것이라 PBR 파이프라인에 그대로 섞으면 에너지 보존이 깨진다. 꼬리질문으로 "Rec.601과 Rec.709 휘도 계수 차이"가 나올 수 있다 — 계수는 각 색공간 프라이머리의 실제 휘도 기여율이고, 초록 계수가 가장 큰 이유는 인간 시각이 초록 파장에 가장 민감해서다(0.587 vs 709의 0.7152; sRGB 콘텐츠에는 엄밀히 709 계수가 맞다).

### Q. 포인트 라이트 감쇠에서 물리적 inverse-square 대신 (1 - d/r)^2 같은 반경 기반 falloff를 쓰는 이유는?

**정의**: 물리 법칙은 1/d^2(구 표면적 4*PI*d^2로 광속이 퍼지므로). 하지만 1/d^2은 d가 커져도 **절대 0이 되지 않아** 모든 라이트가 이론상 무한 범위를 갖는다. 게임에서는 att = saturate(1 - d/radius)^k(또는 UE식 (saturate(1-(d/r)^4))^2 / (d^2+1)) 같은 반경 기반 커브로 radius에서 정확히 0에 수렴시킨다.

**직관**: 라이트 컬링(타일드/클러스터드, 또는 단순 반경 컬링)은 "이 라이트는 이 범위 밖에 영향 없음"이라는 보장이 필요하다. 1/d^2를 반경에서 그냥 자르면 경계에 밝기 불연속(하드 컷)이 원형 띠로 보인다. (1-d/r)^2는 값과 1차 미분이 모두 r에서 0으로 수렴해 경계가 보이지 않는다.

**Winters**: `Mesh3D_PBR.hlsl:87-96`이 attenuation = saturate(1 - d/radius)를 제곱한 스무스 falloff를 쓰고, 스타일라이즈드 경로(`Mesh3D.hlsl:89-95`)는 같은 감쇠에 half-Lambert facing과 0.08 액센트 스케일을 곱한다. 상수 버퍼의 `PointLightData.fRadius`가 셰이딩 감쇠와 (향후) 컬링 반경의 단일 소스가 되는 구조다.

**함정**: 물리 기반 렌더러에서 비물리 감쇠를 쓰면 라이트 강도의 단위(루멘/칸델라)가 무의미해져 아트 튜닝이 라이트마다 제각각이 된다 — "물리 1/d^2에 windowing 함수(반경 마스크)를 곱해 둘을 결합"하는 것이 현대 엔진(Frostbite/UE)의 절충안이라는 것까지 말하면 좋다. d=0 특이점 방지용 분모 클램프(max(d, 0.001) — Winters도 `Mesh3D.hlsl:90`에서 수행)도 잊기 쉬운 디테일.

### Q. 깊이 버퍼만으로 월드/뷰 좌표를 복원(역투영)하는 과정을 설명하라. GTAO류 수평선 기반 AO는 무엇을 적분하는가?

**정의(역투영)**: 스크린 UV와 depth로부터 NDC를 만들고 — x_ndc = uv.x*2-1, y_ndc = 1-uv.y*2(y 반전 주의), z_ndc = depth — clip = (x_ndc, y_ndc, z_ndc, 1)에 inverse(ViewProj)를 곱한 뒤 **w로 나누면**(perspective divide의 역) 월드 좌표가 나온다. 투영은 비선형(동차) 변환이라 역행렬 곱만으로는 안 되고 w 나눗셈이 필수라는 것이 핵심.

**정의(수평선 기반 AO)**: 각 픽셀에서 여러 스크린 방향으로 행진하며 "지평선이 얼마나 올라와 있는가"를 찾아, 법선 반구 중 열린 입체각의 비율을 근사한다. AO = 1/PI * integral(V(omega) * cos, dOmega)의 스크린 스페이스 근사.

**Winters**: `Shaders/SSAO/GTAO_CS.hlsl:20-92`의 `ReconstructWorldPos`가 위 역투영을 그대로 수행하고, 8방향 x 4스텝 샘플로 horizon = saturate((N·sampleDir - thickness)/(1 - thickness))에 거리 가중 (1 - d/R)을 곱해 가시성을 누적, 최종 pow(ao, intensity)로 대비를 조정한다. 법선은 G-buffer에 n*0.5+0.5로 인코딩(`NormalOnly.hlsl:40`)했다가 *2-1로 디코드(`GTAO_CS.hlsl:45`)하며, 그 결과 AO가 스타일라이즈드 디퓨즈의 `lerp(0.52, 1.0, ao)` 접촉 음영(`Mesh3D.hlsl:124`)으로 소비된다 — 컴퓨트 생산, 포워드 소비의 파이프라인. 참고로 텍스처 없는 대안으로 `ContactShadowPlane.hlsl:55-68`은 UV를 [-1,1]로 센터링하고 비등방 스케일(0.82, 1.18) 타원 거리 제곱 d2에 broad = 1-smoothstep(0.18, 1, d2), core = 1-smoothstep(0, 0.36, d2)를 0.54/0.18 가중 합성한 해석적 블롭 그림자를 쓴다 — "샘플링 대신 닫힌 식"이라는 정반대 접근.

**함정**: (1) depth의 y 반전과 D3D NDC z 범위 [0,1](GL은 [-1,1]) 혼동이 역투영 버그 1순위. (2) n*0.5+0.5 8비트 인코딩은 정밀도가 낮아 매끈한 표면에 밴딩을 만든다 — 옥타헤드럴 인코딩이 2채널로 균일한 정밀도를 주는 대안. (3) 스크린 스페이스 AO는 화면 밖/가려진 기하를 모르므로 카메라 이동 시 AO가 "끓는" 한계를 인지하고 있어야 한다.

### Q. 노이즈 기반 dissolve(디졸브) 이펙트의 수학을 설명하라. 타는 가장자리와 UV 왜곡은 어떻게 만드는가?

**정의**: 노이즈 텍스처 값 n(uv)에 대해 dissolved = n - progress(진행도)로 정의하고, dissolved < 0인 픽셀을 discard하면 노이즈의 등고선을 따라 표면이 침식된다. 가장자리 발광은 edgeMask = 1 - smoothstep(0, edgeWidth, dissolved)로 "곧 사라질 얇은 띠"를 분리해 에미시브 색을 입힌다. UV 왜곡은 노이즈 채널을 [-1,1]로 리매핑해 uv += distortVec * strength로 샘플 좌표 자체를 흔든다.

**직관**: threshold 스칼라 하나가 "시간"이고, 노이즈가 "어느 픽셀이 먼저 죽는지"의 지도다. smoothstep 에지는 클리프(불연속) 대신 미분 연속인 경계를 줘서 앨리어싱을 줄인다.

**Winters**: `FxMesh.hlsl:117-163`의 `ApplyMagicSurface`가 풀 조합이다 — 2겹 스크롤 UV 노이즈를 0.7/0.3 가중 옥타브로 합성하고 pow로 대비를 세운 뒤, 노이즈 R채널 기반 UV 왜곡, dissolved = n - age*speed에 `clip(dissolved + edgeWidth)`로 소멸, edgeMask로 타는 가장자리 색 분리, 중심 마스크 pow(1 - length(uv-0.5)*2, p)까지 수행한다. 단순 erode 경로는 임계값 미만 discard만 한다(`FxMesh.hlsl:171-176`). 스프라이트 쪽은 법선/노이즈 대신 Rec.601 luma를 erode 마스크로 재사용한다(`FxSprite.hlsl:158`).

**함정**: (1) age*speed 진행을 프레임 시간이 아닌 누적 age로 파라미터화해야 프레임레이트 독립이 된다. (2) 옥타브 스크롤 속도가 정수비면 패턴 반복이 눈에 띈다 — 0.7/0.3처럼 비정수 가중과 서로 다른 스크롤 방향으로 깨는 것. (3) discard 기반이라 early-Z 트레이드오프(위 문항)와 연결되고, 에지 에미시브는 additive 성분이라 톤 매핑 유무에 따라 포화가 달라진다 — 꼬리로 "이 이펙트를 프리멀티 블렌드 하나로 통합할 수 있나?"(알파 0 = additive 트릭)까지 이어질 수 있다.

---

## 텍스처링 / 샘플링 수학

### Q. UV 좌표란 무엇이고, 텍셀과 픽셀은 어떻게 매핑되는가? 텍셀 중심이 왜 중요한가?

UV는 텍스처 이미지를 해상도와 무관하게 참조하기 위한 정규화 좌표계로, u,v ∈ [0,1]이 텍스처 전체를 덮는다. W×H 텍스처에서 텍셀 인덱스 x의 중심은 u = (x + 0.5) / W — 텍셀은 점이 아니라 면적을 가진 샘플이므로 "+0.5"가 텍셀 중심을 가리킨다. 기하학적으로 UV는 "텍스처 평면 위의 연속 좌표"이고, 샘플러는 그 좌표 주변 텍셀들을 필터 규칙으로 섞는 함수다. D3D11은 래스터라이저의 픽셀 중심도 (x+0.5, y+0.5)로 정의하므로 풀스크린 쿼드에서 UV를 정확히 맞추면 1:1 텍셀-픽셀 매핑이 성립한다(D3D9의 half-pixel offset 문제는 D3D11에서 사라짐). Winters에서는 `Mesh3D_PBR.hlsl:110-111`이 이 관계를 그대로 사용한다 — `aoUV = SV_Position.xy / screenSize`로 픽셀 좌표를 화면 UV로 정규화해 풀스크린 AO 버퍼를 1:1로 읽는다. 흔한 함정: 텍셀 코너 좌표(x/W)를 중심으로 착각하면 바이리니어 필터가 인접 텍셀을 절반씩 섞어 이미지가 반 텍셀 밀리거나 흐려진다. 꼬리질문 "1:1 블릿이 흐릿하다면?"의 정답은 UV 반 텍셀 오프셋 또는 포인트 샘플링 확인이다.

### Q. 텍스처 어드레싱 모드(Wrap/Clamp/Border/Mirror)의 수학과 각각의 용도는? UI와 월드 타일링에 다른 모드를 쓰는 이유는?

범위 밖 UV를 처리하는 규칙이다. Wrap은 u' = frac(u)(u - floor(u))로 주기 반복, Clamp는 u' = clamp(u, 0, 1)로 가장자리 텍셀을 늘림, Border는 범위 밖을 지정 상수색으로, Mirror는 홀수 주기마다 u' = 1 - frac(u)로 반사한다. 직관적으로 Wrap은 "무한 타일 바닥", Clamp는 "가장자리에서 멈추는 스티커"다. Winters의 RHI 매핑(`CDX11Device.cpp:203-225`)이 eRHIAddressMode{Wrap, Clamp, Border} → `D3D11_TEXTURE_ADDRESS_*`로 이를 노출하고, 샘플러 정책(`Texture.cpp:238-253`)은 UI/데칼 = Clamp, 월드 타일링 = Wrap으로 고정한다. 이유: 타일링 지형은 UV가 0~N으로 커지며 반복돼야 하고, UI 스프라이트는 필터링이 반대편 가장자리 텍셀을 끌어오면(Wrap일 때 u=0 근처에서 u=1 텍셀이 섞임) 테두리에 이물 색 줄이 생기기 때문이다. 함정: 아틀라스 안의 서브스프라이트는 어드레싱 모드로는 보호되지 않는다 — 샘플러는 아틀라스 전체 [0,1]만 알기 때문에, 서브렉트 경계 침범은 UV 계산과 패딩으로 막아야 한다.

### Q. 바이리니어 필터링의 수식을 쓰고, 트라이리니어가 추가로 해결하는 문제를 설명하라.

UV를 텍셀 공간으로 옮겨 tx = u*W - 0.5, 정수부로 4개 텍셀 c00..c11을 고르고 소수부 fx, fy로 이중 선형 보간한다: c = (1-fx)(1-fy)·c00 + fx(1-fy)·c10 + (1-fx)fy·c01 + fx·fy·c11. 기하학적으로는 샘플 지점을 둘러싼 4텍셀 사각형 안에서의 면적 가중 평균이다. 트라이리니어는 여기에 mip 축을 더한다 — 계산된 LOD d에 대해 floor(d)와 ceil(d) 두 mip 레벨에서 각각 바이리니어 샘플 후 frac(d)로 한 번 더 lerp(총 8탭). 이것이 해결하는 문제는 mip 경계에서의 급격한 선명도 점프(mip banding)로, 카메라가 이동할 때 바닥에 선명/흐림 경계선이 스치듯 지나가는 아티팩트를 없앤다. Winters의 UI/데칼 샘플러가 `D3D11_FILTER_MIN_MAG_MIP_LINEAR`(트라이리니어)를 기본으로 쓰고 `MaxLOD=FLOAT32_MAX`로 풀 mip 체인을 열어둔다(`Texture.cpp:238-253`). 꼬리질문 대비: MIP 항만 POINT로 바꾼 `MIN_MAG_LINEAR_MIP_POINT`는 mip 간 lerp를 생략해 4탭으로 싸지만 banding이 돌아온다 — 필터 이름의 세 슬롯(MIN/MAG/MIP)이 각각 축소/확대/밉 보간을 독립적으로 제어한다는 걸 아는지 자주 묻는다.

### Q. GPU는 어떤 mip 레벨을 샘플링할지 어떻게 결정하는가? 밉맵이 없으면 무슨 일이 일어나는가?

픽셀 셰이더는 2×2 쿼드 단위로 실행되므로 하드웨어는 이웃 픽셀 간 UV 차분으로 스크린 공간 도함수 ddx(uv), ddy(uv)를 공짜로 얻는다. 이를 텍셀 단위로 스케일해 lod = log2(max(|ddx(uv)|·W, |ddy(uv)|·H)) 형태로 LOD를 계산한다 — "화면 1픽셀이 텍스처에서 몇 텍셀을 건너뛰는가"의 log2다. 기하학적 의미: 픽셀 하나의 풋프린트가 텍셀 2^d개를 덮으면, 미리 2^d배 축소·평균된 mip d를 읽는 것이 그 풋프린트의 올바른 적분 근사다. 밉맵 없이 minification하면 픽셀마다 풋프린트 안의 텍셀 하나만 임의로 찍게 되어 언더샘플링 앨리어싱 — 카메라가 움직일 때 원경이 반짝이는 shimmer — 이 생긴다. Winters는 로더(`RHITextureLoader.cpp:100-142`)가 mipLevels=1로만 만들고, 첫 Bind 시점에 mip 체인을 지연 생성하는 2단 구조(`Texture.cpp:86-156`)로 이 문제를 처리한다. 함정: mip은 메모리를 겨우 +33%(1/4 등비합)만 더 쓰면서 캐시 지역성까지 좋아지므로 "메모리 아끼려 mip 생략"은 거의 항상 손해라는 트레이드오프까지 말해야 한다.

### Q. 경사면에서 트라이리니어가 흐릿해지는 이유와 이방성(anisotropic) 필터링이 그것을 고치는 원리는? MaxAnisotropy 설정의 흔한 함정은?

트라이리니어의 LOD 선택은 ddx/ddy 중 큰 쪽(max) 하나로 등방성 정사각 풋프린트를 가정한다. 바닥을 비스듬히 보면 실제 풋프린트는 시선 방향으로 길게 늘어난 타원인데, 긴 축에 맞춰 흐린 mip을 고르면 짧은 축 방향까지 과도하게 블러되고, 짧은 축에 맞추면 긴 축이 앨리어싱된다. 이방성 필터링은 짧은 축으로 mip을 고르고 긴 축을 따라 최대 N회(x8이면 8탭) 트라이리니어 샘플을 찍어 평균한다 — 타원 풋프린트를 여러 개의 원으로 근사 적분하는 것이다. Winters의 샘플러 정책(`Texture.cpp:238-253`)이 정확히 이 논리로 나뉜다: 월드 타일링(Wrap)은 `D3D11_FILTER_ANISOTROPIC` + MaxAnisotropy=8(경사면 minification shimmer 방지가 주석으로 박제), 화면과 거의 정면으로 마주 보는 UI는 LINEAR로 충분. 함정이자 실전 버그 포인트: D3D11에서 MaxAnisotropy는 Filter가 `D3D11_FILTER_ANISOTROPIC` 계열일 때만 유효하고 `MIN_MAG_MIP_LINEAR`에서는 무시된다 — Winters `CDX11Device` CreateSampler도 maxAnisotropy>1일 때만 반영하도록 방어한다. "MaxAnisotropy=16 넣었는데 안 바뀌어요"는 십중팔구 필터 열거값이 LINEAR인 경우다.

### Q. Map/Unmap으로 텍스처를 업로드할 때 row pitch는 왜 width × bytesPerPixel과 다를 수 있고, memcpy를 왜 행 단위로 해야 하는가?

RowPitch는 GPU가 정한 "한 행의 시작에서 다음 행 시작까지의 바이트 거리"로, 하드웨어 정렬 요구(타일링, 캐시 라인) 때문에 width×bpp보다 클 수 있다. 즉 각 행 끝에 패딩이 붙는다. 한 번의 memcpy(total = w*h*bpp)로 밀어 넣으면 두 번째 행부터 패딩만큼 어긋나 이미지가 대각선으로 흘러내리는(sheared) 고전적 아티팩트가 나온다. 올바른 루프는 행마다 dst = pData + y*RowPitch, src = pixels + y*(w*bpp)로 w*bpp 바이트씩 복사하는 것이다. Winters 포그오브워가 정석 구현이다 — `FogOfWarRenderer.cpp:266-276`이 R8(텍셀당 1바이트) 가시성 텍스처를 `D3D11_MAP_WRITE_DISCARD`로 Map 후 행 단위 memcpy로 채운다. 반대로 CPU 쪽 소스 데이터는 자체 피치를 가질 수 있는데, WIC 로더(`RHITextureLoader.cpp:100-142`)는 RGBA32로 강제 변환 후 rowPitchBytes = width * 4로 타이트하게 CopyPixels 받아 초기 데이터로 넘긴다. 꼬리질문 대비: WRITE_DISCARD는 이전 내용을 버리고 새 메모리를 주므로 "바뀐 행만 쓰고 나머지는 남길" 수 없다 — 부분 갱신이 필요하면 WRITE_NO_OVERWRITE나 UpdateSubresource 전략을 비교해 답한다.

### Q. 아틀라스 플립북 애니메이션에서 시간 t로부터 프레임의 UV rect를 계산하는 수식을 유도해보라. loop와 clamp 처리는 어떻게 다른가?

균등 N=cols×rows 그리드 아틀라스 기준: frame = floor(t * fps)로 정수 프레임을 얻고, 루프면 frame %= frameCount, 아니면 frame = min(frame, frameCount-1)로 마지막 프레임에 고정한다. 셀 좌표는 col = frame % cols, row = frame / cols(정수 나눗셈), UV rect는 u0 = col/cols, v0 = row/rows, u1 = (col+1)/cols, v1 = (row+1)/rows — 셀 크기가 정확히 (1/cols, 1/rows)인 균등 분할이다. 기하학적으로는 시간축을 정수 격자에 양자화한 뒤 1차원 인덱스를 2차원 격자 좌표로 펼치는 것이다. Winters `FxSystem.cpp:344-365`가 이 수식 그대로이며, frameCount를 cols*rows로 min 처리해 매니페스트 오기입이 격자 밖 UV를 만들지 못하게 막는다. UI 쪽은 격자가 아닌 임의 배치 아틀라스라 JSON 매니페스트의 픽셀 rect를 Vec4(x/w, y/h, (x+fW)/w, (y+fH)/h)로 정규화한다(`UIAtlasManifest.cpp:396-409`). 함정: loop에 %가 아니라 frac(t*fps*...)식 float 누적을 쓰면 장시간 재생 시 float 정밀도로 프레임이 튄다는 점, 그리고 `UI_Manager.cpp`에 kAtlasSize=1024 하드코딩 나눗셈 버전이 병존하는데 아틀라스 리사이즈 시 매니페스트 경로만 살아남는다는 유지보수 트레이드오프도 언급할 만하다.

### Q. lerp(uvMin, uvMax, localUV) 형태의 UV rect 리맵은 왜 쓰며, 버텍스 셰이더와 픽셀 셰이더 중 어디서 하는 게 맞는가?

쿼드의 로컬 UV(0~1)를 아틀라스 서브사각형 [uvMin, uvMax]로 보내는 아핀 변환 u' = uvMin + (uvMax - uvMin)·localUV = lerp(uvMin, uvMax, localUV)다. 여기에 스크롤은 u'' = u' + scrollOffset로 더한다. 기하학적으로 "단위 사각형을 아틀라스 안의 작은 사각형으로 평행이동+스케일"하는 것뿐이다. Winters는 이를 버텍스 셰이더 공통 규약으로 박제했다 — `FxSprite.hlsl:56-58`의 `uv = lerp(g_vUVRect.xy, g_vUVRect.zw, localUV) + g_vUVScroll`이 FxMesh.hlsl, UIPlane.hlsl에 동일하게 반복되고, 스크롤 누적치(fUvScrollU/V * fElapsed)는 CPU에서 계산해 상수버퍼로 넘긴다(`FxSystem.cpp:368`). VS에서 해도 되는 이유: 아핀(선형+상수) 변환은 래스터라이저의 원근 보정 보간과 교환 가능해서 정점에서 변환하나 픽셀에서 변환하나 결과가 같고, 정점 4개 연산이 픽셀 수만 번 연산보다 싸다. 반대로 픽셀별로 비선형인 변환(도메인 워핑, 왜곡, frac 기반 타일 리맵)은 보간과 교환되지 않으므로 PS에서 해야 한다. 꼬리질문: "PS에서 frac(uv)로 수동 타일링하면?" — UV 도함수가 랩 경계에서 불연속이 되어 자동 mip 선택이 폭주하고 경계에 한 줄 아티팩트가 생긴다. 이때가 gradient를 직접 넘기는 SampleGrad의 용도다.

### Q. 아틀라스에서 인접 스프라이트 색이 새어 들어오는 bleeding은 왜 생기고, 어떻게 막는가? HP바처럼 스프라이트를 비율로 잘라 그릴 때의 추가 규칙은?

원인은 두 가지다. (1) 바이리니어 필터가 서브렉트 경계 텍셀에서 이웃 스프라이트 텍셀을 절반 섞음 — UV rect를 텍셀 코너가 아닌 반 텍셀 안쪽(u0 + 0.5/W)으로 잡거나 스프라이트 사이에 패딩을 넣어 해결. (2) mip 레벨에서는 축소·평균 과정에서 이웃 스프라이트가 물리적으로 한 텍셀로 합쳐지므로, mip을 쓰는 아틀라스는 mip 단수만큼(레벨 d에서 2^d 텍셀) 넉넉한 패딩 또는 가장자리 복제(edge dilation)가 필요하다. Winters HP바(`ActorHUDPanel.cpp:1326-1338`)는 세 번째 규칙을 보여준다: 체력 비율 r로 채움을 그릴 때 지오메트리와 UV를 같은 비율로 동시 축소한다 — 가로 채움이면 W *= r; u1' = u0 + (u1-u0)·r. UV만 그대로 두고 쿼드만 줄이면 텍스처가 짓눌리고, 쿼드는 그대로 두고 UV만 줄이면 늘어난다. 핵심은 자르기가 항상 자기 서브렉트 내부에서만 일어나 인접 스프라이트를 침범하지 않는다는 것. 원형 초상화(`UIRenderer.cpp:555-587`)도 같은 원리로, 서브렉트 중심 (uc,vc)와 반경 (ur,vr) = ((u1-u0)/2, (v1-v0)/2)의 타원 파라메트릭 uv(a) = (uc + cos(a)·ur, vc + sin(a)·vr)로 부채꼴 정점 UV를 만들어 rect 밖으로 절대 나가지 않는다.

### Q. 알파 테스트(clip/discard)와 알파 블렌드의 차이를 정렬·early-Z 관점에서 설명하고, premultiplied alpha의 이점을 말하라.

알파 테스트는 픽셀을 이진 kill(clip(a - threshold): 인자가 음수면 픽셀 폐기)하므로 깊이 버퍼에 정상 기록되어 그리기 순서와 무관하다. 대신 셰이더에 discard가 존재하는 순간 GPU는 "이 픽셀이 죽을지 셰이더를 돌려봐야 안다"라서 early-Z 최적화를 보수적으로 꺼야 하고, 가장자리가 계단진다. 알파 블렌드 dst' = src·a + dst·(1-a)는 부드럽지만 비가환 연산이라 뒤→앞 정렬이 필요하고 보통 깊이 쓰기를 끈다. Premultiplied alpha는 RGB에 a를 미리 곱해 블렌드를 dst' = src + dst·(1-src.a)로 바꾸는 것으로, 필터링/mip 과정에서 알파 0 텍셀의 쓰레기 RGB가 이웃과 평균되며 검은/이상한 테두리(fringe)를 만드는 문제를 원천 차단하고, additive(a=0, RGB>0)와 일반 블렌드를 한 블렌드 스테이트로 통일할 수 있다. Winters FX 경로가 하이브리드다: 알파블렌드 FX에서 배경을 오염시키는 저휘도/저알파 픽셀을 `FxSprite.hlsl:156-163`의 clip(erodeMask - threshold), clip(a - alphaClip)으로 잘라내고, dissolve 셰이더(`FxSprite.hlsl:104-148`)는 마지막에 finalRGB *= alpha로 premultiplied 출력을 한다. 꼬리질문 대비: "그럼 블렌드+clip을 같이 쓰는 이유는?" — 반투명 정렬 부담을 줄이면서(거의 안 보이는 픽셀은 아예 안 그림) 오버드로우/ROP 비용도 아끼는 실용적 절충이다.

### Q. (실전 사례) 텍스처가 로드되고 드로우콜도 나가는데 화면에 아무것도 안 보였다. 원인이 "UV가 알파 0 영역을 가리킴"이었다면, 어떤 데이터 함정이었고 어떻게 진단해야 하는가?

Winters에서 실제 겪은 사례다: Riot 챔피언 FX 에셋의 render/*.png는 메시의 디퓨즈 텍스처가 아니라 이펙트 결과물의 스프라이트 캡처였고, 그 캡처는 대부분 영역이 알파 0인 큰 캔버스에 이펙트만 찍혀 있다. 메시의 UV는 진짜 머티리얼 텍스처(irelia_base_*_texture.png, *_mult.png) 기준으로 펴져 있으므로, 캡처 텍스처에 대면 UV 대부분이 알파 0 영역을 가리키고, 셰이더의 clip(a - alphaClip)이 전 픽셀을 죽여 "호출은 다 되는데 아무것도 안 보이는" 상태가 된다. 개념적 교훈: 텍스처는 이름이 아니라 "UV 좌표계와 짝이 맞는지"로 판단해야 하며, UV-텍스처는 하나의 계약(어떤 아틀라스/언랩 기준으로 만든 좌표인가)이다. 진단 순서도 정리돼 있다 — CPU 디버거로는 못 잡는 패턴이므로 (1) RenderDoc으로 해당 드로우의 입력 텍스처와 UV를 직접 시각화, (2) 데이터 계측: 텍스처의 알파 bounding box와 메시 UV 분포를 겹쳐 미스매치 확인, (3) 셰이더의 clip 라인을 임시 무력화해 "clip이 범인인지"를 이분한다. 꼬리질문 "코드 추론만으로 왜 못 잡나?"에는 "바인딩·호출 경로는 전부 정상이고 데이터의 의미만 틀렸기 때문 — 이런 버그는 파이프라인 캡처 도구가 1차 도구"라고 답한다.

### Q. Sample, SampleLevel, SampleGrad, Load의 차이는? 분기(if) 안에서 Sample을 호출하면 왜 위험하고, 스크린스페이스 버퍼는 왜 SampleLevel(uv, 0)로 읽는가?

Sample은 하드웨어가 2×2 픽셀 쿼드의 UV 차분으로 암시적 도함수를 구해 mip을 자동 선택한다. SampleLevel(uv, L)은 mip을 명시해 도함수가 필요 없고, SampleGrad는 도함수를 직접 넘기며, Load(int3(x,y,mip))는 정수 텍셀 좌표로 필터링·어드레싱 없이 원본 텍셀을 읽는다. 분기 안 Sample이 위험한 이유: 쿼드 내 4픽셀이 서로 다른 분기를 타면(non-uniform control flow) 이웃 픽셀의 UV가 정의되지 않아 도함수가 미정의(undefined)가 되고 — HLSL 컴파일러가 gradient 명령을 분기 안에서 금지하거나 자동 평탄화한다 — 결과적으로 mip 선택이 쓰레기가 된다. 분기 밖에서 UV/gradient를 먼저 계산하거나 SampleLevel/SampleGrad로 바꾸는 게 정석이다. Winters `Mesh3D_PBR.hlsl:110-111`은 풀스크린 AO 버퍼를 `SampleLevel(sampler, aoUV, 0)`으로 읽는 규약을 박제했다: 스크린스페이스 버퍼는 화면과 1:1이라 minification 자체가 없고 mip도 1장뿐이므로 미분 기반 자동 선택이 무의미하며, 명시 mip 0이 의도를 코드로 문서화한다. 꼬리질문 대비: "그럼 Load와 SampleLevel 0의 차이는?" — Load는 필터링이 전혀 없고 범위 밖이 0을 반환하며, 반해상도 버퍼를 업샘플해 읽을 땐 bilinear가 필요하므로 SampleLevel이 맞다.

### Q. D3D11에서 런타임에 GenerateMips로 mip 체인을 만들 때의 제약 조건과, 오프라인(DDS) mip 대비 트레이드오프는? Winters는 왜 "로더는 1-mip, 첫 Bind에서 체인 생성" 구조를 택했나?

GenerateMips는 SRV 기반으로 각 mip을 이전 mip에서 다운샘플 렌더링하는 기능이라 제약이 셋이다: 텍스처가 `D3D11_RESOURCE_MISC_GENERATE_MIPS` 플래그 + RENDER_TARGET/SHADER_RESOURCE 바인드로 생성돼야 하고, 포맷이 `CheckFormatSupport`의 `D3D11_FORMAT_SUPPORT_MIP_AUTOGEN` 비트를 지원해야 하며(BC 압축 포맷은 렌더타겟이 될 수 없어 불가), MipLevels=0으로 풀 체인(1 + floor(log2(max(W,H)))장)을 예약해야 한다. Winters `Texture.cpp:86-156`이 이 전부를 구현한다: WIC 로더는 규약상 mipLevels=1로만 만들고(`RHITextureLoader.cpp`, CreateTexture도 초기 데이터가 있으면 MipLevels=1 강제 — `CDX11Device.cpp:1216`), 첫 Bind에서 MISC_GENERATE_MIPS 텍스처로 재생성 → CopySubresourceRegion으로 mip0 복사 → GenerateMips 호출. DDS(이미 체인 보유)/MSAA/1×1은 사전에 걸러낸다. 트레이드오프: 런타임 생성은 파이프라인이 단순하고 PNG 원본만 관리하면 되지만, 박스 필터 수준 품질이고 로드 스파이크가 있으며 BC 압축과 양립 못 한다. 오프라인 DDS는 고품질 필터(Kaiser 등)+BC 압축+즉시 사용이 가능하나 에셋 빌드 단계가 필요하다. 함정: GenerateMips는 immediate context 함수라 스레드 안전하지 않다 — Winters도 "렌더 스레드에서만 호출" 규약을 주석으로 박제했고, 비동기 로딩 스레드에서 부르면 미정의 동작이다.

### Q. sRGB와 선형 색공간의 관계를 설명하고, 하드웨어 sRGB 포맷 대신 셰이더에서 pow(1/2.2)로 수동 감마 보정할 때의 장단점을 말하라. 라이팅은 왜 선형에서 해야 하는가?

sRGB는 인간 밝기 지각에 맞춰 어두운 영역에 비트를 몰아주는 비선형 인코딩(근사 c_encoded = c_linear^(1/2.2))이다. 빛의 물리(광자 덧셈, N·L 감쇠, 블렌딩)는 선형 공간에서만 성립하므로, 감마 공간에서 더하거나 곱하면 0.5+0.5가 물리적 1.0이 아니게 되어 중간톤이 떡지고 하이라이트 번짐이 왜곡된다. 정석 파이프라인: 입력 텍스처는 샘플링 시 선형으로 디코드(SRGB 포맷이면 하드웨어가 필터링 전에 텍셀 단위 디코드) → 선형에서 라이팅/톤맵 → 출력 시 재인코딩. Winters는 수동 경로를 택했다: `Mesh3D_PBR.hlsl:129-130`이 Reinhard 톤맵 color/(color+1)로 [0,∞)를 [0,1)로 압축한 뒤 pow(saturate(color), 1/2.2)로 직접 인코딩하고, 로드 측(`Texture.cpp:177-219`)은 IgnoreSRGB 플래그로 하드웨어 sRGB 디코드를 끄고 UNORM으로 샘플링한다. 장점은 백버퍼/RTV 포맷을 _SRGB로 바꿀 필요 없이 셰이더만으로 제어 가능하고 아트가 보던 색과의 호환이 쉬운 것. 단점(꼬리질문 핵심): 수동 pow는 필터링·mip·블렌딩이 전부 감마 공간에서 일어나는 걸 막지 못한다 — 하드웨어 sRGB는 "디코드 → 선형에서 바이리니어"지만, UNORM 샘플링은 감마값끼리 평균하므로 고대비 경계가 어둡게 뭉치고 mip이 전체적으로 침침해진다. 또 2.2 지수는 정확한 sRGB 조각 함수(선형 구간 + 2.4 지수)의 근사임도 알아두면 좋다.

### Q. 그레이스케일 마스크를 만들 때 luma = dot(rgb, (0.299, 0.587, 0.114))처럼 채널별 가중치를 다르게 주는 이유는? Rec.601과 Rec.709의 차이는?

인간 눈은 파장별 민감도가 달라 같은 물리 강도라도 녹색을 가장 밝게, 파랑을 가장 어둡게 지각한다. 단순 평균 (r+g+b)/3은 파란 픽셀을 실제 지각보다 밝게 평가해 마스크가 어긋난다. Rec.601 계수 (0.299, 0.587, 0.114)는 SD 방송 시대의 지각 가중 휘도이고, Rec.709(HD)는 원색 정의가 달라 (0.2126, 0.7152, 0.0722)로 녹색 비중이 더 크다. 엄밀히는 선형 RGB에 709 계수를 적용한 것이 luminance, 감마 인코딩된 값에 적용한 것은 luma라 부른다. Winters의 erode 경로(`FxSprite.hlsl:156-163`)가 정확히 이 용도다 — erodeMask = saturate(dot(texColor.rgb, float3(0.299, 0.587, 0.114)))로 luma를 만들어 clip(erodeMask - g_fErodeThreshold)의 픽셀 킬 기준으로 삼는다. 어두운(배경 오염) 픽셀을 지각 기준으로 잘라내는 것이므로 계수의 절대 정밀도보다 "지각 단조성"이 중요해서 601이면 충분하다. 꼬리질문 대비: FX 마스크처럼 임계값 하나로 자르는 용도에서는 601/709 차이가 결과에 거의 안 보이지만, 톤맵/노출 계산 같은 물리 기반 경로에서 감마 공간 값에 601을 쓰면 이중 오차(잘못된 공간 × 잘못된 계수)가 되므로 선형+709로 가야 한다. FxMesh 변형(`FxMesh.hlsl:171-176`)처럼 아예 별도 노이즈 텍스처의 r채널을 기준으로 삼으면 색 의존성 자체를 끊을 수 있다는 대안도 언급하면 좋다.

### Q. dissolve(소멸) 이펙트의 수학을 단계별로 설명하라 — 노이즈 임계값, smoothstep 에지 밴드, 그리고 도메인 워핑(UV 왜곡)까지.

핵심 아이디어는 "노이즈 값 vs 시간 임계값"의 경주다. 노이즈 n(uv) ∈ [0,1]에 대해 dissolved = n - age·rate로 정의하면, 시간이 흐를수록 노이즈가 낮은 픽셀부터 dissolved < 0이 되어 clip(dissolved + edgeWidth)로 죽는다 — 노이즈의 등고선이 소멸 경계가 되어 유기적으로 갉아먹히는 모양이 나온다. 에지 발광은 경계 근처 [0, edgeWidth] 구간을 edgeMask = 1 - smoothstep(0, edgeWidth, dissolved)로 리맵해 만든다: smoothstep(a,b,x) = 3t² - 2t³ (t = saturate((x-a)/(b-a)))는 양 끝 도함수가 0인 부드러운 S-커브라 밴드 경계가 계단지지 않는다. 도메인 워핑은 "노이즈로 다른 샘플의 좌표를 흔드는" 기법이다: 노이즈 r채널 [0,1]을 r*2-1로 [-1,1] 벡터로 리맵해 distortVec을 만들고 uvA += distortVec·strength로 주 노이즈의 정의역 자체를 왜곡하면 직선적 스크롤이 유체처럼 흐물거린다. Winters `FxSprite.hlsl:104-148`이 전체 조합이다 — uvA = uv + scrollA·t와 uvB = uv·1.7 + scrollB·t로 주파수/속도가 다른 두 좌표계를 만들고(서로 다른 스케일이라 패턴 반복이 눈에 안 띔), uvB에서 오프셋을 달리 한 두 샘플(+1.3, +5.7)로 x/y 왜곡 벡터를 뽑아 uvA를 흔들고, centerMask = pow(saturate(1 - length(localUV-0.5)·2), k)의 방사 감쇠로 쿼드 가장자리를 정리한 뒤 premultiplied로 출력한다. 함정: 이 모든 UV 연산은 픽셀별 비선형이라 반드시 PS에서 해야 하고, clip이 들어가므로 early-Z 이점은 포기한 경로임을 인지하고 있어야 한다.

### Q. 포그오브워를 R8 단일 채널 텍스처 + smoothstep 이중 리맵으로 구현한다면 각 단계의 수학적 역할은 무엇인가? 경계 처리는 왜 필요한가?

가시성은 셀당 스칼라 하나(0=미탐험 ~ 1=현재 시야)면 충분하므로 R8(텍셀당 8bit) 텍스처가 최소 표현이다 — RGBA 대비 대역폭 1/4이고, 매 프레임 CPU 갱신이므로 업로드 비용이 직접 이득이 된다. 셰이더에서 한 fog 값을 두 개의 임계 구간으로 리맵한다: explored = smoothstep(0.02, 0.55, fog)는 "한 번이라도 밝힌 적 있음"의 낮은 문턱, visible = smoothstep(param, 1, fog)는 "지금 시야 안"의 높은 문턱이다. 같은 스칼라에서 두 S-커브로 서로 다른 대역을 잘라내 상태 2비트를 쓰지 않고 3단계를 얻는 것이 요점이고, 최종 알파는 alpha = lerp(lerp(unexplored, explored, e), 0, v) 체인 — 미탐험색에서 탐험색으로, 다시 시야 안(완전 투명)으로의 중첩 보간이다. smoothstep의 양끝 미분 0 덕분에 시야 원 가장자리가 하드 링 없이 페이드된다. Winters 구현(`FogOfWarWorld.hlsl:34-53`, `FogOfWarRenderer.cpp:266-276`)은 CPU 업로드에서 행 단위 RowPitch memcpy 규약을 지키고, 맵 밖 UV는 미탐험색으로 처리하는 경계 가드를 둔다 — 가드가 없으면 어드레싱 모드에 따라 Clamp면 맵 가장자리 가시성이 무한히 늘어나고 Wrap이면 반대편 시야가 새어 들어오는 논리 버그가 되기 때문이다. 꼬리질문 대비: 텍셀 해상도가 게임플레이 그리드보다 낮으므로 바이리니어 샘플링 자체가 공짜 블러 역할을 한다는 점, 더 부드럽게 하려면 CPU측 시간 페이드(현재값→목표값 지수 수렴)를 얹는다는 확장까지 말하면 좋다.

---

## 애니메이션 / 스키닝 수학

### Q. 스켈레탈 애니메이션에서 본(bone)의 로컬 공간과 모델 공간은 무엇이고, 글로벌(모델 공간) 행렬은 어떻게 누적하는가?

**정의.** 각 본의 로컬 행렬 L_i 는 부모 본 기준의 트랜스폼이고, 모델 공간 글로벌 행렬은 계층을 따라 곱해 얻는다: Global_i = L_i × Global_parent(i) (row-vector 규약 기준, child가 왼쪽). 루트까지 재귀적으로 펼치면 Global_i = L_i × L_parent × ... × L_root.

**직관.** "어깨가 돌면 팔꿈치·손목이 따라 도는" 계층적 종속을 행렬 곱 하나로 표현한 것이다. 로컬 값만 애니메이션 키로 저장하면 부모의 움직임은 자동으로 상속된다.

**Winters 적용.** `Engine/Private/Resource/Skeleton.cpp:63` 에서 "부모 인덱스는 항상 자기 인덱스보다 앞"이라는 위상 정렬 가정 하에 재귀 없이 단일 for 루프로 global = matLocal * parentGlobal 을 누적한다. 스크래치 XMMATRIX 벡터를 재사용하는 WithScratch 버전으로 per-frame 힙 할당도 피한다.

**함정/꼬리질문.** "위상 정렬이 깨지면?" — 부모가 뒤에 오면 아직 계산 안 된 글로벌을 읽어 쓰레기 포즈가 나온다. 임포트 단계(Assimp 노드 트리 DFS 순회)에서 부모-우선 순서를 보장해 저장해야 하고, 런타임에 assert 로 parentIdx < i 를 검증하는 게 안전하다.

### Q. 본 오프셋 행렬(inverse bind pose)은 왜 필요한가? 최종 스키닝 행렬 공식을 유도해 보라.

**정의.** 정점은 바인드 포즈(스킨을 입힌 순간의 포즈)의 모델 공간에 저장돼 있다. 본 j 가 정점을 움직이려면 먼저 정점을 그 본의 로컬 공간으로 되돌려야 하는데, 그 역변환이 오프셋 행렬 Offset_j = inverse(BindGlobal_j) 이다. 최종 스키닝 행렬은 Final_j = Offset_j × Global_j: "바인드 포즈 모델 공간 → 본 로컬 공간 → 현재 포즈 모델 공간".

**직관.** 본이 바인드 포즈 그대로면 Global_j = BindGlobal_j 이므로 Final_j = identity — 즉 정점이 제자리에 있다. 스키닝 행렬은 언제나 "바인드 포즈 대비 얼마나 움직였는가"의 델타다.

**Winters 적용.** `Skeleton.cpp:63` 이 Assimp 표준대로 Final = matOffset × Global × m_matGlobalInverseRoot 를 저장한다. 마지막 GlobalInverseRoot 는 임포트된 씬 루트 노드에 스케일/회전이 박혀 있는 경우(FBX 단위 변환 등) 이를 상쇄해 모델을 원점 기준 정규 공간으로 되돌리는 항이다. 오프셋 행렬 자체는 `WMeshWriter.cpp:26` 에서 Assimp 의 mOffsetMatrix 를 전치해 .wmesh BoneEntry 에 구워 둔다.

**함정/꼬리질문.** Offset 을 곱하는 걸 빼먹으면 모든 본 변환이 "원점 기준 회전"으로 적용돼 메시가 폭발하듯 흩어진다. 반대로 Global 대신 Local 을 쓰면 부모 움직임이 상속되지 않는다. "Offset 을 런타임에 매번 역행렬로 구하면 안 되나?" — 가능하지만 바인드 포즈는 불변이므로 임포트 시 1회 계산해 굽는 것이 정답.

### Q. 리니어 블렌드 스키닝(LBS)의 수식을 쓰고, 가중치 합이 1이어야 하는 이유를 설명하라.

**정의.** 정점 v 에 대해 v' = Σ_k w_k × (v × Final_bk), k = 0..3, Σ w_k = 1. 실제 구현은 행렬을 먼저 섞는다: skinMatrix = Σ w_k × Final_bk, v' = v × skinMatrix — 정점당 행렬-벡터 곱을 1회로 줄이고 위치/법선/탄젠트에 같은 행렬을 재사용할 수 있다.

**직관.** 정점이 여러 본을 "지분율"만큼 따라간다. 가중치 합이 1이 아니면 아핀 결합이 깨진다 — 합이 0.9면 정점이 원점 방향으로 10% 수축하고, 1.1이면 바깥으로 팽창한다. 행렬의 평행이동 성분이 w 배로 스케일되기 때문이다.

**Winters 적용.** `Shaders/Skinned3D.hlsl:157` 의 정점 셰이더가 정확히 이 형태다: g_BoneMatrices[idx_k].m × weight_k 4개를 합산해 skinMatrix 를 만들고, 위치는 float4(pos, 1), 법선은 float4(nrm, 0)으로 변환한다. 정점 포맷은 uint4 BLENDINDICES + float4 BLENDWEIGHT 를 포함한 76바이트 VertexSkinned(`WMeshFormat.h:73`, static_assert 로 legacy VTXANIM 과 바이트 일치 보장)다.

**함정/꼬리질문.** "w=(1,0,0,0) 단일 본 정점도 4개 행렬을 다 읽는데 낭비 아닌가?" — 분기 비용이 곱셈 비용보다 비싸서 GPU 에서는 그냥 4개 다 계산하는 게 보통 이득. "영향 본이 하나도 없는 정점은?" — Winters 는 임포트 시 bone0/weight1.0 폴백(`WMeshWriter.cpp:321`)으로 채워, 셰이더가 zero matrix 를 합산해 정점이 원점으로 붕괴하는 사고를 막는다.

### Q. LBS 의 candy-wrapper 아티팩트는 왜 생기고, Dual Quaternion Skinning 은 이를 어떻게 해결하는가? 트레이드오프는?

**정의.** LBS 는 회전 행렬들을 "행렬 성분 단위로" 선형 보간하는데, 회전 행렬의 선형 결합은 더 이상 회전(강체 변환)이 아니다. 두 본이 180°에 가깝게 비틀리면 0.5×R1 + 0.5×R2 가 특이(rank 손실) 행렬에 가까워져 손목이 사탕 포장지처럼 조여드는 부피 손실이 생긴다. 90° 굽힘에서도 관절 안쪽이 파고드는 부피 손실이 나타난다.

**직관.** 회전의 올바른 평균은 "호를 따라가는" 보간(slerp)인데, LBS 는 "현(chord)을 가로지르는" 보간을 하므로 중간 지점이 회전 공간 밖으로 새어 나가는 것이다.

**해결.** DQS 는 강체 변환을 듀얼 쿼터니언 q̂ = q_r + ε q_d 로 표현하고 Σ w_k q̂_k 를 정규화해 쓴다. 정규화된 듀얼 쿼터니언 블렌드는 항상 강체 변환이므로 부피가 보존된다. 대신 정점당 비용 증가, 스케일/시어 본 표현 불가(별도 처리 필요), 그리고 관절이 부풀어 보이는 bulging 아티팩트가 새로 생긴다.

**Winters 적용.** Winters 는 LoL 스타일 톱다운 뷰라 손목 클로즈업이 없어 표준 4-본 LBS(`Skinned3D.hlsl:157`)로 충분하다는 판단 — "아티팩트가 보이는 거리/부위인가"가 선택 기준임을 답변에 포함하면 좋다.

**꼬리질문 대비.** "그럼 AAA 는 뭘 쓰나?" — 문제 부위에만 보조 본(twist bone)을 추가해 LBS 를 유지하는 방식이 가장 흔하다. 리깅에서 해결하면 셰이더 비용이 늘지 않는다.

### Q. 키프레임 사이 보간은 위치/스케일/회전 각각 어떻게 하는가? 정규화 파라미터 f 계산에서 방어해야 할 엣지 케이스는?

**정의.** 세그먼트 [k_i, k_i+1] 안에서 f = (t − k_i.time) / (k_i+1.time − k_i.time) 을 구한 뒤, 위치·스케일은 lerp(a, b, f) = a + f(b − a), 회전은 quaternion slerp 로 보간한다. 회전에 성분별 lerp 를 쓰면 중간 쿼터니언의 크기가 1보다 작아져(정규화 전) 각속도가 일정하지 않게 된다.

**Winters 적용.** `Animation.cpp:105` 가 정확히 이 구조다: f 계산 시 span <= 0 방어(중복 타임스탬프 키)와 0~1 클램프를 넣고, XMVectorLerp / XMQuaternionSlerp 를 쓴다. 키가 1개면 그 값을 그대로 반환하고, 키가 없으면 zero 평행이동 / identity 회전 / 1 스케일 기본값을 준다.

**함정/꼬리질문.** span=0 나눗셈은 실제 DCC 익스포트에서 흔히 터지는 NaN 소스다. 또 t 가 첫 키보다 앞이거나 마지막 키보다 뒤일 때의 클램프 정책(외삽 금지)을 명시해야 한다 — Winters 는 이진 탐색 단계에서 각각 세그먼트 0 과 size−2 로 클램프한다(`Animation.cpp:9`). "보간 후 SRT 를 어떤 순서로 합성?" — row-vector 규약에서 S × R × T (`Animation.cpp:99`), 아래 규약 질문 참조.

### Q. 매 프레임 키프레임 세그먼트를 찾는 전략으로 이진 탐색, 마지막 인덱스 캐싱, 균일 프레임레이트 베이크가 있다. 각각의 트레이드오프는?

**이진 탐색.** keys[i].time <= t < keys[i+1].time 인 i 를 lower-bound 로 O(log n)에 찾는다. 상태가 없어(stateless) 여러 스레드/여러 인스턴스가 같은 애니 데이터를 공유해도 안전하고, 시간 점프(스킬 캔슬, 서버 스냅 보정)에도 성능이 일정하다. Winters 의 선택이 이것이다 — `Animation.cpp:9` 의 FindVectorKeySegment / FindQuatKeySegment.

**마지막 인덱스 캐싱.** 재생은 대부분 전진하므로 직전 인덱스에서 선형 탐색하면 평균 O(1). 단 인스턴스별 캐시 상태가 필요해 애니 데이터와 재생 상태의 분리가 깨지고, 역재생/시간 점프에서 최악 O(n).

**균일 베이크.** 30fps 등 고정 간격으로 키를 다시 샘플링해 저장하면 인덱스가 i = floor(t / dt) 로 O(1) 산술 계산이 된다. 탐색이 사라지는 대신 메모리가 커지고(희소 키의 이점 상실), 베이크 레이트보다 빠른 원본 디테일이 잘린다.

**꼬리질문 대비.** "Winters 에서 캐싱으로 바꾸면?" — Animation(공유 데이터)과 Animator(인스턴스 상태)가 분리돼 있으므로 캐시는 Animator 쪽에 둬야 한다는 소유권 논점까지 말하면 좋다. 또 VectorKey/QuatKey 용 탐색이 동일 로직 중복 구현인 점은 템플릿화 여지가 있는 정직한 개선점.

### Q. Slerp, Nlerp, Lerp 의 차이는? dot < 0 일 때 부호를 반전하는 이유는?

**정의.** slerp(q1, q2, f) = q1 × sin((1−f)θ) / sinθ + q2 × sin(fθ) / sinθ (cosθ = q1·q2) — 4차원 단위 초구면 위의 대원(great circle)을 등각속도로 따라간다. nlerp 는 normalize(lerp(q1, q2, f)) — 현을 지나 구면으로 투영하므로 경로는 같은 호지만 속도가 중간에서 살짝 빨라진다. lerp 만 쓰면 결과가 단위 쿼터니언이 아니어서 회전으로 쓸 수 없다.

**비용/선택.** slerp 는 acos/sin 이 들어가 비싸고, nlerp 는 곱셈+정규화뿐이라 싸다. 두 키 사이 각도가 작으면(고밀도 키프레임) 속도 오차가 무시할 수준이라 nlerp 로 충분하고, 각도가 큰 희소 키나 블렌딩 품질이 중요한 곳은 slerp.

**부호 반전.** q 와 −q 는 같은 회전(이중 피복)이다. q1·q2 < 0 이면 두 쿼터니언이 초구면에서 90° 이상 떨어진 것이므로, 반전 없이 보간하면 "270° 를 도는" 장경로를 택해 캐릭터가 반대 방향으로 홱 도는 아티팩트가 난다. 한쪽 부호를 뒤집어 최단 경로를 강제한다.

**Winters 적용.** `Animation.cpp:105` 는 XMQuaternionSlerp 를 쓰는데, DirectXMath 의 Slerp 는 내부에서 최단 경로 처리를 해 준다. 다만 직접 구현 시 θ→0 에서 sinθ 나눗셈이 발산하므로 작은 각도에서 nlerp 로 폴백하는 방어가 표준임을 언급하면 좋다.

### Q. row-vector 와 column-vector 행렬 규약의 차이가 본 계층 누적과 SRT 합성 순서에 어떻게 나타나는가? Assimp 임포트 시 왜 전치가 필요한가?

**정의.** DirectXMath 는 row-vector 규약: v' = v × M 이라 왼쪽에 쓴 행렬이 먼저 적용된다. 그래서 "스케일 → 회전 → 이동" 로컬 포즈는 S × R × T 로 곱하고, 본 계층 누적은 Global_child = L_child × Global_parent 로 child 가 왼쪽이다. column-vector(OpenGL/Assimp 수학 표기) 규약에서는 v' = M × v 라 모든 순서가 뒤집혀 T × R × S, Global_parent × L_child 가 된다.

**직관.** 규약이 다른 게 아니라 같은 변환의 전치 표현이다: (A × B)^T = B^T × A^T. 그래서 두 세계를 오갈 때는 곱 순서를 뒤집거나 행렬을 전치하거나 둘 중 하나만 하면 된다 — 둘 다 하면 원위치.

**Winters 적용.** 세 군데에서 일관되게 지켜진다: `Animation.cpp:99` 의 S×R×T 로컬 합성, `Skeleton.cpp:63` 의 matLocal × parentGlobal 누적, 그리고 `WMeshWriter.cpp:26` 의 ConvertAndTranspose 가 column-major aiMatrix4x4(mOffsetMatrix 포함)를 row-major XMMATRIX 로 전치해 저장한다. HLSL 쪽은 본 팔레트를 row_major float4x4 로 선언해 CPU 메모리 레이아웃을 그대로 받는다.

**함정/꼬리질문.** 전치를 빼먹으면 "회전이 반대 방향 + 평행이동이 이상한 축으로" 나타나는 특유의 깨진 포즈가 된다. HLSL 기본은 column_major packing 이므로 CPU 에서 row-major 행렬을 그대로 memcpy 하면 mul(v, M) 과 mul(M, v) 중 무엇을 쓰는지, row_major 지시자를 붙였는지까지 세트로 맞아야 한다 — "어디서 한 번 전치하고, 그 뒤로는 절대 안 건드린다"는 단일 변환 지점 원칙을 답하면 좋다.

### Q. 애니메이션 채널이 없는 본을 identity 로 초기화하면 어떤 버그가 생기는가? 무엇으로 초기화해야 하나?

**정의.** 애니메이션 클립은 움직이는 본에만 채널(키 트랙)을 갖는다. 채널 없는 본의 로컬 행렬을 identity 로 두면 그 본이 "부모 위치에 겹쳐지고 회전 0"이 되는데, 이는 스켈레톤의 기본 골격 형태(본 길이, 기본 굽힘)를 담은 rest pose 로컬 트랜스폼과 전혀 다르다.

**증상.** 특정 클립에서만 손가락이 손목 안으로 말려들거나, 무기 본·보조 본이 원점/부모 위치로 붕괴하는 부분적 T-포즈 붕괴가 난다. 클립마다 채널 유무가 다르니 "어떤 애니에서만 깨지는" 재현 조건이 되어 디버깅이 고약하다.

**Winters 적용.** `Animation.cpp:88` 의 Evaluate 는 시작 시 모든 본을 pSkeleton->GetBone(i).matRestLocal 로 초기화하고, 주석으로 "채널 없는 본은 Rest Pose(노드 기본 트랜스폼)로 초기화 — Identity가 아님!" 을 명시해 규약으로 박아 뒀다.

**꼬리질문 대비.** "rest pose 와 bind pose 는 같은가?" — 보통 같게 익스포트하지만 개념적으로 별개다: bind pose 는 스킨 바인딩 시점의 포즈(오프셋 행렬의 기준), rest pose 는 노드 트리의 기본 로컬 트랜스폼. 둘이 어긋난 에셋은 채널 없는 본에서 미세한 포즈 오차로 나타난다.

### Q. 법선 변환에 왜 inverse-transpose 행렬이 필요한가? 스킨 행렬에 비균등 스케일 본이 섞이면 어떻게 되는가?

**정의.** 법선은 표면의 접평면에 수직인 벡터라서 위치와 같은 행렬로 변환하면 안 된다. 접선 t 가 M 으로 변환될 때 수직성 n·t = 0 을 유지하려면 법선은 (M^-1)^T 로 변환해야 한다. M 이 강체 변환(회전+이동)이면 M 의 회전부는 직교 행렬이라 (M^-1)^T = M 이 되어 그냥 M 을 써도 된다.

**직관.** 비균등 스케일로 구를 y 축으로 납작하게 누르면, 표면 접선은 같이 눌리지만 법선은 오히려 y 쪽으로 세워져야 한다 — 위치와 법선이 반대 방향으로 변형되는 것이 inverse 의 기하학적 의미다.

**Winters 적용.** `Skinned3D.hlsl:157` 은 법선에 skinMatrix 를 그대로 적용한다 — "본은 강체 변환"이라는 가정을 명시적으로 깔고, 비용이 큰 정점당 4개 행렬 역전치를 회피한 것. 대신 월드 변환 단계(모델 전체 스케일이 들어올 수 있는 곳)에서는 g_matWorldInvTranspose 로 정석 처리한다. 즉 "스케일이 들어올 수 있는 경계에서만 invTranspose" 라는 비용-정확도 절충이다.

**함정/꼬리질문.** 리깅에 squash-and-stretch 스케일 본이 들어오는 순간 이 가정이 깨져 라이팅이 미묘하게 어두워지거나 번들거린다(법선 길이/방향 왜곡, N·L 오염). 대응은 (1) 스케일 본 금지 규약, (2) 셰이더에서 변환 후 normalize(방향 왜곡은 남음), (3) 팔레트에 역전치 행렬을 별도 업로드. 균등 스케일만 있으면 normalize 로 완전히 복구된다는 것도 알아두면 좋다.

### Q. 정점당 본 영향을 4개로 제한하는 이유는? 잘라낸 뒤 재정규화를 하지 않으면 어떤 오차가 생기는가?

**정의.** GPU 정점 포맷을 고정폭으로 유지하기 위해 영향 본을 top-4 로 자르고 BLENDINDICES(uint4)+BLENDWEIGHT(float4)로 저장하는 게 표준이다. 원래 6개 본이 (0.4, 0.2, 0.15, 0.1, 0.1, 0.05)로 나눠 갖던 정점을 4개로 자르면 남은 합이 0.85 가 되는데, 재정규화(각 w 를 0.85 로 나눔) 없이 그대로 쓰면 Σw < 1 이 되어 정점이 원점 방향으로 수축하고, 애니 중 미세한 스킨 떨림/꺼짐으로 나타난다.

**올바른 절차.** 가중치 내림차순 정렬 → 상위 4개 선택 → w_i / Σw 로 재정규화. "선착순 4개"를 쓰면 큰 가중치가 잘려나갈 수 있어 정렬이 먼저다.

**Winters 적용.** `WMeshWriter.cpp:255` 는 현재 정점당 선착순 4슬롯만 기록하고 정렬·재정규화를 하지 않는다 — LoL 리그는 대부분 정점당 영향이 4개 이하라 실질 피해가 없지만, 고밀도 리그(Elden 방향 에셋)를 받으면 터질 수 있는 알려진 부채라고 솔직하게 짚는 것이 좋은 답변이다. 영향 0개 정점의 bone0/weight1.0 폴백(line 321)은 반대편 엣지 케이스 방어.

**꼬리질문 대비.** "8본 지원하려면?" — 정점 포맷을 늘리는 대신 두 번째 인덱스/가중치 스트림을 추가하거나, 컴퓨트 pre-skinning 으로 옮겨 정점 포맷 제약 자체를 없애는 선택지를 비교할 수 있어야 한다.

### Q. 본 팔레트를 GPU 에 어떻게 전달하는가? cbuffer 의 한계와 StructuredBuffer 로 옮길 때의 트레이드오프를 설명하라.

**정의.** 스키닝 VS 는 본별 Final 행렬 배열(매트릭스 팔레트)을 읽는다. 전달 수단은 (1) cbuffer — 슬롯당 최대 64KB, float4x4 가 64B 이므로 최대 1024개지만 실무에선 다른 상수와 함께 쓰여 보통 256~512본 한계, (2) StructuredBuffer SRV — 크기 제한이 사실상 없고 DYNAMIC + MAP_WRITE_DISCARD 로 매 프레임 업로드, (3) 텍스처 팔레트(VTF) — 구형 API 호환용.

**트레이드오프.** cbuffer 는 GPU 상수 캐시를 타서 균일 접근(모든 정점이 비슷한 본을 읽음)에 유리하고, StructuredBuffer 는 일반 메모리 로드라 약간 느릴 수 있지만 본 수 상한을 없앤다. 팔레트처럼 발산 접근(정점마다 다른 인덱스)이 큰 데이터는 실측 차이가 작다.

**Winters 적용.** `ModelRenderer.cpp:28` 에서 1024+ 본 대형 리그(Elden Ring 보스급) 지원을 위해 CBBoneMatrices bones[512] cbuffer 를 D3D11_RESOURCE_MISC_BUFFER_STRUCTURED 버퍼의 SRV(register t8, kMaxGPUBones=1024)로 이전했다. HLSL 쪽은 row_major float4x4 를 감싼 래퍼 구조체로 기존 CBBones 와 메모리 레이아웃을 보존해 셰이더 수정을 최소화했고, Animator 의 GetFinalBoneMatrices() 를 매 프레임 memcpy 로 업로드한다.

**꼬리질문 대비.** "수백 마리 스키닝 인스턴싱은?" — 인스턴스별 팔레트를 하나의 큰 StructuredBuffer 에 이어 붙이고 인스턴스 ID × 본 수로 오프셋하는 방식(팔레트 어레이)이 표준. cbuffer 로는 구조적으로 불가능하다는 점이 SRV 이전의 또 다른 이유가 된다.

### Q. 스키닝을 CPU 에서 할 때와 GPU(VS) 에서 할 때의 장단점은? 멀티패스 렌더링에서 VS 스키닝의 숨은 비용은?

**CPU 스키닝.** 스킨 결과 정점을 CPU 가 직접 계산해 버퍼에 올린다. 스킨 후 위치가 CPU 에 있으므로 정밀 피킹, CPU 파티클 부착, 스킨 후 물리에 유리하지만, 정점 수 × 캐릭터 수만큼 CPU 를 태우고 매 프레임 정점 버퍼 업로드 대역폭이 든다.

**VS 스키닝.** 팔레트(본 수 × 64B)만 업로드하면 병렬성 좋은 GPU 가 정점을 처리한다 — 업로드가 정점 수가 아니라 본 수에 비례하는 것이 핵심 이점. 대신 스킨 결과가 GPU 에만 존재해 CPU 는 정확한 스킨 후 위치를 모른다.

**멀티패스 비용.** VS 스키닝은 패스마다 재계산된다. Winters 가 실제 사례다: 메인 패스 `Skinned3D.hlsl:157` 과 SSAO 노멀 패스 `SkinnedNormalOnly.hlsl:39` 가 동일한 4-본 LBS 수식을 복제해, 같은 캐릭터가 프레임당 두 번 스키닝된다. 그림자 패스까지 추가되면 세 번이다.

**해결 방향.** 컴퓨트 셰이더 pre-skinning: 프레임 초두에 CS 로 스킨 결과를 UAV 버퍼에 한 번 구워 두고, 모든 패스가 static 메시처럼 그 버퍼를 읽는다. 패스 수 × 스키닝 비용이 1회로 줄고 정점 포맷 4본 제한도 사라지지만, 캐릭터당 스킨 버퍼 메모리와 CS-VS 동기화가 추가된다. "패스가 2개를 넘는 순간부터 pre-skinning 이 이기기 시작한다"가 실무 감각.

**함정.** 셰이더 간 수식 복제는 한쪽만 고치는 드리프트 버그의 온상 — Winters 도 두 HLSL 이 복제 상태라 include 공통화가 정직한 개선점이다.

### Q. 투사체나 이펙트를 캐릭터의 손 본에 붙이려면 어떤 행렬이 필요한가? 스키닝용 Final 행렬을 그대로 쓰면 왜 안 되는가?

**정의.** 본 어태치에 필요한 것은 본의 현재 포즈 글로벌 행렬 Global_j (모델 공간)이고, 소켓 월드 위치는 world = Global_j × EntityWorld 로 합성한 뒤 로컬 오프셋을 TransformPoint 하면 된다. 스키닝용 Final_j = Offset_j × Global_j 는 "바인드 포즈 정점을 옮기는 델타"라서, 여기에 붙이면 오프셋(inverse bind)이 이중 적용돼 소켓이 엉뚱한 곳(대략 바인드 포즈 역변환만큼 어긋난 위치)에 잡힌다.

**직관.** Final 은 "정점용 변위", Global 은 "본 좌표계 그 자체". 어태치는 본 좌표계에 물건을 놓는 일이므로 후자가 필요하다.

**Winters 적용.** `ModelRenderer.cpp:825` 의 TryResolveBoneWorldPosition 이 Animator 의 TryGetBoneGlobalTransform(오프셋 곱하기 전 글로벌)을 받아 matBoneLocal × matEntityWorld 를 합성하고 vLocalOffset 을 투사해 FX/투사체 소켓 위치를 만든다. 이 때문에 Animator 가 Final(스키닝용)과 Global(어태치용) 두 배열을 분리 보관한다 — 메모리를 더 쓰는 대신 용도별 행렬을 명확히 나눈 설계.

**꼬리질문 대비.** "Global 만 저장하고 Final 은 셰이더에서 Offset 을 곱하면?" — 가능하지만 정점마다 행렬 곱이 1회 늘어난다(팔레트는 본당 1회면 끝날 일). "어태치가 한 프레임 늦게 따라오는 버그는?" — 애니 갱신과 어태치 조회의 업데이트 순서 문제로, 행렬 수식이 아니라 시스템 실행 순서를 먼저 의심해야 한다.

### Q. 스킬의 castFrame 같은 애니메이션 프레임 이벤트를 정확히 1회만 발화시키려면 어떻게 판정해야 하는가? 루프 애니에서의 함정은?

**정의.** 프레임 시간이 이산 dt 로 진행하므로 "현재 프레임 == 이벤트 프레임" 같은 등호 비교는 프레임을 건너뛰면 놓친다. 올바른 판정은 반개구간 통과 검사: prevFrame < eventFrame && eventFrame <= curFrame — 이벤트 프레임이 (prev, cur] 구간에 들어온 그 틱에 정확히 1회 발화한다. 반개구간이어야 경계 프레임에서 이번 틱과 다음 틱이 중복 발화하지 않는다.

**Winters 적용.** `Animator.h:36` 의 HasFramePassed 가 이 판정이며, DLL export 를 피하려고 헤더 인라인으로 정의해 Client TU 가 직접 호출하는 규약(Phase T-2 주석)이다. 스킬 castFrame/hitFrame 발화가 여기 걸려 있어 전투 타이밍의 정합성이 이 한 줄에 달려 있다.

**루프 함정.** 루프 랩 틱에서는 curFrame 이 prevFrame 보다 작아져(예: prev=58, cur=2) prev < e && e <= cur 가 어떤 e 에 대해서도 거짓이 된다 — 랩 직전 구간(58~60)과 직후 구간(0~2)의 이벤트가 통째로 누락된다. Winters 코드도 이 구조적 한계를 갖고 있으며, 수정하려면 랩 감지 시 (prev, duration] 과 [0, cur] 두 구간으로 나눠 검사해야 한다. 실전에서 castFrame 은 non-loop 캐스팅 애니에 있어 안 터지고 있을 뿐이라는 "왜 아직 문제가 안 됐는가"까지 말하면 좋다.

**연관 수식.** 시간 진행 자체는 `Animator.cpp:42` 에서 t += dt × ticksPerSecond × playSpeed 로 틱 누적, loop 는 fmod 랩(역재생 음수 시간은 +duration 보정), non-loop 는 클램프 후 정지. ticksPerSecond <= 0 인 에셋은 25.0 기본값(`Animation.cpp:68`) — Assimp/FBX 임포트에서 실제로 0 이 들어오는 고전 함정이다. SetPlaySpeed 의 ±0.01 데드존은 0 배속으로 시간이 얼어 이벤트가 영원히 안 오는 상태를 금지한다.

### Q. 두 애니메이션 클립을 부드럽게 전환(크로스페이드)하려면 어느 공간에서 무엇을 보간해야 하는가? 최종 행렬을 보간하면 왜 안 되는가?

**정의.** 크로스페이드는 두 클립을 각각 평가해 얻은 본별 로컬 SRT 포즈를 성분별로 섞는다: pos = lerp(posA, posB, w), scale = lerp, rot = slerp(rotA, rotB, w) (w 는 페이드 진행도 0→1, 보통 0.1~0.3초). 섞은 로컬 포즈로 글로벌 누적 → 스키닝 행렬 생성은 평소와 동일하다.

**최종 행렬 보간이 안 되는 이유.** Final/Global 행렬을 성분별로 lerp 하면 회전부가 회전이 아니게 되어(LBS candy-wrapper 와 같은 수학적 원인) 팔다리가 수축·전단되며 지나간다. 회전은 반드시 쿼터니언 공간에서 slerp 해야 하고, 그러려면 행렬로 합성되기 전인 로컬 SRT 단계에서 개입해야 한다.

**Winters 현황.** 현재 Animator 는 클립 하드 스위칭이다 — Evaluate 가 단일 클립의 SRT 를 뽑아 `Animation.cpp:99` 에서 즉시 S×R×T 행렬로 합성한다. 크로스페이드를 넣으려면 "행렬 합성 전 SRT 배열" 단계를 공개된 중간 표현으로 분리해 두 클립분을 섞는 구조 변경이 필요하고, 이것이 블렌드 트리(2D 방향 이동 블렌딩 등)로 가는 첫 계단이라는 로드맵까지 답하면 좋다.

**꼬리질문 대비.** "하드 스위칭의 실제 비용은?" — 시각적 팝핑. LoL 류 톱다운+빠른 스킬 캔슬 게임은 팝이 짧아 우선순위가 낮았다는 제품 판단을 함께 말해야 한다. "블렌드 중 castFrame 이벤트는 어느 클립 기준?" — 이벤트 소유 클립의 자체 타임라인으로 판정해야 하며, 블렌드 가중치와 이벤트 발화를 섞으면 안 된다는 게 정석.

### Q. 애니메이션 임포트 파이프라인(Assimp → 자체 포맷)에서 겪은 대표적인 함정과, 자체 바이너리 포맷으로 굽는 이유를 설명하라.

**굽는 이유.** 런타임 Assimp 파싱은 느리고(FBX 파싱 수백 ms), DCC 포맷의 규약 차이(행렬 major, 단위, 축)가 런타임까지 새어 들어온다. 오프라인에서 규약을 전부 자체 규약으로 정규화해 바이너리로 구우면 런타임은 memcpy 수준 로드 + 규약 단일화가 된다.

**Winters 의 함정 목록.** (1) 행렬 major — Assimp 는 column-major 라 `WMeshWriter.cpp:26` 의 ConvertAndTranspose 가 mOffsetMatrix 를 전치해 .wmesh BoneEntry 에 저장, 이후 런타임은 전치를 전혀 모른다. (2) ticksPerSecond=0 에셋 — `Animation.cpp:68` 이 25.0 기본값으로 방어(FBX 관례값). (3) 채널 없는 본 — rest pose 초기화 규약(`Animation.cpp:88`)과 세트. (4) 정점 포맷 정합 — .wmesh VertexSkinned 와 legacy VTXANIM 이 static_assert 로 76바이트 일치를 컴파일 타임 강제(`WMeshFormat.h:73`), 포맷 드리프트를 기계로 막는다. (5) 본 식별 — .wanim 은 본 이름 문자열 대신 이름 해시 채널로 스켈레톤과 리매핑해 로드 시 문자열 비교를 없앤다(WAnimFormat.h).

**꼬리질문 대비.** ".wanim 이 raw float 키 저장인데 압축은?" — 키 quantization(회전을 smallest-three 로 48비트 압축 등), 곡선 피팅, 균일 베이크 후 델타 압축이 다음 단계이며, LoL 급 챔피언 수십 종에서는 아직 메모리가 병목이 아니라 미적용이라는 우선순위 판단을 함께 답한다.

### Q. 서버 권위 게임에서 클라이언트 애니메이션 시스템이 캐릭터의 트랜스폼(yaw)을 건드리면 어떤 문제가 생기는가? 애니메이션과 이동 권위는 어떻게 분리해야 하나?

**문제 구조.** 애니메이션/이동 시스템이 클라에서 로컬로 트랜스폼을 쓰면, 서버 스냅샷이 적용한 권위 값과 로컬 시스템이 같은 프레임에 같은 필드를 놓고 경합한다. 마지막에 쓴 쪽이 이기므로 실행 순서에 따라 캐릭터가 프레임마다 다른 방향을 보는 지터가 난다.

**Winters 사례.** 실제로 클라 CNavigationSystem 이 SnapshotApply 와 SyncFromECS 사이에서 스냅샷이 적용한 챔피언 yaw 를 덮어쓰는 사고가 있었고, 규약은 "서버 권위 게임플레이에서는 복제된 챔피언에 대해 클라 NavAgent/Velocity 이동 시스템을 돌리지 않는다 — 움직임이 계단식이 되면 로컬 내비를 되살리는 게 아니라 스냅샷 보간/예측을 고친다"로 박혔다. 같은 맥락에서 본체 yaw 는 연속 상태로 유지하고(ResolveChampionVisualYawNear 로 현재 yaw 근방 해 선택) 정규화는 wire/로그 비교용으로만 쓴다 — 매 틱 정규화하면 ±PI 경계를 재교차하며 몸이 홱홱 도는 버그가 있었다.

**루트 모션 연결.** 루트 모션(FinalBoneMatrices[0] 에서 이동/회전 추출)을 쓰는 엔진이라면 이 분리가 더 첨예해진다: 서버 권위 이동에서는 루트 모션이 위치의 진실이 될 수 없으므로, 루트 본 이동을 제자리 고정(in-place)으로 굽고 이동은 전적으로 서버 시뮬레이션이 결정하거나, 루트 모션을 서버 시뮬에도 동일하게 태워야 한다. Winters 는 전자 — 애니는 표현, 위치·yaw 는 서버 스냅샷이 진실이고, 클라 예측 yaw 는 서버 yaw 가 따라잡거나 액션 락이 걸릴 때까지만 보호된다.

**꼬리질문 대비.** "그럼 애니메이션 선택(어떤 클립을 틀지)은 누가 결정?" — 상태(이동 속도, 스킬 캐스팅)는 서버가 복제하고 클립 매핑·재생은 클라가 한다. 표현과 권위의 경계를 "트랜스폼 필드 쓰기 권한"으로 정의할 수 있어야 한다.

---

## 내비게이션 / 패스파인딩 수학

### Q. A* 알고리즘의 f = g + h에서 각 항의 의미는 무엇이고, 다익스트라와는 어떤 관계인가?

**정의**: A*는 시작점에서 노드 n까지의 실제 누적 비용 g(n)과, n에서 목표까지의 추정 비용 h(n)을 합한 f(n) = g(n) + h(n)이 가장 작은 노드부터 확장하는 최선 우선 탐색이다. open list(확장 후보, 보통 min-heap)와 closed list(확장 완료 집합)를 유지한다.

**직관**: g는 "지금까지 온 거리", h는 "앞으로 남았을 것 같은 거리"다. h = 0으로 두면 f = g가 되어 다익스트라와 완전히 동일해진다 — 즉 다익스트라는 목표에 대한 정보가 전혀 없는 A*의 특수 경우이고, h가 정확할수록 탐색이 목표 방향으로 좁아져 방문 노드 수가 줄어든다. 반대로 h가 실제 비용을 과대평가하면 최적 경로를 놓칠 수 있다.

**Winters 적용**: `Engine/Private/Manager/Navigation/Pathfinder.cpp`의 `FindPathInternal`(416~569행)이 512×512 그리드 위 8방향 A*를 구현한다. 직선 이동 비용 1.0, 대각 비용 kSqrt2 = 1.41421356237, `std::priority_queue`를 f 비교 min-heap으로 사용하고 goal 도달 시 parent 인덱스를 역추적해 `std::reverse`로 경로를 복원한다.

**함정/꼬리질문**: "h를 크게 키우면(가중 A*) 어떻게 되나?" — 탐색은 빨라지지만 최적성이 깨진다(bounded suboptimal). 게임에서는 종종 의도적으로 쓰는 트레이드오프임을 언급하면 좋다.

### Q. 휴리스틱의 admissibility와 consistency는 무엇이고, 8방향 그리드에서 왜 맨해튼/유클리드가 아닌 옥타일 거리를 쓰는가?

**정의**: admissible(허용 가능)은 모든 노드에서 h(n) ≤ 실제 최단 비용 h*(n), 즉 절대 과대평가하지 않는 성질이다. consistent(일관성)는 삼각부등식 형태 h(n) ≤ c(n, n') + h(n')를 만족하는 것으로, consistent면 admissible이고 노드를 닫은 뒤 다시 열 필요가 없다. 옥타일 거리는 h = max(|dx|,|dy|) + (√2−1)·min(|dx|,|dy|)이다.

**직관**: 8방향 그리드의 실제 최단 이동은 "대각으로 min(|dx|,|dy|)칸, 직선으로 나머지"이므로 옥타일이 장애물 없는 경우의 정확한 비용이다. 맨해튼 |dx|+|dy|는 대각 이동이 허용되는 순간 실제보다 커질 수 있어(대각 1스텝 비용 √2 < 맨해튼 2) inadmissible이 되고 최적성이 붕괴한다. 유클리드 √(dx²+dy²)는 admissible이지만 실제 그리드 비용보다 항상 작거나 같아 탐색이 불필요하게 넓어진다 — 옥타일이 "타이트한 admissible 하한"이다.

**Winters 적용**: `Pathfinder.cpp` 82~87행의 `OctileDistance`가 kSqrt2m1 = √2 − 1 상수(18행)로 위 식을 그대로 구현하며, 시작 노드 push(488행)와 이웃 확장(527행) 양쪽에서 사용된다.

**함정**: "admissible하지만 inconsistent한 h면?" — closed 노드를 재오픈해야 최적성이 보장된다. 옥타일은 consistent라 재오픈이 필요 없다는 점까지 답하면 좋다.

### Q. 그리드 기반과 내비메시(navmesh) 기반 내비게이션의 트레이드오프는?

**정의**: 그리드는 공간을 균일 셀로 이산화해 셀당 walkable 비트를 저장하고, 내비메시는 걸을 수 있는 영역을 볼록 폴리곤 집합으로 표현해 폴리곤 인접 그래프 위에서 탐색한다.

**직관**: 그리드는 갱신(동적 장애물 rasterize)과 임의 좌표 쿼리가 O(1)로 싸고 구현이 단순하지만, 노드 수가 해상도²로 늘고 경로가 8방향 계단 모양이라 후처리(스무딩)가 필수다. 내비메시는 노드 수가 적고 넓은 공간을 소수 폴리곤으로 덮어 탐색이 빠르며 funnel로 자연스러운 경로가 나오지만, 생성/동적 갱신이 복잡하고 에이전트 반경별 메시가 따로 필요하다.

**Winters 적용**: LoL식 톱다운 맵 특성상 512×512, 셀당 0.5 유닛의 비트팩 그리드(`NavGrid.cpp` — kByteSize = (kTotalCells+7)/8, 비트 접근 bits[idx>>3] >> (idx&7) & 1)를 택했다. `WorldToCell`은 floor((world − origin) / cellSize)로 음수 좌표에도 안전한 내림, `CellToWorld`는 (cellIdx + 0.5)·cellSize로 셀 중심을 복원한다. 그리드의 계단 경로 단점은 별도의 string pulling으로 상쇄한다.

**함정**: "왜 int 캐스트가 아니라 floor인가?" — int 캐스트는 0 방향 절삭이라 음수 월드 좌표에서 셀 경계가 한 칸 어긋난다. 좌표 변환의 off-by-one은 내비 버그의 단골 원인이다.

### Q. 우선순위 큐 기반 A*에서 decrease-key 없이 어떻게 gScore 갱신을 처리하나?

**정의**: 이진 힙 기반 `std::priority_queue`는 decrease-key(힙 내부 원소의 키 감소)를 지원하지 않는다. 대안이 lazy deletion — 더 좋은 g를 발견하면 같은 셀을 새 f로 그냥 다시 push하고, pop 시점에 이미 closed인(또는 stale인) 항목을 검사해 건너뛴다.

**직관**: 힙 안에 같은 셀의 낡은 복사본이 여러 개 떠 있어도, 가장 좋은 복사본이 항상 먼저 pop되므로 정답에는 영향이 없다. 힙 크기가 O(V)가 아닌 O(E)까지 커질 수 있지만, 그리드처럼 간선 수가 노드당 상수(≤8)인 그래프에서는 피보나치 힙 같은 decrease-key 지원 구조보다 상수 계수가 훨씬 작아 실측으로 더 빠른 경우가 대부분이다.

**Winters 적용**: `Pathfinder.cpp` 498~501행에서 pop한 노드가 이미 closed 플래그가 서 있으면 continue로 skip한다. 셀 상태(gScore/parent/closed)는 힙 밖의 배열이 진실이고 힙은 "후보 티켓" 역할만 한다.

**함정**: "stale 검사를 closed가 아니라 gScore 비교로 해도 되나?" — 된다(pop한 f/g가 배열의 현재 값보다 크면 skip). 어느 쪽이든 검사 없이 그냥 확장하면 같은 노드를 중복 확장해 성능이 아니라 정확성까지 흔들릴 수 있다(inconsistent h일 때).

### Q. 8방향 그리드에서 diagonal corner cutting이 무엇이고 어떻게 막는가?

**정의**: 대각 스텝 (dx, dy) (dx≠0, dy≠0)을 허용할 때, 두 직교 이웃 (x+dx, y)와 (x, y+dy) 중 하나라도 막혀 있으면 에이전트가 벽 모서리를 "긁고" 지나가는 문제다. 방지 규약은: 대각 이동은 양쪽 직교 이웃 셀이 모두 walkable일 때만 허용.

**직관**: 셀 중심에서 대각 셀 중심으로 가는 선분은 두 직교 셀의 모서리를 스치므로, 반경이 0이 아닌 에이전트는 실제로는 그 벽에 부딪힌다. 경로는 나왔는데 이동 검증(스윕 검사)에서는 막히는 "플래너-무버 불일치"의 대표 원인이다.

**Winters 적용**: `Pathfinder.cpp` 47행 `CanStepToNeighbor`가 정확히 이 양쪽 직교 셀 검사를 하고, 반경 버전 `CanStepToNeighborForRadius`(59~80행)도 `IsCellWalkableForRadius` 기반으로 같은 규약을 반복한다.

**함정**: "한쪽 직교 셀만 열려 있으면 허용해도 되지 않나?" — 점 에이전트라면 기하적으로 통과 가능하지만, 반경이 있는 순간 모서리 관통이 된다. 플래너의 허용 규칙은 반드시 무버의 충돌 모델과 같거나 더 보수적이어야 한다.

### Q. 매 A* 호출마다 512×512 탐색 배열을 초기화하면 비용이 크다. 어떻게 최적화했나?

**정의**: 세대 카운터(generation stamp) 기법 — 셀마다 uint32 세대 번호를 저장하고, 탐색 시작 시 전역 세대를 1 증가시킨다. 셀에 접근할 때 셀의 세대 ≠ 현재 세대면 "그때서야" gScore = inf, parent = −1로 초기화하고 세대를 찍는다.

**직관**: memset은 O(전체 셀 수) = 262,144셀을 매 호출 지불하지만, 세대 카운터는 초기화 비용을 O(실제 방문 노드 수)로 낮춘다. 짧은 경로(방문 수백 셀)가 대부분인 실시간 게임에서 수백 배 이득이다. 대가는 셀당 4바이트 추가와, 세대가 0으로 되돌아오는 오버플로 시 한 번의 전체 fill.

**Winters 적용**: `Pathfinder.cpp` 123행 `BeginSearchGeneration`이 thread_local gScore/parent/closed 배열의 세대를 올리고, `Touch` 람다(472~481행)가 현 세대가 아닌 셀만 초기화한다. 오버플로(0 rollover) 시에만 전체 fill한다. 배열이 thread_local이므로 워커 스레드 간 데이터 레이스 없이 병렬 A*가 가능하다 — 대신 스레드 수 × 배열 크기만큼 메모리를 쓴다는 트레이드오프를 함께 말해야 한다.

**함정**: "왜 uint8 세대로 안 하나?" — 255회마다 전체 fill이 발생해 프레임 스파이크 요인이 된다. uint32면 사실상 rollover가 없다.

### Q. 플레이어의 raw 클릭 지점과 A*가 만든 웨이포인트는 왜 분리된 결정인가? 직선으로 갈 수 있는 클릭은 어떻게 처리하나?

**정의**: 클릭 의도(raw target)는 "그 지점으로 가고 싶다"는 명령이고, 웨이포인트는 장애물 회피를 위한 파생 데이터다. 목표까지의 세그먼트가 walkable 스윕 검사를 통과하면 A*를 아예 돌리지 않고 [start, goal] 2점 경로를 반환하는 LOS 바이패스를 먼저 시도한다.

**직관**: 그리드 A*는 셀 중심을 지나므로, 뻥 뚫린 평지 클릭에도 첫 웨이포인트가 클릭 방향과 미묘하게 어긋나 캐릭터가 순간적으로 엉뚱한 방향을 보는 현상이 생긴다. "막힌 클릭만 pathfind"가 원칙이다.

**Winters 적용**: `Pathfinder.cpp` 571행 `TryBuildDirectCellPath`가 `SegmentWalkable` 스윕 통과 시 즉시 2점 경로를 반환하고 `AStar::DirectBypass` 프로파일 카운터(589행)를 올린다. 팀 규약으로도 박제되어 있다: 클라이언트는 Move 커맨드에 raw 클릭 XZ를 그대로 실어 보내고(보정된 타겟을 보내면 서버가 원 의도를 잃음), 클릭 의도와 반대 방향인 첫 웨이포인트가 초기 yaw를 결정하면 안 된다.

**함정**: "높이(Y)는?" — 표면 Y가 나쁠 때만 보정값을 쓰고 XZ 의도는 건드리지 않는다. 클라 보정과 서버 권위 해석의 책임 분리를 묻는 꼬리질문에 대비.

### Q. 벽이나 도달 불가능한 지점을 클릭하면 어떻게 처리하나? 매번 탐색으로 풀면 안 되나?

**정의**: 두 단계 사전 계산 캐시를 쓴다. (1) 8-연결 BFS flood fill로 walkable 셀마다 connected component ID를 라벨링, (2) 모든 walkable 셀을 시드로 한 multi-source BFS로 각 셀에서 "가장 가까운 walkable 셀"(nearestCell/nearestComponent/nearestDistance) 필드를 구축 — 사실상 그리드 distance transform이다.

**직관**: multi-source BFS는 모든 시드에서 동시에 물결이 퍼지는 것과 같아, 한 번의 O(전체 셀) 패스로 모든 unwalkable 셀의 최근접 walkable 셀이 나온다. component ID 비교로 "스냅한 셀이 시작점과 같은 섬에 있는가"까지 O(1)로 판정된다 — 도달 불가능한 목표에 A*를 돌리면 open list가 빌 때까지 맵 전체를 태우는 최악 케이스가 되는데, 이를 탐색 전에 차단하는 것이 핵심이다.

**Winters 적용**: `Pathfinder.cpp` 151행 `BuildReachabilityCache`가 캐시를 굽고, 벽 클릭 시 `TryFindNearestReachableGoalInternal`(593행)이 O(1) 룩업으로 목표를 스냅한다. 룩업 실패 시 `FindNearestCellInComponent`(334행)가 링 BFS로 같은 depth 후보 중 시작점까지 옥타일 거리가 최소인 셀을 고른다. 캐시는 NavGrid의 revision/cacheId 증가로 무효화되고 thread_local 벡터에 반경별로 보관된다.

**함정**: 빈 경로를 그냥 무시하는 silent fail — Winters에서 실제로 Chase 미니언이 제자리 애니메이션으로 stuck된 사고가 있었고, 이후 "경로 실패 원인(시작 블록/목표 블록/경로 없음)을 구분해 노출하고, fallback(직선 이동 등)은 명시적으로" 규약이 생겼다. "실패를 enum으로 반환하는 API 설계"까지 답하면 강하다.

### Q. A*가 뱉는 계단 모양 셀 경로를 어떻게 매끄럽게 만드나? string pulling, funnel, Theta*를 비교하라.

**정의**: (1) greedy string pulling — anchor에서 경로의 먼 쪽 웨이포인트부터 역방향으로 LOS(스윕 walkable) 검사를 하고, 통과하는 가장 먼 지점으로 점프해 중간 웨이포인트를 제거. (2) funnel algorithm — 내비메시 포털(폴리곤 간 공유 변) 시퀀스 위에서 좌/우 깔때기 정점을 좁혀가며 이론적 최단 문자열 경로를 구함. (3) Theta* — A* 확장 중에 parent와의 LOS가 있으면 조부모로 직접 연결하는 any-angle 탐색.

**직관**: string pulling은 후처리라 플래너와 독립적이고 구현이 단순하지만 LOS 검사 비용이 경로 길이에 비례하고 greedy라 전역 최단은 보장 못 한다. funnel은 내비메시 전제에서 최단이 보장되지만 그리드에는 그대로 못 쓴다. Theta*는 탐색 자체가 any-angle 경로를 내지만 노드마다 LOS 검사가 붙어 A*보다 비싸다.

**Winters 적용**: 그리드 기반이므로 greedy string pulling을 택했다 — `Server/Private/Game/WalkabilityAuthority.cpp` 193행 `SmoothPathCells`가 `LineCellsWalkableForRadius`(내부적으로 `SegmentWalkable`)로 통과하는 가장 먼 웨이포인트로 점프하며 프루닝하고, 같은 패턴이 `GameRoomInternal.cpp:131`, `ServerMinionWaveRuntime.cpp:102`에도 쓰인다.

**함정**: 스무딩의 LOS 검사가 에이전트 반경을 무시하면(중심선만 검사) 프루닝된 경로가 벽을 긁는다 — 반드시 반경 포함 스윕으로 검사해야 하며 Winters도 ForRadius 버전을 쓴다.

### Q. 에이전트 반경은 어떻게 처리하나? 그리드 인플레이션(clearance map)과 쿼리 시 반경 검사의 트레이드오프는?

**정의**: (1) Minkowski sum 기반 인플레이션 — 장애물을 에이전트 반경 r만큼 사방으로 불린 그리드를 사전에 구워, 이후 에이전트를 점으로 취급. (2) 쿼리 시 검사 — 원본 그리드를 유지하고 매 walkability 쿼리마다 원형 footprint(원 vs 셀 AABB)를 검사.

**직관**: "반경 r인 원이 장애물에 닿지 않고 지나갈 수 있는가"는 "장애물을 r만큼 불린 공간에서 점이 지나갈 수 있는가"와 동치라는 게 Minkowski sum의 핵심이다. 인플레이션은 쿼리가 비트 룩업 1회로 싸지만 반경 종류마다 그리드 하나씩 필요하고 동적 장애물 갱신 시 다시 구워야 한다. 쿼리 시 검사는 메모리가 싸고 임의 반경을 지원하지만 쿼리마다 주변 셀 다중 검사 비용이 든다.

**Winters 적용**: 둘 다 쓴다. `NavGrid.cpp` 318행 `BuildInflated`가 반경 인플레이션 그리드를 통째로 사전에 굽고(clearance map), `IsAreaWalkable`(177행)은 `CircleOverlapsAabbXZ`로 즉석 원형 검사를 한다. `SegmentWalkable`(206행)은 막힌 셀 AABB를 r만큼 확장(231~234행)한 뒤 선분 vs 확장 AABB 교차로 swept-circle 판정을 환원한다.

**함정**: "원을 AABB로 불리면 모서리가 부정확하지 않나?" — 맞다. 정확한 Minkowski sum은 모서리가 반지름 r의 원호가 되는데, AABB 확장은 모서리에서 r(√2−1)만큼 과보수적이다. 게임 내비에서는 이 보수성이 오히려 안전 마진으로 허용되는 경우가 많다는 트레이드오프까지 말하면 좋다.

### Q. 선분 vs AABB 교차(slab method)와 원 vs AABB 겹침 판정을 수식으로 설명하라.

**정의**: slab method — 선분을 P(t) = A + t·(B−A), t∈[0,1]로 파라미터화하고, 각 축마다 슬랩 [min, max]에 대해 t1 = (min − A)/d, t2 = (max − A)/d를 구해 [tEnter, tExit] 구간을 축별로 교집합. 최종 tEnter ≤ tExit이고 구간이 [0,1]과 겹치면 교차(Liang–Barsky 클리핑 계열). 원 vs AABB — 원 중심 C를 박스에 축별 clamp한 최근접점 Q를 구하고 |C − Q|² ≤ r²면 겹침.

**직관**: slab은 "선분이 각 축의 벽 사이 구간을 통과하는 시간 창"의 교집합이 비어 있지 않은지 보는 것이다. d가 0에 가까운 축은 나눗셈 대신 A가 슬랩 안에 있는지로 처리해야 한다. 원-AABB clamp는 "박스 위에서 원 중심에 가장 가까운 점"과의 거리만 보면 충분하다는 관찰이고, sqrt 없이 거리²으로 비교해 비용을 아낀다.

**Winters 적용**: `NavGrid.cpp` 45~73행 `SegmentIntersectsAabbXZ`가 축별 t1/t2 클리핑 slab을 구현해 `SegmentWalkable`의 코어가 되고, 30~43행 `CircleOverlapsAabbXZ`가 clamp 최근접점 판정으로 `IsAreaWalkable`의 원형 footprint 검사를 담당한다.

**함정**: t1 > t2일 때 swap을 빼먹거나(d의 부호), 0으로 나누기 처리를 빼먹는 것이 단골 실수. "swept circle vs AABB는?" — 박스를 r만큼 확장하고 선분 vs 확장 AABB로 환원한다고 답하면 위 질문과 연결된다.

### Q. 이동하려는 세그먼트가 중간에 막혀 있으면 어떻게 하나? 전부 취소하나?

**정의**: 부분 이동을 허용한다 — 세그먼트를 P(t) = start + t·(end − start)로 두고, t∈[0,1]에서 `SegmentWalkable`이 통과하는 최대 t를 이분 탐색으로 찾아 그 지점까지만 이동시킨다.

**직관**: walkability는 t에 대해 단조("어디까지는 되고 그 뒤로는 안 됨")라고 근사할 수 있으므로 이분 탐색이 성립한다. 12회 반복이면 구간 해상도가 1/4096로, 셀 크기 0.5 유닛 기준 충분한 정밀도다. 전부 취소하면 벽에 비스듬히 걸을 때 캐릭터가 완전히 멈춰 "벽에 붙어 미끄러지는" 자연스러운 거동이 사라진다.

**Winters 적용**: `WalkabilityAuthority.cpp` 410행 `TryClampMoveSegmentXZ`가 12회 이분 탐색으로 최대 t를 찾고, low ≤ 0.001이면 제자리 유지한다. 시작 셀 자체가 unwalkable이면(외력으로 밀려 들어간 경우) `TryFindNearestWalkableCell`(NavGrid.cpp:152, 확장 사각 링 스캔)로 탈출시키는 stuck 복구 경로가 붙어 있다.

**함정**: "walkability가 t에 단조가 아니면?(장애물 지나 다시 열린 공간)" — 이분 탐색은 첫 번째 막힘 이전의 안전한 t를 보수적으로 찾을 뿐이며, 그 목적에는 단조 가정 위반이 문제되지 않는다는 점을 짚으면 좋다. 또 하나: 클램프 결과가 계속 0에 수렴하면 stuck인데, 이를 조용히 넘기지 말고 사유(stuck/resolve reason)를 디버그로 노출해야 한다는 것이 팀 규약이다.

### Q. Reynolds 스티어링(seek/arrive/separation)을 설명하고, Winters의 각도 부채꼴 회피가 RVO/ORCA 계열과 어떻게 다른지 말하라.

**정의**: seek는 desired = normalize(target − pos)·maxSpeed로 목표를 향한 속도를 만들고, arrive는 목표 반경 안에서 속도를 거리에 비례해 감쇠시켜 오버슈트를 막는다(스텝은 min(speed·dt, dist)로 클램프). separation은 이웃마다 반발 벡터를 거리 역비례로 합산한다. RVO/ORCA는 상대 속도 공간에서 "미래 충돌을 일으키는 속도 집합(velocity obstacle)"을 계산해 그 밖의 속도를 선형계획으로 고르는 예측형 회피다.

**직관**: 부채꼴 샘플링은 "지금 원하는 방향이 막혔으면 조금씩 틀어서 첫 번째로 뚫린 방향을 택한다"는 반응형(reactive) 접근이고, RVO는 "상대도 피할 것"을 가정한 예측형이다. 부채꼴은 구현이 단순하고 결정론적이며 navgrid 검사와 자연스럽게 결합되지만, 정면 대치에서 서로 같은 쪽으로 트는 진동이 날 수 있다. RVO/ORCA는 밀집 군중에 강하지만 비용과 구현 복잡도가 크고 서버 결정론 유지가 까다롭다.

**Winters 적용**: `Shared/GameSim/Systems/Move/MoveSystem.cpp` 260행 `ResolveAvoidedDirection`이 kAngles = {0, ±35°, ±70°, ±90°}(rad로 0, ±0.610865, ±1.22173, ±1.570796) 순서로 원하는 방향을 `RotateXZ`로 회전시켜 후보를 만들고, 블로커 클리어(`IsCandidateClear` — 아군은 거리가 늘어나는 separating 후보만 허용) + navgrid `SegmentWalkableXZ`를 모두 통과하는 첫 방향을 채택한다. 전부 실패하면 zero vector와 함께 'avoidance-dead-end' stuck 트레이스를 남긴다. 서버 미니언도 `GameRoomUnitAI.cpp:1351`에서 동일 부채꼴을 쓴다.

**함정**: "왜 0°를 첫 후보로 두나?" — 원 의도를 최우선 보존하고, 회피는 필요할 때만 최소로 트는 것이 원칙이기 때문. 매 프레임 재패스(A* 폭주)로 회피를 풀려는 시도는 쿨다운/틱당 빌드 버짓/blocked-frame 히스테리시스로 억제해야 한다는 리패스 정책 꼬리질문에 대비.

### Q. 미니언 군중이 서로 겹치는 문제를 소프트 분리력으로 어떻게 풀었나? 완전히 겹친 두 유닛은?

**정의**: penetration 가중 depenetration — 이웃 블로커마다 penetration = minDist − dist (minDist = (rA + rB)·radiusScale + padding)를 구하고, push += (away/dist)·penetration·weight를 누적한 뒤 정규화해 최대 스텝으로 클램프한 변위를 적용한다.

**직관**: 하드 해소(한 프레임에 완전 depenetration)는 밀집 웨이브에서 유닛들이 팝핑하며 진동(jitter)하는 반면, 소프트 해소는 침투 깊이에 비례한 작은 힘을 여러 틱에 걸쳐 적용해 부드럽게 풀린다. radiusScale < 1로 미니언끼리는 약간의 겹침을 허용하는 것이 LoL식 "몸비비기" 감성이고, maxStep 클램프가 폭발적 밀림과 진동을 막는 댐퍼 역할을 한다.

**Winters 적용**: `GameRoomUnitAI.cpp` 950행 `TryResolveMinionDepenetrationStep`. 미니언 간에는 소프트 계수(kMinionSoftSeparationRadiusScale = 0.65, Weight = 0.35, MaxStep = 0.18 — `ServerMinionTuning.h:13-15`), 구조물은 weight 1.0(하드). 최종 push는 `TryClampMoveSegmentXZ`로 navgrid 가드를 거친다. 완전 겹침(distSq ≤ 1e-4)이면 away 방향이 정의되지 않으므로, entity ID를 spatial hash 소수 73856093/19349663으로 해싱해 결정론적 밀어내기 방향을 생성한다(983~989행) — rand()를 쓰면 서버 재실행/리플레이 결정론이 깨진다.

**함정**: "왜 클라가 아니라 서버에서?" — 위치는 서버 권위 상태라 분리력도 서버 시뮬에 있어야 스냅샷과 일관된다. 이웃 수집 비용 꼬리질문: `CSpatialIndex::QueryRadius`(SpatialIndex.cpp:60)가 uniform grid 해시맵으로 cellRadius = ceil(r/cellSize)+1 범위 셀만 순회 — 전수 O(N²) 페어 검사를 O(N·이웃수)로 낮춘다. 셀 크기는 대략 평균 쿼리 반경/유닛 지름 스케일로 잡아야 셀당 후보 수와 순회 셀 수가 균형 잡힌다.

### Q. 이동 방향에서 바디 yaw를 만드는 수학과, ±π 경계에서 캐릭터가 한 바퀴 도는 버그를 어떻게 막았는지 설명하라.

**정의**: yaw = atan2(dir.x, dir.z) + yawOffset — +Z를 전방으로 보는 톱다운 XZ 규약(일반 수학의 atan2(y, x)와 인자 순서가 다름에 주의). 정규화는 NormalizeRadians로 fmod 기반 [−π, π) wrap. 최근접 코터미널 각은 NearestEquivalentRadians(yaw, ref) = ref + NormalizeRadians(yaw − ref) — yaw와 2π 배수만큼 차이나는 각들 중 현재 값 ref에 가장 가까운 것을 고른다.

**직관**: 매 틱 yaw를 [−π, π)로 정규화해 저장하면, 캐릭터가 −π 근처를 향한 채 좌우로 미세하게 흔들릴 때 저장값이 +π ↔ −π를 오가고, 보간기는 그것을 "거의 한 바퀴 회전"으로 해석해 스핀 아티팩트가 난다. 그래서 저장 yaw는 연속값(코터미널 중 현재값 최근접)으로 유지하고, 정규화는 wire 전송/로그/델타 비교에서만 한다 — "상태는 연속, 표현은 정규화"가 원칙이다.

**Winters 적용**: `Engine/Include/WintersMath.h` 178행 `YawFromDirectionXZ`, 188행 `NormalizeRadians`, 199행 `NearestEquivalentRadians`. 챔피언 래퍼(`ChampionRuntimeDefaults.cpp:39-68`)의 `ResolveChampionVisualYawNear`가 방향→yaw→최근접 코터미널을 합성하며, 서버 CommandExecutor/MoveSystem의 모든 Transform yaw 쓰기가 이 경로를 지나도록 팀 규약(gotchas)으로 박제되어 있다. 실제로 빠른 연속 우클릭에서 ±π 재교차 스핀이 발생했던 사고에서 나온 규칙이다.

**함정**: "yawOffset은 왜 있나?" — 메쉬 전방축이 에셋 패밀리마다 다르기 때문. 수정된 .wmesh(Irelia/Yasuo/Viego)는 0, raw Riot FBX 바디는 +π 보정을 `GetDefaultChampionVisualYawOffset`이 챔피언별로 반환한다. 호출부마다 +π를 흩뿌리는 순간 유지보수가 붕괴하므로 오프셋은 단일 함수로 중앙화한다. 또 레거시 미니언은 `NavigationSystem.cpp:20-39` `FaceMoveDirection`이 의도적으로 atan2(−x, −z) 역방향 페이싱을 쓰므로, 챔피언 규약을 전 유닛에 일괄 적용하면 안 된다.

### Q. 서버 권위 이동에서 클라이언트가 예측한 yaw/이동을 스냅샷이 덮어쓸 때 생기는 문제와 해결책은?

**정의**: 클라이언트는 클릭 즉시 로컬에서 이동 방향/yaw를 예측 표시하고, 서버 스냅샷이 도착하면 권위 상태로 수렴시킨다. 문제는 (1) 아직 그 클릭을 반영하지 않은 오래된 스냅샷이 예측 yaw를 되돌려 "고개가 휙 돌아갔다 돌아오는" 팝핑, (2) 클라 로컬 내비/속도 시스템이 스냅샷 적용값을 같은 프레임에 덮어쓰는 파이프라인 순서 충돌이다.

**직관**: 서버 권위는 "최종적으로 서버가 맞다"는 뜻이지 "모든 프레임에 서버의 낡은 값이 이긴다"는 뜻이 아니다. 예측값은 서버가 실제로 따라잡을 때까지 보호해야 하고, ack 시퀀스가 앞섰다는 사실만으로는 "그 스냅샷의 yaw가 내 클릭을 반영했다"는 보장이 없다 — 시퀀스 진행과 상태 수렴은 별개 신호다.

**Winters 적용**: SnapshotApplier가 Hello로 받은 로컬 net id를 기억하고, 서버 yaw가 실제로 예측 yaw에 수렴하거나 액션 락이 페이싱을 인수할 때까지 예측 move yaw를 보호한다 — `lastAckedCommandSeq`가 전진했다는 이유만으로 보호를 해제하지 않는다(gotchas 규약). 또 클라 `CNavigationSystem`이 SnapshotApply와 SyncFromECS 사이에서 복제 챔피언의 yaw를 덮어쓰는 사고가 있어, 복제 챔피언에는 로컬 NavAgent/Velocity 이동 시스템을 아예 돌리지 않는다 — 그로 인해 이동이 계단식이 되면 로컬 내비를 되살리는 게 아니라 스냅샷 보간/예측을 고치는 것이 규약이다.

**함정**: "rapid click 스팸은?" — 서버는 같은 세션의 pending Move들을 최신 Move로 교체(coalescing)해 stale 클릭마다 가시적 스티어링 턴이 생기는 것을 막고, non-move 커맨드는 그대로 권위를 유지한다. 예측·보간·권위의 3자 우선순위를 명확히 설계했는지가 이 질문의 핵심 평가 포인트다.

---

## 네트워크 / 스냅샷 보간 수학

### Q. 서버 시뮬레이션을 왜 고정 틱(fixed timestep)으로 돌리는가? 가변 dt로 돌리면 무엇이 깨지는가?

**정의:** 고정 틱은 시뮬레이션을 항상 동일한 dt(예: 1/30초)로 전진시키는 방식이다. 시간 환산은 t_ms = tick * 1000 / tickRate로 정수 틱 인덱스가 단일 진실(single source of truth)이 된다. 렌더 프레임과 분리할 때는 accumulator 패턴(acc += frameDt; while acc >= fixedDt { Step(fixedDt); acc -= fixedDt; })을 쓴다.

**직관:** 부동소수점 연산은 결합법칙이 성립하지 않으므로(a+b)+c ≠ a+(b+c), dt가 프레임마다 달라지면 같은 입력이라도 결과 상태가 달라진다. 고정 dt + 고정 명령 처리 순서 + 결정론적 순회가 갖춰져야 서버/클라(또는 리플레이)가 같은 입력에서 같은 상태를 재현할 수 있다.

**Winters:** 서버 틱 스레드는 `Server/Private/Game/GameRoomTick.cpp:72`에서 steady_clock 기준 `sleep_until(next += 33333us)`로 30Hz 루프를 돈다 — `sleep(33ms)` 방식과 달리 다음 기상 시각을 절대 시각으로 누적하므로 sleep 오차가 드리프트로 쌓이지 않는다. kFixedDt = 1/30, kTicksPerSecond = 30은 `Shared/GameSim/Core/Determinism/DeterministicTime.h:9`가 단일 소유하고, `GameRoom.cpp:376`에서 serverTimeMs = tick*1000/30으로만 환산한다. 매 틱 DrainCommands → ExecuteCommands → SimulationSystems → BroadcastEvents → BroadcastSnapshot 순서가 고정이라 명령 적용 시점도 결정론적이다.

**함정/꼬리질문:** "sleep_until 대신 sleep(33ms)를 쓰면?" — 매 틱 스케줄러 오차(1~15ms)가 누적돼 실제 틱레이트가 30Hz 밑으로 떨어지고 serverTimeMs와 벽시계가 어긋난다. "틱이 밀리면(spiral of death)?" — accumulator에 상한을 두거나 틱을 드랍하는 정책이 필요하다는 것까지 말하면 좋다.

### Q. 선형 lerp, smoothstep, 지수 감쇠 스무딩의 차이를 수식과 함께 설명하라. 왜 네트워크 보간에 smoothstep을 쓰는가?

**정의:** lerp(a,b,t) = a + (b-a)*t는 속도가 일정한 C0 보간. smoothstep은 s(t) = t*t*(3 - 2t) = 3t² - 2t³로 t를 재매핑한 뒤 lerp하며, s'(0) = s'(1) = 0이라 시작/끝에서 속도가 0으로 붙는 C1 연속이다. 지수 감쇠는 x += (target - x) * (1 - exp(-k*dt))로 목표에 점근하는 방식(정확히는 도달하지 않음, 프레임레이트 독립을 위해 exp 형태 필수).

**직관:** lerp는 보간 구간의 시작과 끝에서 속도가 불연속으로 점프해 스냅샷 경계마다 "덜컥"이는 시각적 팝핑이 생긴다. smoothstep은 가속-등속-감속 형태라 구간이 이어져도 속도 불연속이 완화된다. 지수 감쇠는 구현이 가장 단순하지만 목표 도달 시점이 불명확하고 큰 오차에서 초반 이동이 과격하다.

**Winters:** `Client/Private/Scene/Scene_InGameNetwork.cpp:138`의 SmoothStep01이 t*t*(3-2t) Hermite 이징으로 55ms 윈도우 동안 위치/회전을 전진시키고, t >= 1에서 목표에 정확히 스냅해 도달 시점이 결정적이다.

**함정:** smoothstep도 구간과 구간 사이 가속도는 불연속(C2 아님)이다. "완전히 매끄럽게 하려면?" — Catmull-Rom/Hermite 스플라인으로 이웃 스냅샷 4개를 쓰거나 속도까지 상태에 포함시키는 방법을 언급하라.

### Q. 스냅샷 보간(interpolation)과 외삽(extrapolation)의 차이는? interpolation delay는 왜 필요한가?

**정의:** 보간은 이미 수신한 두 스냅샷 S(t0), S(t1) 사이를 렌더 타임 t_render에 대해 t = (t_render - t0)/(t1 - t0)로 lerp하는 것이고, 외삽(데드레코닝)은 마지막 상태 + 속도로 미래를 추정(x = x_last + v * dt_elapsed)하는 것이다. 보간을 하려면 렌더 타임을 최신 스냅샷보다 과거로 미뤄야 하며(t_render = t_server - interpDelay), 이 지연 버퍼가 지터 버퍼 역할을 한다. 통상 interpDelay는 스냅샷 간격의 1.5~2배(30Hz면 약 50~100ms)로 잡아 스냅샷 하나가 늦거나 유실돼도 보간 구간이 마르지 않게 한다.

**직관:** 보간은 "확정된 과거를 정확히 재생"(지연 비용), 외삽은 "불확실한 미래를 추측"(오차·롤백 비용). 방향 전환이 잦은 MOBA 이동은 외삽 오차가 커서 보간이 유리하다.

**Winters:** Winters는 고전적 타임라인 버퍼 대신 "적용 후 이징" 방식을 쓴다. 스냅샷 도착 시 `Scene_InGameNetwork.cpp:665`의 CaptureNetworkActorInterpolationStarts가 현재 포즈를 시작점으로 캡처하고, 694행 BeginNetworkActorInterpolationForSnapshot이 스냅샷이 덮어쓴 포즈를 목표로 삼은 뒤 트랜스폼을 시작점으로 되감아, 761행 ApplyNetworkActorInterpolation이 55ms(kNetworkActorInterpDurationSec = 0.055) 동안 smoothstep으로 전진시킨다. 외삽은 하지 않고 t >= 1에서 스냅한다.

**트레이드오프(꼬리질문 대비):** 표준 버퍼 방식은 스냅샷 도착 지터를 지연으로 완전히 흡수하지만 상시 +지연을 지불한다. Winters 방식은 추가 고정 지연이 없고 구현이 단순한 대신, 스냅샷 도착 간격이 55ms보다 벌어지면(패킷 지연·유실) 이징이 먼저 끝나 정지-점프가 보이고, 보간 목표가 항상 "가장 최근 스냅샷"이라 타임라인상 두 스냅샷 사이의 정확한 재생은 아니다. "왜 55ms인가?"에는 30Hz 스냅샷 간격 33ms의 약 1.7배로, 다음 스냅샷이 도착하기 전 이징이 끝나 팝핑되는 것과 항상 목표에 못 미쳐 끌리는 것 사이의 절충이라고 답하면 된다.

### Q. 이동 방향 벡터에서 yaw를 구하는 수식과, 각도를 [-π, π)로 정규화하는 방법을 설명하라.

**정의:** +Z를 전방으로 하는 좌표계에서 yaw = atan2(dir.x, dir.z)다(일반 수학의 atan2(y,x)와 인자 순서가 다름에 주의 — z가 코사인 축, x가 사인 축). 정규화는 NormalizeRadians(a) = a - 2π * floor((a + π) / 2π) 또는 fmod 기반 래핑으로 [-π, π)에 넣는다. 임의 각도 x를 기준각 r의 최근접 등가각으로 옮기는 것은 NearestEquivalent(x, r) = r + Normalize(x - r)이다.

**직관:** 각도는 원 위의 값이라 179°와 -179°는 수치로 358° 차이지만 실제로는 2° 차이다. "최근접 등가각"은 목표각에 ±2πk를 더해 현재각과 가장 가까운 표현을 고르는 것으로, 원 위의 최단호를 수직선 위의 최단 거리로 펴는 연산이다.

**Winters:** `Engine/Include/WintersMath.h:188` NormalizeRadians가 fmod 래핑, :199 NearestEquivalentRadians가 reference + NormalizeRadians(x - reference)를 구현하고, 방향→yaw 변환은 ResolveChampionVisualYawFromDirection = NormalizeRadians(atan2(dir.x, dir.z) + offset)로 챔피언별 비주얼 오프셋까지 합성한다(`Shared/GameSim/Definitions/ChampionRuntimeDefaults.cpp:39`).

**함정:** fmod는 음수 피연산자에서 부호가 피제수를 따라가므로 단순 fmod(a, 2π)만으로는 [-π,π)가 안 나온다 — 오프셋을 더하고 빼는 처리가 필요하다. 또 dir이 영벡터일 때 atan2(0,0)은 정의상 0을 주지만 "방향 없음"과 "정북"이 구분되지 않으므로 호출 전 길이 검사가 필요하다.

### Q. yaw를 스냅샷마다 [-π,π]로 정규화해서 보간하면 어떤 버그가 생기는가? 최단호(shortest-arc) 보간은 어떻게 구현하나?

**정의:** 최단호 보간은 target' = NearestEquivalent(target, current)로 목표를 옮긴 뒤 current += Normalize(target' - current) * t로 전진하는 것이다. delta = Normalize(target - current)가 ±π를 넘지 않으므로 항상 짧은 쪽 호로 돈다.

**직관:** 현재 3.1rad에서 목표 -3.1rad로 갈 때 수치상 6.2rad를 도는 대신, 목표를 3.18rad(= -3.1 + 2π)로 재해석해 0.08rad만 돈다. 문제는 보간 "중간값"을 매 프레임 정규화하면, ±π 경계 근처에서 값이 +π 쪽과 -π 쪽을 오가며 재교차해 캐릭터가 프레임마다 반대로 홱홱 도는 것이다.

**Winters:** 이 문제를 규약으로 못 박았다 — **Transform에 저장되는 yaw는 정규화하지 않는 연속(continuous) 상태**이고, Normalize는 wire/로그/비교 전용이다. 서버는 `SnapshotBuilder.cpp:177`에서 송신 직전에만 NormalizeChampionVisualYaw로 [-π,π] 정규화하고, 클라 `SnapshotApplier.cpp:694`는 수신한 wire yaw를 MakeChampionVisualYawNear(sourceYaw, 현재yaw)로 다시 연속각으로 복원한다. 보간 자체는 `Scene_InGameNetwork.cpp:153` LerpRotationNear가 최근접 등가각 + NormalizeChampionVisualYaw(delta)*t로 수행한다. 실제로 매 서버 이동 틱마다 정규화했다가 빠른 우클릭 연타에서 ±π 경계를 재교차하는 사고가 있었고, 이후 CommandExecutor/MoveSystem의 모든 yaw 쓰기를 ResolveChampionVisualYawNear로 통일했다.

**함정:** "그럼 wire에는 왜 정규화해서 보내나?" — 연속각은 이론상 무한히 커질 수 있어 직렬화 범위·양자화·비교에 부적합하기 때문. 저장은 연속, 전송은 정규화, 수신 시 최근접 복원 — 이 3단계 소유권 구분이 답의 핵심이다. 쿼터니언 slerp를 쓰면 경계 문제가 사라지지만(q와 -q 부호 선택만 처리), yaw 1자유도만 필요한 탑다운 게임에서는 스칼라 최단호가 더 싸고 디버깅이 쉽다는 트레이드오프까지 말하라.

### Q. 챔피언마다 비주얼 yaw 오프셋이 다른 이유는? 전 챔피언에 +π를 일괄 적용하면 안 되는가?

**정의:** 시뮬레이션 yaw(이동 방향의 atan2)와 렌더 메쉬의 전방 축은 별개다. 메쉬의 모델 공간 전방이 +Z가 아니면 visualYaw = simYaw + offset의 상수 보정이 필요하고, offset은 에셋 제작 규약(DCC 툴 축, 익스포터)에 종속된다.

**직관:** 수학이 아니라 데이터 파이프라인 문제다. 같은 게임 안에서도 에셋 출처가 다르면 전방 축이 다르다.

**Winters:** raw Riot FBX로 뽑은 바디 메쉬는 -Z 전방이라 `ChampionRuntimeDefaults.cpp:39` GetDefaultChampionVisualYawOffset이 kPi(+π)를 반환하지만, 자체 변환한 fixed `.wmesh` 계열(Irelia, Yasuo, Viego)은 0이다. 초기에 호출부마다 +π를 흩뿌리는 땜질이 누적돼 챔피언별로 facing이 갈라지는 사고가 났고, 이후 오프셋을 챔피언 정의 한 곳으로 모으고 모든 방향→yaw 변환을 ResolveChampionVisualYawFromDirection으로 라우팅했다. 투사체/FX 메쉬의 180도 보정은 바디 보정과 별도 경로로 분리한다.

**함정:** "서버가 오프셋 포함 yaw를 저장하면 안 되나?" — 서버 시뮬 상태에 렌더 에셋 종속 상수가 섞이면 에셋 교체 시 시뮬이 바뀌는 역결합이 생긴다. 다만 Winters는 wire yaw 자체가 비주얼 yaw 규약이라, 이 규약이 서버-클라 공용 Shared 정의(ChampionRuntimeDefaults)에 박제되어 양쪽이 같은 진실을 본다는 점을 짚으면 좋다.

### Q. 스냅샷 위치를 항상 보간하면 안 되는 경우는? 텔레포트 감지 임계값은 어떻게 설계하나?

**정의:** 프레임당 이동량 d에 대해 d² > T²이면 순간이동으로 판정해 보간 없이 스냅하고, d² < ε²이면 보간을 생략한다. 제곱 비교를 쓰는 이유는 sqrt를 피하기 위해서다(단조성이 보존되므로 대소 비교에 충분).

**직관:** 점멸/텔레포트를 보간하면 캐릭터가 맵을 미끄러져 날아가는 비주얼이 나오고, 1cm 미만 미세 변화까지 보간하면 정지한 유닛이 떨리거나 불필요한 보간 상태가 쌓인다. 임계값은 "정상 이동으로 한 스냅샷 간격에 갈 수 있는 최대 거리"보다 커야 한다 — 이속 상한 * 스냅샷 간격 * 여유 계수.

**Winters:** `Scene_InGameNetwork.cpp:134`에서 kNetworkActorInterpTeleportSq = 9(XZ 거리제곱, 즉 3m 초과는 즉시 스냅), kNetworkActorInterpMinMoveSq = 0.0001(1cm), kNetworkActorInterpMinYaw = 0.0005rad 미만은 보간 생략. 추가로 로컬 Kalista 패시브 대시 중인 챔피언은 bLocalDashProtected(746행)로 보간 대상에서 제외한다 — 로컬 대시 연출을 스냅샷 이징이 되감아 버리는 것을 막기 위해서다.

**함정:** 거리 임계값만으로는 "빠른 대시"와 "텔레포트"를 구분할 수 없다. 정석은 서버가 명시적 teleport/dash 플래그나 이벤트를 내려주는 것이고, 거리 휴리스틱은 그 보조라는 점, 그리고 Y축을 빼고 XZ만 비교하는 이유(지형 높이 스냅으로 인한 오탐 방지)까지 언급하면 좋다.

### Q. 클라이언트 예측(client-side prediction)과 서버 리컨실리에이션의 표준 구조를 설명하고, Winters가 채택한 방식과 비교하라.

**정의:** 표준(Quake/Overwatch식) 구조는 (1) 입력에 시퀀스를 붙여 전송하며 로컬에서 즉시 시뮬, (2) 미ack 입력을 버퍼에 보관, (3) 권위 스냅샷 수신 시 lastAckedSeq 시점 상태로 로컬을 되감고(rewind) 미ack 입력을 전부 재적용(replay), (4) 잔여 오차는 시각적으로만 스무딩. 핵심 불변식은 "클라 표시 상태 = 서버 확정 상태 + 미확정 입력의 재시뮬"이다.

**직관:** 예측은 RTT만큼의 반응성 착시를 만들고, 리컨실리에이션은 그 착시가 권위와 발산하지 않게 주기적으로 접합하는 것이다. 무엇을 예측할지는 오예측 비용으로 결정한다 — 이동은 싸고, 피해/사망 같은 것은 비싸서 예측하지 않는다.

**Winters:** 전체 상태 replay 대신 **선별적 예측 + 보호 카운터** 방식이다. 우클릭 시 PredictLocalMoveYaw(`Scene_InGameLocalSkills.cpp:1455`)가 클릭 방향 yaw를 즉시 적용하고, {commandSeq, 예측 목표, 방향}을 최대 64개 deque에 기록(`Scene_InGameNetwork.cpp:1157` RecordNetworkMovePrediction), 매 스냅샷의 lastAckedCommandSeq로 commandSeq <= ack인 항목을 front부터 정리한다(1177행 PruneAckedNetworkMovePredictions). 위치는 서버 권위를 그대로 받고(이동 자체는 예측 시뮬하지 않음), yaw만 보호한다. MOBA는 클릭-이동이라 FPS처럼 프레임 단위 입력 재적용이 필요 없고, 체감 반응성의 대부분이 "즉시 도는 것"에서 오기 때문에 가장 싼 상태(yaw)만 예측하는 합리적 절충이다.

**함정:** "replay 방식이 항상 우월한가?" — replay는 클라에 결정론적 시뮬 전체가 필요하고(코드 이중화·치트 표면 증가), 30Hz 클릭 이동에는 과설계다. 반대로 Winters 방식의 한계는 위치 예측이 없어 이동 시작 자체는 RTT+틱 지연을 그대로 체감한다는 점 — 이 트레이드오프를 명시적으로 말할 수 있어야 한다.

### Q. 서버가 권위라면 스냅샷의 yaw로 로컬 챔피언을 즉시 덮어쓰면 되지 않나? 예측 yaw '보호'가 왜 필요하고 언제 해제해야 하는가?

**정의:** 예측 상태 보호란, 권위 스냅샷이 도착해도 그 값이 아직 예측을 반영하기 전(파이프라인상 과거)이면 로컬 예측값을 유지하는 것이다. 스냅샷은 "명령이 서버에 도착해 틱에서 실행되고 되돌아오는" RTT + 틱 지연만큼 과거의 세계이므로, ack가 왔다고 그 스냅샷의 yaw가 내 클릭을 반영했다는 보장이 없다.

**직관:** 클릭 직후 로컬은 이미 새 방향을 보는데 스냅샷은 아직 옛 방향이다. 이를 즉시 덮어쓰면 캐릭터가 "돌았다가 → 되돌아갔다가 → 다시 도는" 러버밴딩이 생긴다.

**Winters:** ProtectLocalMoveYaw(`SnapshotApplier.cpp:438`)가 commandSeq와 함께 예측 yaw를 등록하고, 스냅샷 적용부(660행 부근)는 세 가지 해제 조건 중 하나가 될 때까지 서버 yaw 대신 예측 yaw를 유지한다: (1) 서버 yaw가 예측에 0.20rad 이내로 수렴(IsYawClose) — 서버가 따라잡았으므로 정상 인계, (2) Dead/Attack 등 stateFlags 액션락 — 스킬/사망 연출의 facing이 이동 예측보다 우선, (3) ack 이후 kLocalMoveYawMaxProtectedSnapshots = 12 스냅샷(약 400ms) 유예 만료 — 보호가 영구화되어 진짜 발산을 가리는 것을 방지하는 안전핀. 특수 케이스로 서버 yaw가 예측과 정확히 반대(π ± 0.35, IsYawHalfTurn)면 유예 만료를 막는다 — wire 정규화/최단호 복원 과정에서 반턴 등가각으로 플립되는 순간에 보호를 풀면 캐릭터가 눈앞에서 180도 뒤집히기 때문이다. 초기 구현은 lastAckedCommandSeq가 전진하면 바로 보호를 해제했다가 이 러버밴딩을 겪었고, "ack ≠ 반영"이 gotcha로 박제되어 있다.

**함정:** "보호가 치트 표면이 되지 않나?" — 보호는 순수 표현(presentation) 계층이고 서버 시뮬·판정에는 전혀 개입하지 않으므로 권위는 유지된다는 구분이 핵심이다.

### Q. u32 시퀀스 번호가 랩어라운드해도 안전하게 "더 최신"을 비교하는 방법은? 그 한계는?

**정의:** IsNewer(a, b) = (int32)(a - b) > 0. 무부호 뺄셈은 mod 2^32로 정의되므로 랩을 넘어도 차이가 보존되고, 이를 부호 있는 정수로 재해석하면 차이가 2^31 미만인 한 올바른 순서가 나온다. RFC 1982 serial number arithmetic의 실용형이다.

**직관:** 시퀀스 공간을 직선이 아니라 원으로 보고, 두 값 중 "시계 방향으로 반 바퀴 이내에 있는 쪽"을 최신으로 판정하는 것이다. a = 5, b = 0xFFFFFFFE이면 a - b = 7 > 0이므로 랩을 넘은 a가 올바르게 최신이다.

**Winters:** `SnapshotApplier.cpp:111` IsCommandSeqAtLeast와 `EventApplier.cpp:170` IsNewerActionSeq가 모두 이 패턴이며, seq == 0은 "없음" 센티널로 특별 취급해 비교에서 제외한다. 이벤트 중복 재생 차단은 시퀀스 비교(순서 역전)와 FNV-1a 64bit 큐 키((netId, ownerNet, kind, serverTick, extra)를 해시, `EventApplier.cpp:428` BuildCueKey)의 이중 방어다 — 시퀀스는 "오래된 것"을, 해시 집합은 "같은 것의 재전송"을 거른다.

**함정:** (1) 두 값의 실제 거리가 2^31 이상 벌어지면 판정이 뒤집힌다(절반 범위 한계) — 장기 유휴 연결이나 seq 재사용 시 위험. (2) (int32) 캐스트의 무부호→부호 변환은 C++20 이전엔 구현 정의였다(실무상 2의 보수로 동작하지만 면접에서 짚으면 가점). (3) a == b(차이 0)와 정확히 2^31 차이(비교 불능)의 처리를 정의해야 한다.

### Q. 클라이언트가 연타한 Move 명령을 서버가 전부 순서대로 실행해야 하는가? 코얼레싱(coalescing)의 정당성과 위험을 설명하라.

**정의:** 코얼레싱은 큐에 아직 pending인 동종 명령을 최신 것으로 교체하는 것이다. 이동 명령은 멱등적 "목표 상태" 선언(마지막 클릭만 의미 있음)이므로 안전하지만, 스킬 시전 같은 "이벤트성" 명령은 하나하나가 의미라 코얼레싱하면 안 된다.

**직관:** 우클릭 연타 시 스테일 Move를 전부 실행하면 캐릭터가 옛 목표들로 지그재그 조향 턴을 하는 것이 보인다. 사용자의 의도는 "마지막 클릭 지점"뿐이다.

**Winters:** `CommandIngress.cpp:74`가 같은 세션의 pending Move를 최신 Move로 **제자리 교체**한다 — 큐에서 빼고 뒤에 다시 넣는 게 아니라 슬롯을 교체해, Move와 그 사이에 낀 비이동 명령(스킬 등)의 상대 순서를 보존한다. 클라 쪽에도 대칭 규칙이 있다: Move 커맨드에는 클라가 보정한 좌표가 아니라 **raw 클릭 XZ**를 실어 보내고 내비 해석은 서버가 한다(클라 보정을 보내면 서버가 원 의도를 알 수 없게 됨).

**함정:** "제자리 교체하면 교체된 Move의 commandSeq는?" — 최신 명령의 seq를 쓰되, ack/예측 버퍼 정리(PruneAckedNetworkMovePredictions) 쪽에서 건너뛴 seq가 문제없이 정리되는지 확인해야 한다. 또 "왜 큐 뒤에 append하면 안 되나?"에는 순서 재배열로 스킬-이동 인터리빙 의도(무빙샷)가 깨진다고 답한다.

### Q. 투사체는 왜 매 틱 위치를 복제하지 않는가? dead reckoning 방식의 수식과 판정 분리를 설명하라.

**정의:** dead reckoning은 초기 조건과 운동 모델만 복제하고 이후 상태를 수신 측이 적분하는 방식이다. 등속 직선 투사체면 p(t) = p0 + v * t, v = Normalize(dir) * speed, 수명 T = maxDist / speed로 스폰 이벤트 하나가 전체 궤적을 결정한다.

**직관:** 결정론적 운동은 "함수의 파라미터"만 보내면 되고, 매 틱 샘플을 보내는 것은 그 함수를 점으로 다시 그리는 낭비다. 대역폭이 O(투사체 수 * 틱)에서 O(스폰 이벤트 수)로 줄고, 클라 로컬 적분은 프레임레이트로 돌므로 30Hz 샘플 보간보다 오히려 더 매끄럽다.

**Winters:** `EventApplier.cpp:608`이 ProjectileSpawnEvent 수신 시 velocity = Normalize3D(dir) * speed, lifetime = maxDist / speed를 계산해 FX 엔티티를 스폰하고 이후 비행은 클라가 로컬 적분한다. **판정(명중/피해)은 전적으로 서버 시뮬의 투사체가 수행**하고 클라 것은 순수 비주얼이므로, 두 궤적이 수 cm 어긋나도 게임플레이 정합성은 깨지지 않는다. 재전송/중복 스폰 이벤트로 FX가 이중 재생되는 것은 FNV-1a 큐 키 집합(m_seenProjectileCueKeys, 4096개 초과 시 클리어)으로 막는다.

**함정:** 유도(호밍) 투사체나 도중 소멸(막힘/철회)은 이 모델로 안 되고 보정 이벤트(리타겟/디스폰)가 추가로 필요하다. "클라 비주얼이 맞았는데 서버는 빗나갔다면?" — 비주얼은 명중 이벤트 수신 시점에 접합하거나 서버 명중 이벤트로만 히트 FX를 트리거하는 설계를 답하라.

### Q. 래그 컴펜세이션(서버 되감기)의 원리와, 되감기 상한을 200ms로 제한하는 이유를 설명하라.

**정의:** 서버가 엔티티별 과거 상태(위치/HP/사망)를 틱 히스토리로 보관하고, 판정 시 공격자의 지연만큼 과거로 되감아(targetTick = latestTick - rewindTicks) 그 시점 스냅샷 기준으로 명중을 검사하는 기법이다. 필요한 히스토리 깊이는 ticks = ceil(maxRewindMs * tickRate / 1000).

**직관:** 클라이언트가 화면에서 본 것은 RTT/2 + 보간 지연만큼 과거의 세계다. 서버가 현재 상태로 판정하면 "분명 맞췄는데 빗나감"이 되므로, 공격자가 본 과거를 서버가 재구성해 준다 — favor-the-shooter. 대가로 피격자 입장에선 "엄폐한 뒤에 맞는" 억울함이 생기고, 되감기 상한이 그 억울함과 고핑 유저 배려 사이의 정책 다이얼이다.

**Winters:** `Server/Private/Security/LagCompensation.cpp:8`이 매 틱 RecordHistory로 엔티티별 deque에 {tickIndex, generation, 위치/HP/사망}을 push하고 kMaxRewindTicks 초과분을 pop한다. kMaxRewindTicks = (200*30 + 999)/1000 = 6틱(`LagCompensation.h:13`) — 정수 ceil 트릭 (a*b + 999)/1000을 쓴다. TryGetHistoricalState는 targetTick 이하의 가장 최신 프레임을 역방향 탐색하는 **floor 샘플링**(프레임 간 보간 없음)이고, generation 불일치 시 히스토리 전체를 클리어해 엔티티 슬롯 재사용으로 죽은 유닛의 과거가 새 유닛에 오염되는 것을 막는다.

**함정:** (1) 200ms 초과 지연 유저는 보상을 못 받는데, 이는 "고핑 유저가 정상 유저의 체감을 망가뜨리지 못하게 하는" 의도적 컷이다. (2) floor 샘플링은 최대 1틱(33ms) 판정 오차를 감수하는 단순화 — 정밀 FPS라면 두 프레임 사이 보간이 필요하다. (3) 되감기는 위치만이 아니라 "그 시점 생존 여부"까지 포함해야 이미 죽은 대상 명중 같은 모순을 막는다는 점(히스토리에 사망 플래그 포함)이 꼬리질문 포인트다.

### Q. 서버 권위 게임에서 복제된 챔피언에 클라이언트 NavAgent/이동 시스템을 계속 돌리면 어떤 버그가 생기는가?

**정의:** 서버 권위 복제에서 한 상태 변수의 쓰기 권한(ownership)은 정확히 한 곳이어야 한다. 복제 수신 상태에 로컬 시뮬 시스템이 같은 프레임에 또 쓰면, 두 권위가 프레임 내 실행 순서에 따라 서로를 덮어쓰는 write-write 경합이 된다.

**직관:** 스냅샷이 yaw/위치를 쓰고 → 같은 프레임 뒤에 클라 NavigationSystem이 로컬 경로 기준으로 다시 쓰면, 최종 렌더에 반영되는 값은 "누가 나중에 실행됐나"로 결정된다. 증상은 원인(이중 쓰기)에서 멀리 떨어진 곳(끊기는 회전, 떨림)에서 나타나 디버깅이 어렵다.

**Winters:** 실제 사고 사례다 — 클라 CNavigationSystem이 SnapshotApply와 SyncFromECS 사이에서 스냅샷이 적용한 챔피언 yaw를 덮어써 회전이 깨졌다. 결론은 규약으로 박제됐다: **복제된 챔피언에 대해 클라 NavAgent/Velocity 이동 시스템을 돌리지 않는다.** 그리고 이동 시스템을 끄자 움직임이 계단식(step-like)이 되면, 로컬 내비를 다시 켜는 게 아니라 스냅샷 보간/예측을 고치는 것이 올바른 수리다 — Winters에선 그것이 55ms smoothstep 이징(`Scene_InGameNetwork.cpp:133`)이다.

**함정:** "그럼 클라 내비는 완전히 무용한가?" — 아니다. 로컬 예측 전용 경로(클릭 즉시 facing, 직선 클리어 판정)나 순수 로컬 엔티티에는 쓸 수 있다. 핵심은 "복제 수신 상태 vs 로컬 소유 상태"를 시스템 스케줄링 레벨에서 분리하는 것이고, 스무딩은 시뮬 상태가 아니라 표현 계층에서만 수행해야 한다는 소유권 원칙이다.

### Q. 스냅샷에 없는 엔티티를 파괴하는 '부재 기반 리핑(reaping)' 방식의 장단점은? 명시적 despawn 이벤트와 비교하라.

**정의:** 전량(full) 스냅샷에서는 스냅샷에 포함된 netId 집합이 곧 생존 집합이므로, 클라가 seenIds - snapshotIds 차집합을 파괴하면 생성/소멸 동기화가 자동으로 맞는다. 델타 스냅샷이나 관심 관리(interest management)가 있으면 "부재 = 사망"이 성립하지 않으므로 명시적 despawn 이벤트가 필요하다.

**직관:** 전량 스냅샷은 "매번 세계 전체의 사진"이라 상태 기술적(state-based)이고 자기 수복적(self-healing)이다 — 한 장을 잃어도 다음 장이 전체 진실이다. 이벤트 기반 despawn은 대역폭이 싸지만 이벤트 유실 시 유령 엔티티가 남으므로 신뢰 전송이나 재확인 메커니즘이 필요하다.

**Winters:** `SnapshotApplier.cpp:1423` OnSnapshot이 전량 스냅샷을 받아 m_seenNetIds - snapshotNetIds 차집합으로 사라진 미니언/워드/센티널을 파괴한다. 이 방식이 안전한 전제 두 가지가 코드에 함께 있다: (1) 서버 `SnapshotBuilder.cpp:117`이 DeterministicEntityIterator + netId 정렬로 스냅샷 자체의 완전성/결정론을 보장하고, (2) 수신 시 FlatBuffers Verifier 검증을 통과한 스냅샷만 적용한다. 그리고 검증 실패를 **침묵 드랍하면 안 된다** — bare return으로 버리면 스키마 드리프트(빌드 불일치)가 네트워크 정지와 구분 불가능한 "조용한 세계 정지"로 보이므로, bounded 로그/카운터를 반드시 남긴다(gotcha로 박제된 규칙).

**함정:** 확장 질문 대비 — 전량 스냅샷은 대역폭이 O(엔티티 수)라 규모가 커지면 델타 압축(기준 스냅샷 대비 변경 필드만 + 비트마스크)과 관심 관리(시야/거리 기반 구독)로 가야 하는데, 그 순간 부재 기반 리핑은 "관심 밖"과 "사망"을 오인하므로 despawn 명시화가 강제된다는 인과를 설명할 수 있어야 한다. Winters의 stateFlags 비트마스크 패킹은 그 방향의 초석이다.

### Q. 클라이언트는 서버 시간을 어떻게 알고, 지터는 어디서 흡수되는가? Winters의 시간 전파 구조를 평가하라.

**정의:** 표준 클록 동기화는 클라가 타임스탬프 t0을 실어 보내고 서버가 (t0, t_server) 에코, 클라가 수신 시각 t1로 RTT = t1 - t0, offset = t_server + RTT/2 - t1을 추정하는 NTP식 절차다(RTT 대칭 가정). 추정 offset은 이동 평균/최소 RTT 샘플로 필터링한다. 지터 버퍼는 도착 간격의 분산을 고정 지연으로 바꿔 소비 측을 등간격으로 만드는 큐다.

**직관:** 렌더 타임라인을 서버 타임라인에 "일정한 뒤처짐"으로 걸어두는 것이 목적이다. 뒤처짐이 일정해야 보간 구간이 마르지도, 넘치지도 않는다.

**Winters:** 서버는 매 스냅샷에 serverTick을 실어 보내고 시간 환산은 serverTimeMs = tick*1000/30 하나로 통일된다(`GameRoom.cpp:376`). 클라는 별도의 클록 오프셋 추정이나 타임라인 버퍼 없이, 스냅샷 도착을 트리거로 55ms 이징을 시작하는 이벤트 구동 방식이다 — 즉 지터는 명시적 버퍼가 아니라 이징 윈도우(55ms > 틱 간격 33ms)의 여유분이 암묵적으로 흡수한다. OnAuthoritativeSnapshot(`Scene_InGameNetwork.cpp:1140`)이 serverTick/ack/pending 개수를 프로파일 카운터로 노출해 이 파이프라인을 계측 가능하게 해 둔 상태다.

**함정/개선 논의(꼬리질문 대비):** 이 구조는 LAN/저지터 환경에선 단순하고 충분하지만, 지터가 틱 간격을 넘나드는 WAN에선 이징 재시작 간격이 요동쳐 속도감이 출렁인다. 개선안은 RTT/offset 추정 → t_render = t_server_est - interpDelay 타임라인 구축 → 스냅샷을 (tick, state) 쌍으로 버퍼링해 타임라인 보간, 그리고 interpDelay를 관측 지터의 백분위수로 적응 조정하는 것 — "현재 구조의 전제(안정 도착 간격)가 깨지는 조건과 그때의 다음 단계"를 말할 수 있으면 설계 이해를 증명하는 답이 된다.

---

## AI 탐색 / 의사결정 수학

### Q. MCTS의 4단계(Selection / Expansion / Simulation / Backpropagation)를 설명하고, 각 단계가 왜 필요한지 말해보세요.

**정의.** MCTS는 한 번의 반복(iteration)마다 (1) Selection — 루트에서 트리 정책(UCB1 등)으로 가장 유망한 리프까지 내려가고, (2) Expansion — 그 리프에 자식 노드(가능한 행동)를 추가하고, (3) Simulation(rollout) — 그 지점부터 기본 정책(default policy)으로 종단까지 시뮬레이션해 보상을 얻고, (4) Backpropagation — 그 보상을 경로상 모든 노드의 `visits`/`totalReward`에 누적하는 루프입니다.

**직관.** "어디를 더 파볼지"(Selection)와 "판 결과가 어땠는지"(Simulation)를 분리해, 전체 게임 트리를 다 펼치지 않고도 통계적으로 유망한 가지에 예산을 집중시키는 구조입니다. 트리는 비대칭적으로 자랍니다 — 좋은 수 주변만 깊게.

**Winters.** `CMCTSPlanner::Plan`(Engine/Private/AI/MCTSPlanner.cpp:79)이 정확히 이 루프입니다. iterations회 동안 `Select → Expand → Rollout → Backpropagate`를 돌고, Expansion은 "이미 한 번 방문된(visits > 0) 미확장 노드"에서만 수행합니다(첫 방문은 리프 자체에서 rollout, 두 번째 방문에서 확장 — 표준적인 lazy expansion). `Backpropagate`(:176)는 부모 포인터를 따라 루트까지 `visits++`, `totalReward += reward`를 누적합니다.

**함정/꼬리질문.** "Expansion을 매 방문마다 하지 않고 한 번 방문된 노드에서만 하는 이유는?" — 메모리와 통계 신뢰도 때문입니다. 방문 0회 노드까지 다 펼치면 트리가 폭발하고, 표본 없는 노드의 UCB 값은 의미가 없습니다. 또 하나: backprop 때 2인 제로섬 게임이면 레벨마다 보상 부호를 뒤집어야(negamax 방식) 하는데, 1인 관점 플래너(Winters처럼 상대를 환경으로 취급)는 부호 반전이 없습니다 — 이 차이를 물으면 "우리 구현은 상대 행동을 모델링하지 않는 단일 에이전트 MCTS"라고 명확히 답해야 합니다.

### Q. UCB1 수식을 쓰고, exploration 상수를 sqrt(2)로 두는 근거를 설명해보세요.

**정의.** UCB1 = Q_i + C · sqrt(ln N / n_i). Q_i = 자식 i의 평균 보상(totalReward/visits, exploit 항), N = 부모 방문 수, n_i = 자식 방문 수, C = 탐험 상수. multi-armed bandit에서 각 팔의 평균 보상에 대한 신뢰 상한(upper confidence bound)을 취해, "낙관적 낙관주의(optimism in the face of uncertainty)"로 팔을 고르는 정책입니다.

**수학적 근거.** Hoeffding 부등식에서 [0,1] 범위 보상의 표본평균이 참평균에서 e 이상 벗어날 확률은 exp(-2·n·e²) 이하입니다. 이 확률을 N^(-4) 수준으로 억제하도록 e를 풀면 e = sqrt(2·ln N / n)이 나오고, 여기서 C = sqrt(2)가 유도됩니다. 이 선택은 누적 후회(regret)를 O(ln N)으로 — 즉 최적 대비 손해가 로그적으로만 자라도록 — 보장합니다.

**직관.** exploit 항은 "지금까지 좋았던 팔", explore 항은 "표본이 적어 아직 모르는 팔"에 대한 불확실성 보너스입니다. n_i가 커지면 explore 항이 1/sqrt(n)으로 줄어들고, 부모가 방문될수록 ln N이 서서히 자라 방치된 자식도 언젠가 다시 뽑힙니다 — 어떤 팔도 영원히 굶지 않는(no starvation) 성질입니다.

**Winters.** `CMCTSPlanner::Select`(MCTSPlanner.cpp:117)가 이 수식을 그대로 구현하며, `m_fExploration = 1.4142f`(MCTSPlanner.h:84)로 이론값 sqrt(2)를 사용합니다. `visits == 0`인 자식은 UCB 계산 없이 즉시 반환해 "미방문 노드 = UCB 무한대" 규약을 분기로 처리합니다(부동소수점 inf 연산과 0 나눗셈을 회피하는 실무적 처리).

**함정.** C = sqrt(2)의 유도는 보상이 [0,1]에 있다는 가정 위에 있습니다. 보상 스케일이 다르면 C도 같이 스케일되어야 합니다 — 다음 질문과 직결됩니다.

### Q. exploration 상수 C를 키우면/줄이면 실제 탐색 행동이 어떻게 달라집니까? 보상 정규화와는 무슨 관계가 있나요?

**정의.** C는 explore 항의 가중치이므로, C가 크면 방문 수가 고른 넓은 트리(BFS에 가까움), C가 작으면 초기 평균이 좋았던 가지에 몰빵하는 깊은 트리(greedy에 가까움)가 됩니다.

**핵심 관계.** UCB1은 exploit 항(보상 단위)과 explore 항(무차원 · C 단위)의 **덧셈**이므로, 두 항의 스케일이 맞아야 균형이 성립합니다. 보상이 [0,1]이면 C ≈ sqrt(2)가 이론적 기준이지만, 보상이 [-100,100]이면 explore 항이 상대적으로 소멸해 사실상 greedy가 됩니다. 그래서 "C 튜닝"과 "보상 정규화"는 같은 문제의 양면입니다.

**Winters.** `EvaluateReward`(MCTSPlanner.cpp:219)가 보상을 myHpRatio − enemyHpRatio, 즉 [-1,1]로 정규화해 두었기 때문에 C = sqrt(2)가 의미 있는 기본값으로 작동합니다. 보상 범위가 [0,1]이 아니라 [-1,1](폭 2)이므로 엄밀히는 C를 2·sqrt(2) 근처로 두거나 보상을 (r+1)/2로 재매핑하는 선택지가 있다는 것까지 말하면 좋습니다 — 실전에서는 어차피 경험적 튜닝 대상입니다.

**꼬리질문 대비.** "iteration이 50처럼 적을 때 C는?" — 예산이 적으면 탐험에 쓸 여유가 없어 C를 낮추는 게 보통 유리합니다. 예산·보상 분산·분기 수가 모두 C의 실효값에 영향을 주므로, C는 이론값이 아니라 시작점이라고 답하는 게 정확합니다.

### Q. 탐색이 끝난 뒤 최종 행동을 고를 때 '방문 수 최대(robust child)'와 '평균 보상 최대(max child)' 중 무엇을 왜 선택합니까?

**정의.** max child = argmax(Q_i), robust child = argmax(n_i). 이론상 방문이 무한하면 둘은 일치하지만, 유한 예산에서는 다릅니다.

**직관.** 평균 보상은 표본 수가 적을수록 분산이 큽니다 — 2번 방문에 운 좋게 높은 평균을 얻은 자식이 max child로 뽑히면 노이즈에 속는 겁니다. 방문 수는 UCB1이 "검증해볼 가치가 있다"고 반복 판단한 결과의 누적이므로, 그 자체가 신뢰도가 반영된 통계량입니다. robust child는 "많이 검증된 좋은 수"를 고르는 보수적이고 안정적인 기준입니다.

**Winters.** `Plan`의 최종 선택(MCTSPlanner.cpp:107-114)은 자식들을 스캔해 `visits`가 최대인 자식의 action을 반환합니다 — 특히 ITERATIONS = 50(MCTSSystem.h)처럼 예산이 작은 환경에서는 robust child가 사실상 필수입니다.

**함정.** "그럼 Q는 왜 저장하나?"라는 꼬리질문에는 "선택(Selection) 단계의 exploit 항으로 쓰이고, 최종 선택에서도 max-robust child(방문 수와 평균이 모두 최대인 자식이 나올 때까지 추가 탐색) 같은 절충 기준에 쓸 수 있다"고 답하면 됩니다.

### Q. 미니맥스/알파베타 대신 MCTS를 쓰는 기준은 무엇입니까? 각각 언제 유리한가요?

**정의.** 미니맥스는 깊이 d까지 전체 트리를 펼쳐 min/max를 교대로 백업하고, 알파베타는 [alpha, beta] 창 밖으로 증명된 가지를 잘라 같은 답을 O(b^(d/2))(최선 순서 기준)까지 줄입니다. MCTS는 통계적 샘플링으로 트리를 비대칭 성장시키는 anytime 알고리즘입니다.

**선택 기준 3가지.** (1) **평가함수** — 알파베타는 리프에서 좋은 정적 평가함수가 있어야 강합니다(체스). 평가함수 설계가 어려운 게임(바둑)이나 상태가 연속적/복합적인 게임에선 rollout 기반 MCTS가 유리합니다. (2) **분기 계수** — b가 크면 알파베타는 깊이를 못 벌고, MCTS는 유망 가지에만 집중합니다. (3) **시간 예산** — 알파베타는 반복 심화(iterative deepening)로 anytime화해야 하지만, MCTS는 본질적으로 아무 때나 끊어도 현재 최선을 반환합니다.

**Winters.** LoL류 실시간 전투는 행동 공간이 연속·동시적이고 완전한 min 상대 모델이 없으므로, 상대를 환경으로 접은 단일 에이전트 MCTS + 근사 forward model(MCTSPlanner.cpp의 ApplyAction)을 선택했습니다. 알파베타의 min 노드가 성립하려면 "상대가 최적 대응한다"는 교대 턴 구조가 필요한데, 실시간 게임에는 그 구조 자체가 없습니다.

**꼬리질문 대비.** "MCTS의 약점은?" — 함정수(trap)·좁은 전술 라인에 약합니다. 랜덤 rollout이 정확히 그 한 수를 샘플링할 확률이 낮아, 얕은 전술은 오히려 알파베타가 잘 봅니다. 그래서 체스 엔진들이 오래 알파베타를 유지했다는 사례를 붙이면 좋습니다.

### Q. 평가(보상)함수를 설계할 때 무엇을 고려해야 합니까? HP 비율 차 같은 zero-sum 스칼라화의 장단점은?

**정의.** 평가함수는 상태를 하나의 스칼라로 접는 함수 V(s)입니다. Winters MCTS의 보상은 r = myHpRatio − enemyHpRatio(적 전체 hp합/maxHp합 기준)로, [-1,1] 범위의 준-제로섬 평가입니다(MCTSPlanner.cpp:219).

**설계 원칙.** (1) **범위 고정** — UCB의 exploit 항과 explore 항의 스케일 정합을 위해 [-1,1]이나 [0,1]로 clamp/정규화. (2) **단조성** — "내가 유리해지는 변화"가 반드시 값을 올려야 함. (3) **제로섬 대칭** — 내 이득과 적 손실을 같은 축에 놓으면 "적 HP만 깎고 나는 안 죽는" 교환 판단이 한 수식에서 나옵니다. (4) **비율 사용** — 절대 HP가 아니라 hp/maxHp를 쓰면 레벨·챔피언에 무관하게 스케일이 유지됩니다.

**직관.** 평가함수는 탐색의 "나침반"입니다. 탐색이 아무리 깊어도 나침반이 잘못 가리키면 정교하게 틀린 답을 냅니다 — 탐색 예산을 늘리는 것보다 평가함수의 방향성이 훨씬 지배적입니다.

**Winters.** `WorldSnapshot::CaptureFromWorld`(:15-52)가 반경 30 내 유닛만 캡처해 평가 대상 자체를 국소화하고, 거리 비교는 sqrt 없이 dx·dx + dz·dz > radiusSq로 처리합니다.

**함정.** HP 차 단일 스칼라는 포탑 위험, 쿨다운, 포지션 가치를 전부 무시합니다 — "이 평가함수로 봇이 포탑 다이브를 안 하는 이유를 설명 못 하죠?"라는 꼬리질문에는 "그 항들은 utility 레이어(ChampionAIValuation)가 담당하고 MCTS는 거시 목표만 낸다"는 계층 분리로 답해야 합니다.

### Q. 롤아웃(default policy)을 균등 랜덤으로 두는 것과 휴리스틱으로 두는 것의 트레이드오프, 그리고 forward model 부정확성이 만드는 sim2real 격차를 설명해보세요.

**정의.** rollout 정책은 리프 이후 종단까지 행동을 고르는 정책입니다. 균등 랜덤은 편향이 없지만(unbiased) 분산이 크고, 휴리스틱 rollout은 분산을 줄이는 대신 휴리스틱의 편향이 평가에 스며듭니다. "약하지만 균형 잡힌 정책이, 강하지만 편향된 정책보다 나은 트리 평가를 주는" 역설이 실제로 관찰됩니다.

**Winters.** `Rollout`(MCTSPlanner.cpp:160)은 uniform_int_distribution으로 균등 랜덤 행동을 최대 m_uMaxDepth = 5스텝, 스텝당 snap.time += 0.5f로 진행합니다. forward model인 `ApplyAction`(:186)은 의도적으로 거친 근사입니다 — 평타 45 / 스킬 70 고정 데미지, Retreat는 HP +30, 그리고 이동 계열 행동(MoveTowardEnemy/MoveAway)은 default로 떨어져 사실상 no-op입니다.

**sim2real 격차.** 플래너는 "내부 시뮬레이터가 그린 세계"에서 최적일 뿐입니다. 모델이 이동을 무시하면 플래너는 거리 조절의 가치를 영원히 배울 수 없고, 고정 데미지 모델은 스킬 적중률·쿨다운을 못 봅니다. 더 위험한 건 **silent fail** — 모델이 틀려도 에러 없이 그럴듯한 답이 나오므로 격차가 계측 없이는 안 보입니다. Winters에서 실제로 겪은 사례가 미니언 pathfinder의 emp� path silent fail이었고, 그 교훈이 ".claude/gotchas — silent fail 환경 모델은 MCTS/RL 도입 시 sim2real 격차가 된다"로 박제되어 있습니다.

**정직하게 짚을 개선점(면접에서 강점이 됨).** 현재 `Plan`은 Selection으로 내려간 트리 경로의 행동들을 스냅샷에 재적용하지 않고 rollout이 루트 상태에서 시작합니다 — 통계적으로 depth-1 bandit에 가깝게 동작하는 구조라, "경로 행동을 ApplyAction으로 순차 적용한 뒤 rollout"이 다음 개선 슬라이스라고 스스로 말할 수 있어야 합니다.

### Q. Utility AI의 선형 가중합 스코어링을 설계할 때 항 간 스케일 정합과 clamp는 왜 중요합니까?

**정의.** utility = clamp01(w0 + w1·f1 + w2·f2 + ...) 형태의 선형 가중합입니다. 각 특징 f_i가 미리 [-1,1]이나 [0,1]로 정규화되어 있어야 가중치 w_i가 "이 항이 결정에 미치는 영향력"이라는 의미를 갖습니다. 정규화 없이 골드차(수천)와 HP비율(0~1)을 섞으면 가중치가 사실상 단위 환산 계수가 되어 튜닝 불가능해집니다.

**Winters.** `ChampionFightValue`(ChampionAIValuation.cpp:31)가 교과서적 예시입니다: 0.45 기저 + 생존여유·0.30 + HealthLead·0.35 + (사거리내 ? +0.20 : −0.10) + 아군웨이브 0.10 + EconomyLead·0.10 − turretDanger·0.50, 최종 Clamp01. 입력 특징인 `EconomyLead`/`HealthLead`(:17, :26)는 먼저 [-1,1]로 clamp되어 상위 utility 항들의 **공용 feature**로 재사용됩니다. `TradeWindow`(:57)는 사거리 밖이면 0으로 **하드 게이트**한 뒤에만 가중합을 계산합니다 — 연속 점수로 표현하면 안 되는 불능 조건은 게이트로 처리한다는 원칙입니다.

**직관.** 가중치의 부호와 크기 자체가 설계 의도를 문서화합니다. turretDanger 가중치가 −0.50으로 절대값 최대인 것은 "안전이 어떤 이득보다 우선"이라는 정책 선언입니다. clamp01은 한 항의 극단값이 다른 항 전부를 압도하는 것을 막는 마지막 안전벨트입니다.

**함정.** 선형 모델은 항 간 상호작용(예: "체력 우위 AND 사거리 내"일 때만 가치 급증)을 못 잡습니다 — 필요하면 곱셈 항이나 response curve(Utility AI에서 흔한 비선형 커브)를 도입한다고 답하세요. 또 가중치 튜닝은 반드시 한 번에 한 항씩, 관측 가능한 행동 변화 기준으로.

### Q. 모든 가치를 '골드'라는 공통 통화로 환산하는 보상 설계의 이유와 한계는?

**정의.** 서로 다른 종류의 이득(킬, CS, 포탑, 레벨)을 비교·합산하려면 공통 단위가 필요합니다. 경제학의 common currency 개념으로, 다목적 최적화를 단일 스칼라 최적화로 환원하는 표준 기법입니다.

**Winters.** `ChampionAIValuation.h:21`이 단일 평가 소스입니다: kChampionKillGold = 300, kMeleeMinionGold = 21, kRangedMinionGold = 14, kTurretGold = 250, kLevelGoldValue = 120(레벨 1 ≈ 120골드), kGoldLeadFullScale = 1000(경제차 1000골드 = 만점). 핵심은 이 상수들이 임의값이 아니라 RewardRegistry의 **실측 지급값**(RewardRegistry.cpp:82-86의 21/14골드)과 정렬되어 있다는 점입니다 — 평가함수가 게임 경제의 ground truth를 따라가므로, 밸런스 패치로 보상이 바뀌면 AI 평가도 한 곳만 고치면 됩니다. `EconomyLead`는 (goldDiff + levelDiff·120) / 1000을 [-1,1]로 clamp해 골드·레벨을 골드 등가로 합산합니다.

**한계.** (1) 골드로 환산 안 되는 가치 — 시야, 포지션, 오브젝트 타이머 압박 — 는 이 축에서 0이 됩니다. (2) 환산 계수는 게임 단계에 따라 변합니다(초반 CS 21골드의 전략적 가치 > 후반). (3) 선형 환산은 한계효용을 무시합니다(1000골드 리드와 5000골드 리드의 승률 차는 비선형). "그래서 kGoldLeadFullScale = 1000으로 포화(clamp)시켜 후반 폭주를 잘라둔 것"이라고 연결하면 설계 의도까지 답하는 셈입니다.

### Q. AI 의사결정이 두 선택지 사이에서 진동(thrashing)하는 문제를 어떻게 막습니까?

**정의.** 두 utility 점수가 교차점 근처에 있으면 매 틱 결정이 뒤집힙니다. 제어이론의 해법이 그대로 적용됩니다: (1) **데드밴드(마진)** — 전환에는 "동점 초과 + 마진"을 요구, (2) **홀드 타이머** — 결정을 최소 시간 유지, (3) **2단 임계값 히스테리시스** — 진입 임계값과 이탈 임계값을 분리(슈미트 트리거와 동일한 구조).

**직관.** 스코어 함수가 연속이고 입력(거리, HP)이 매 틱 미세 변동하는 이상 교차 진동은 필연입니다. 진동은 상태 전환 비용(애니메이션 캔슬, 이동 반전)으로 실제 성능을 깎고, 무엇보다 "봇 같아 보이는" 최대 원인입니다.

**Winters.** 세 기법을 전부 사용합니다. RuleBased brain(ChampionAIBrain.cpp:37)은 championScore >= farmScore + fChampionScoreMargin(기본 0.10)일 때만 교전으로 전환하고(데드밴드), intentHoldTimer로 결정을 일정 시간 고정합니다. PlayerLike brain은 kCommitScale = 1.5f(:86)로 유지 시간을 늘려 봇 특유의 즉답 태세전환을 억제합니다. 별도로 retreatHpRatio 0.10 / reengageHpRatio 0.25(ChampionAISystem.cpp:1880-1883)의 2단 임계값 — 10%에서 빠지고 25%가 되어야 재교전 — 이 전형적 히스테리시스입니다.

**함정.** 마진·홀드를 과하게 주면 반응성이 죽어 명백한 킬각도 놓칩니다. "홀드 중이라도 긴급 조건(처형 HP, 포탑 다이브)은 홀드를 관통해야 한다"는 우선순위 인터럽트 설계까지 말하면 완성입니다.

### Q. 타겟 선택(막타, 처형 타겟, 대시 경유지) 스코어링을 어떻게 설계했고, 왜 전부 argmax 스캔에 제곱거리 비교입니까?

**정의.** 타겟 선택은 후보 집합에 score(c)를 매기고 argmax를 취하는 문제입니다. 스코어는 "가치 항 − 비용 항" 구조가 기본이고, 후보 필터(반경, 팀, 생존)를 먼저 걸어 스캔 비용을 줄입니다.

**Winters 사례 3종.** (1) 저체력 처형 타겟(ChampionAISystem.cpp:988): score = (hpThreshold − hpRatio)·100 − dist — HP 부족분을 지배 항으로, 거리를 패널티로. (2) 막타 미니언(:1066): score = (1 − hpRatio)·60 + distanceFit·25, distanceFit = 1 − clamp01(dist/range) — 낮은 HP와 사거리 적합도의 가중합. (3) 야스오 E 대시 경유 미니언(:1133): score = currentDistSq − afterDistSq — "대시 후 챔피언과의 거리 감소량"이라는 **목표 자체를 직접 스코어화**한 예로, 프록시 지표 대신 최종 목적을 점수로 쓰는 좋은 패턴입니다.

**제곱거리 최적화.** 거리 **비교**에는 sqrt가 불필요합니다(단조 변환이므로 대소 관계 보존): dx·dx + dz·dz > radiusSq. MCTS의 WorldSnapshot 반경 필터(MCTSPlanner.cpp:31)도 동일 패턴입니다. sqrt는 실제 거리값이 스코어의 **합산 항**으로 필요할 때만 지연 계산합니다.

**함정.** (1)처럼 서로 다른 단위(HP비율·100 vs 거리)를 한 식에 더하면 계수가 암묵적 단위 환산이 됩니다 — 동작은 하지만 튜닝 시 계수의 의미를 설명할 수 있어야 합니다. 꼬리질문 "distSq끼리는 더해도 되나?" — 비교는 되지만 합산은 왜곡됩니다(제곱은 비선형), 합산 항이면 sqrt를 풀어야 한다고 답하세요.

### Q. Behavior Tree, Utility, MCTS, RL을 한 AI에 어떻게 계층화했습니까? 각 레이어의 책임 분리 기준은?

**정의.** BT의 합성 노드 의미론: Selector = 자식을 순서대로 시도해 첫 Success에서 성공(우선순위 폴백, OR), Sequence = 전부 성공해야 성공(AND), Parallel = 동시 실행 후 successThreshold로 판정, Decorator(Inverter/Cooldown)는 자식 결과 변환·재실행 게이트. BT는 "반응적 구조"에 강하고, utility는 "연속 점수 비교"에, MCTS는 "미래 시뮬레이션"에, RL은 "데이터로 학습된 정책"에 강합니다.

**Winters의 계층.** Engine/Public/AI/BehaviorTree.h:59의 CBTSelector/CBTSequence/CBTParallel/CBTInverter/CBTCooldownDecorator가 phase 8 의사결정 층이고, 커스텀 게임플레이가 phase 9, MCTSSystem이 phase 10으로 **시스템 실행 순서 자체를 분리**했습니다. MCTS는 5초에 한 번 거시 목표를 계산해 Blackboard "macroGoal"에 기록하고(MCTSSystem.cpp:26), BT 레이어가 그것을 **소비**합니다 — 즉 느린 플래너(전략)와 빠른 반응 레이어(전술)를 시간 스케일로 분리한 구조입니다. 미시 전투 판단은 Shared의 utility(ChampionAIValuation)가 담당하고, RLBridge(RLBridge.h:30)는 STATE_DIM = 24, ACTION_DIM = eMCTSAction::END로 **MCTS와 상태-행동 공간을 공유**해 나중에 학습 정책이 같은 인터페이스로 꽂히도록 설계했습니다.

**직관.** 분리 기준은 "결정의 시간 스케일과 필요한 정보량"입니다. 5초짜리 결정(라인 운영)에 매 틱 CPU를 쓸 이유가 없고, 매 틱 결정(스킬 회피)에 트리 탐색을 돌릴 시간이 없습니다.

**함정.** 레이어 간 목표 충돌 — MCTS가 "전진"을 냈는데 utility가 "후퇴"를 내면 누가 이기는가? Winters는 상위 레이어가 목표(goal)를, 하위 레이어가 실행 가부(veto)를 갖는 구조이며, retreat 임계값 같은 생존 규칙이 최종 거부권을 갖는다고 답할 수 있어야 합니다.

### Q. 탐색 예산을 어떻게 관리합니까? MCTS의 anytime 성질과 프레임 예산의 관계를 설명해보세요.

**정의.** MCTS는 anytime 알고리즘입니다 — 반복 어느 시점에 중단해도 현재까지의 통계로 유효한 답(robust child)을 반환합니다. 따라서 예산 제어 축이 세 개 생깁니다: iteration 수(탐색 품질), 호출 주기(반응성), 호출 대상 필터(누가 이 비용을 쓸 자격이 있나).

**Winters.** MCTSSystem.h:39가 세 축을 전부 사용합니다: TICK_INTERVAL = 5.f초 누적 후에만 실행(주기), ITERATIONS = 50으로 반복 상한(품질 캡), MCTSSystem.cpp:26에서 bot.difficulty < 2는 스킵(난이도 게이트 — 쉬운 봇에게 플래너 비용을 안 씀). 결과는 즉시 행동이 아니라 Blackboard macroGoal로 기록되므로, 5초 지연이 게임플레이 반응성을 해치지 않습니다.

**직관.** 60fps 프레임 예산 16.6ms에서 AI에 줄 수 있는 몫은 밀리초 단위입니다. iteration 수를 고정하면 비용이 결정적(deterministic budget)이라 프레임 스파이크가 없고, 시간 기반 컷(예: 2ms까지)은 품질이 하드웨어에 따라 변합니다 — 서버 권위 시뮬에서는 결정적 예산 쪽이 재현성에도 유리합니다.

**꼬리질문 대비.** "50회로 충분한가?" — 행동 9개 분기에서 50회는 자식당 평균 5-6 방문으로, 거시 목표 4-5개 중 방향을 고르는 데는 충분하지만 깊은 계획에는 부족합니다. 예산을 늘리는 대신 여러 프레임에 iteration을 쪼개는 time-slicing(트리를 유지하며 재개)이 다음 단계라고 답하면 좋습니다.

### Q. 서버 권위 시뮬레이션에서 AI의 결정성(determinism)은 왜 중요하고, RNG는 어디에 격리해야 합니까?

**정의.** 결정성 = 같은 입력 시퀀스가 항상 같은 상태를 만드는 성질. 서버 권위 구조에서 (1) 리플레이 재현, (2) 데스크톱/서버 간 시뮬 일치 검증(smoke test), (3) 버그 재현이 전부 이 성질에 의존합니다. 난수·벽시계 시간·순서 비결정적 컨테이너 순회가 3대 오염원입니다.

**Winters.** 규약이 레이어별로 명시되어 있습니다. 서버 권위 판정에 쓰이는 Shared의 `ChampionAIValuation`은 헤더 주석에 **"결정적: 난수·시간 없음"** 규약을 박아, 같은 ValueInput이면 항상 같은 점수가 나옵니다 — 가치함수는 순수함수여야 한다는 선언입니다. 반면 MCTS의 RNG는 std::mt19937 `m_rng`로 **플래너 내부에만 격리**되어 있고(MCTSPlanner.cpp:74, random_device 시드), 시뮬 상태가 아닌 WorldSnapshot 복사본 위에서만 굴러가므로 탐색의 확률성이 월드 상태를 직접 오염시키지 않습니다.

**직관.** "난수를 쓰지 말라"가 아니라 "난수의 **소유권과 경계**를 정하라"입니다. 판정 경로(Shared 시뮬)는 순수, 탐색/연출 경로는 자기 RNG를 소유 — 이러면 리플레이 시 판정은 재현되고 봇의 탐색만 달라질 수 있는데, 봇 결정까지 재현하려면 m_rng를 고정 시드 + 틱 기반 시드로 바꾸는 스위치가 필요하다는 것까지 말해야 합니다(현재 random_device 시드는 재현 불가능 — 알고 있는 트레이드오프로 제시).

**함정.** float 연산 순서도 결정성을 깹니다(/fp: 옵션, 병렬 리덕션 순서). "RNG만 잡으면 끝"이라고 답하면 감점 포인트입니다.

### Q. UCT와 AlphaZero의 PUCT는 무엇이 다릅니까? 정책 사전확률(prior)을 selection에 어떻게 결합하나요?

**정의.** UCT: score = Q_i + C·sqrt(ln N / n_i). PUCT: score = Q_i + c_puct · P_i · sqrt(N) / (1 + n_i). 핵심 차이는 (1) 신경망 정책이 낸 사전확률 P_i가 explore 항에 **곱해져** 유망해 보이는 수를 먼저 탐색하고, (2) ln이 빠지고 sqrt(N)/(1+n) 형태라 미방문 노드(n=0)도 유한한 값을 가지므로 "미방문 = 무한대" 규약이 필요 없으며, (3) rollout 대신 가치망 V(s)로 리프를 평가한다는 점입니다.

**직관.** UCT는 "모든 자식은 평등하게 의심스럽다"에서 출발하지만, PUCT는 "정책망이 좋다고 한 수부터 의심하라"입니다. 분기가 수백인 게임에서 균등 탐험은 예산 낭비이므로, prior가 탐색의 초기 방향을 잡아주고 방문이 쌓이면 Q가 prior를 덮어씁니다(P의 영향력이 1/(1+n)으로 감쇠).

**Winters.** 이 전환의 배선이 이미 준비되어 있습니다 — RLBridge.h:30이 STATE_DIM = 24, ACTION_DIM = eMCTSAction::END로 MCTS와 행동 공간을 공유하므로, ONNX 정책망(LoadModel은 현재 스텁)이 로드되면 그 logits를 softmax해 P_i로 쓰는 PUCT 업그레이드가 자연스럽습니다. 현재 `BestAction`(RLBridge.cpp:73)은 std::max_element argmax의 탐욕 정책으로, 탐색 없이 정책만 쓰는 최소 경로입니다.

**함정.** prior가 나쁘면 PUCT는 나쁜 수 주변만 파는 확증편향에 빠집니다 — AlphaZero가 루트에 Dirichlet 노이즈를 섞는 이유가 정확히 이것("prior에 대한 강제 탐험")이라고 답하면 이해 깊이를 보여줄 수 있습니다.

### Q. 강화학습의 가치함수와 정책, 보상, 감가율 gamma를 설명하고, 추론 시 argmax와 softmax 샘플링의 차이를 말해보세요.

**정의.** 정책 pi(a|s)는 상태에서 행동의 확률분포, 가치함수 V(s) = E[R_t | s]는 그 상태에서 정책을 따랐을 때의 기대 누적 보상, Q(s,a)는 행동까지 조건화한 버전입니다. 누적 보상은 R_t = r_t + gamma·r_{t+1} + gamma²·r_{t+2} + ... 로, gamma ∈ [0,1)가 미래 보상의 현재 가치를 지수 감쇠시킵니다.

**gamma의 직관.** gamma는 "계획 지평선"입니다 — 유효 지평선이 대략 1/(1−gamma) 스텝이라, gamma = 0.99면 약 100스텝 앞을 봅니다. gamma < 1은 무한합 수렴이라는 수학적 필요이기도 하지만, 게임 AI에서는 "지금의 HP 교환이 30초 뒤 포탑보다 얼마나 중요한가"라는 설계 파라미터입니다. MCTS의 rollout 깊이 제한(m_uMaxDepth = 5)도 같은 역할을 하는 하드 컷 지평선이라고 연결할 수 있습니다.

**argmax vs softmax.** argmax(탐욕)는 결정적이고 최고 성능이지만 다양성이 없고, softmax 샘플링 p_i = exp(logit_i / T) / sum(exp(logit_j / T))는 온도 T로 다양성을 조절합니다 — T→0이면 argmax와 동일, T가 크면 균등분포에 접근. 학습(자기대국) 중에는 샘플링으로 탐험을, 배포·평가 시에는 argmax로 성능을 취하는 게 표준입니다.

**Winters.** RLBridge의 `BestAction`(RLBridge.cpp:73)은 logits에 std::max_element argmax — 배포용 탐욕 정책입니다. 게임 봇 관점에서는 argmax의 결정성이 "예측 가능해서 봇 같아 보이는" 문제를 낳으므로, 난이도별로 T를 달리하는 softmax가 자연스러운 확장이라고 답하면 좋습니다(단, 그 순간 결정성 규약과 충돌하므로 RNG 격리 규약이 다시 등장 — 시드 소유권을 명시해야 함).

**함정.** "STATE_DIM = 24 같은 상태 인코딩에서 뭐가 중요한가?"라는 꼬리질문 — 각 특징의 정규화(HP비율, 거리/최대사거리 등 [0,1] 스케일 통일)가 학습 안정성의 절반이고, 이는 utility 설계에서 EconomyLead/HealthLead를 [-1,1]로 정규화한 것과 정확히 같은 원리라고 묶어 답하세요.

### Q. 지금의 MCTS를 실전 봇 품질로 끌어올린다면 어떤 순서로 개선하겠습니까?

**답의 구조(우선순위와 근거).** (1) **forward model 충실도** — 이동 행동을 no-op에서 실제 위치 갱신으로, 고정 데미지를 챔피언 스탯·쿨다운 기반으로. 평가·탐색이 아무리 좋아도 모델이 틀리면 전부 헛돕니다(sim2real이 병목). (2) **트리 경로 상태 적용** — Selection 경로의 행동을 스냅샷에 순차 적용해 진짜 다단계 탐색으로 만들기(현재는 rollout이 루트 상태에서 시작). (3) **보상 함수 확장** — HP 차 스칼라에 ChampionAIValuation의 turretDanger·EconomyLead를 항으로 편입, 이미 [-1,1] 정규화 규약이 맞으므로 결합 비용이 낮음. (4) **rollout 정책 개선** — 균등 랜덤을 "사거리 내면 공격 우선" 수준의 가벼운 휴리스틱으로(편향 주의, A/B 필수). (5) **예산 구조** — 5초 배치 실행을 프레임 분할 time-slicing으로, 트리 재사용(이전 루트의 선택 자식을 새 루트로) 추가. (6) 마지막에 **PUCT + 정책망** — RLBridge의 공유 행동 공간이 이미 준비된 배선.

**왜 이 순서인가.** "모델 → 탐색 구조 → 평가 → 정책 → 예산 → 학습" 순서는 각 단계의 개선 효과가 앞 단계의 정확성에 곱해지기 때문입니다. 부정확한 모델 위의 정교한 탐색은 정교하게 틀린 답을 냅니다.

**함정.** 개선마다 검증 기준을 붙여야 합니다 — Winters의 원칙대로 "튜닝 전에 계측": macroGoal 결정 분포, 자식별 방문 수/Q 값을 디버그 오버레이로 노출하고, 결정적 시드 리플레이로 전후 비교가 가능해야 개선인지 노이즈인지 구분할 수 있습니다.

---

## 충돌 / 피킹 / 물리 적분 수학

### Q. 마우스 클릭으로 3D 월드의 오브젝트를 집으려면(피킹) 스크린 좌표에서 어떻게 월드 레이를 만드는가?

**정의/수식.** 스크린 좌표 (mx, my)를 먼저 NDC로 변환한다: ndcX = 2·mx/width - 1, ndcY = 1 - 2·my/height (Y축은 스크린과 NDC가 반대라 뒤집는다). 그 다음 NDC의 near점 (ndcX, ndcY, 0)과 far점 (ndcX, ndcY, 1)을 inverse(view·proj)로 월드로 되돌리고(언프로젝트), rayDir = normalize(farWorld - nearWorld), rayOrigin = nearWorld로 레이를 얻는다.

**직관.** 프로젝션은 "월드 → 스크린"으로 가는 압축이므로, 피킹은 그 역행렬로 스크린의 한 점을 "카메라에서 뻗어나가는 선분"으로 되돌리는 것이다. 스크린의 픽셀 하나는 월드에서 점이 아니라 반직선(레이)에 대응한다는 것이 핵심이다.

**Winters 적용.** `Engine/Private/Platform/CInput.cpp:58`의 `GetMouseWorldRay`가 정확히 이 파이프라인이다. `XMMatrixInverse(view*proj)`로 near(z=0)/far(z=1) 두 점을 `XMVector3TransformCoord`로 언프로젝트한 뒤 차를 정규화한다. 여기서 나온 레이가 챔피언 호버 피킹과 지면 클릭 이동 둘 다의 입력이 된다.

**함정/꼬리질문.** `XMVector3TransformCoord`는 변환 후 w-divide(perspective divide)를 자동으로 해주는 함수라는 점을 알아야 한다 — 역프로젝션에서도 동차좌표 w로 나누지 않으면 결과가 틀린다. 꼬리질문: "DirectX는 NDC z가 [0,1]인데 OpenGL은 [-1,1]이다. near점 z를 -1로 쓰면?" → far/near 평면 위치가 어긋나 레이 방향이 틀어진다.

### Q. 지면(y=0 평면) 클릭 위치는 어떻게 구하나? 일반적인 레이-평면 교차식과 함께 설명하라.

**정의/수식.** 평면 n·p + d = 0, 레이 p(t) = O + t·D에서 t = -(n·O + d) / (n·D). y=0 평면은 n=(0,1,0), d=0이므로 t = -O.y / D.y로 단순화된다. 교차점은 O + t·D.

**직관.** 분모 n·D는 "레이가 평면을 향해 다가가는 속도"다. 이것이 0이면 평면과 평행이라 영원히 만나지 않고, t < 0이면 교차점이 카메라 뒤에 있다는 뜻이다.

**Winters 적용.** `CInput.cpp:85`의 `GetMouseGroundPos`가 t = -Origin.y / Dir.y를 그대로 쓰며, 수평 레이(|Dir.y| < 1e-4)와 t < 0을 명시적으로 거부한다. 이 지면점이 우클릭 이동 명령의 raw 클릭 좌표가 된다.

**함정/꼬리질문.** 거부 조건 두 개(평행, 뒤쪽 교차)를 빼먹으면 카메라가 수평에 가까워질 때 0으로 나누거나, 하늘을 클릭했는데 카메라 뒤 지점으로 이동 명령이 나가는 버그가 생긴다. 꼬리질문: "지형이 평면이 아니라 하이트맵이면?" → 레이마칭이나 지형 메시 교차로 대체해야 하고, Winters도 클릭 y는 raw로 두고 서버 nav가 표면 y를 해석하는 쪽으로 설계했다.

### Q. 레이-실린더 교차를 유도해 보라. LoL식 유닛 클릭에 왜 실린더를 쓰는가?

**정의/수식.** Y축 정렬 실린더는 XZ 평면에 투영하면 원이 된다. 레이의 XZ 성분만 취해 O' = (Ox, Oz), D' = (Dx, Dz), 원 중심을 원점으로 옮기면 |O' + t·D'|^2 = r^2. 전개하면 (D'·D')t^2 + 2(O'·D')t + (O'·O' - r^2) = 0. B = O'·D', C = O'·O' - r^2로 두면 판별식 disc = B^2 - lenSq·C이고, 근은 t = (-B ± sqrt(disc)) / lenSq. 각 근에 대해 t ≥ 0이고 교차점 y가 [base.y, base.y + height] 안이면 유효.

**직관.** 3D 문제를 "위에서 내려다본 2D 원 교차 + 높이 구간 검사"로 분해하는 것이다. LoL류 게임 유닛은 어느 방향에서 봐도 대략 세로로 선 기둥이라, 메시 정밀 피킹보다 실린더가 값싸고 클릭 감도도 일관된다.

**Winters 적용.** `Client/Private/Scene/GameplayQuery.cpp:18` `RayVsCylinder`가 이 유도 그대로다(fB, fC, fDisc = fB·fB - fLenSq·fC). 특수 케이스로 레이가 거의 수직(fLenSq < 1e-8)이면 2차방정식이 퇴화하므로, XZ 반경 안인지 먼저 확인하고 상/하 캡 평면과의 t로 처리한다. `TryFindHoverTarget(:78)`은 챔피언/미니언(0.6r×2h)/타워(2r×6h) 등 아키타입별 실린더를 순회하며 히트 중 **가장 작은 t**를 채택한다 — 카메라에 가장 가까운 유닛이 클릭되는 이유다.

**함정/꼬리질문.** (1) 두 근 중 무조건 작은 근만 쓰면 안 된다: 카메라가 실린더 내부에 있으면 작은 근이 음수라 큰 근을 써야 한다. Winters도 두 후보를 순서대로 검사한다. (2) 최근접-t 선택을 빼먹으면 뒤에 있는 큰 타워가 앞의 미니언을 가로채는 클릭 버그가 난다. 꼬리질문: "캡(뚜껑) 히트는?" → 일반 케이스에선 측면만 검사하는데, 탑다운 카메라에선 레이가 충분히 기울어 있어 실용상 문제가 없고, 완전 수직 레이만 캡 평면으로 보완한다.

### Q. 거리 비교에서 sqrt를 피하는 이유와 방법은? epsilon과 NaN은 어떻게 다루나?

**정의/수식.** distance(a,b) ≤ r ⇔ distanceSq(a,b) ≤ r^2 (양변이 음수가 아니므로 단조성이 보존된다). 비교만 필요하면 sqrt를 아예 호출하지 않고 제곱끼리 비교한다.

**직관.** sqrt는 비싸서라기보다(요즘 하드웨어에선 싸다), "불필요한 연산은 하지 않는다"는 원칙과, 제곱 도메인에서 일관되게 비교하면 정밀도 손실 지점이 하나 줄어든다는 의미가 크다. 정규화가 정말 필요할 때만 invDist = 1/dist를 한 번 구해 곱한다.

**Winters 적용.** waypoint 도착 판정(`MoveSystem.cpp`, arriveRadius=0.12의 제곱 비교), 스킬샷 판정(`DistanceSqPointToSegmentXZ` 결과를 반경^2과 비교), 콘 판정의 사거리 필터, 커서 근접 타겟팅(`FindAttackTargetNearCursor`의 (targetRadius+0.85)^2) 등 판정 경로 전반이 제곱 거리 규약이다. 또 `HitVolume.cpp`의 `SanitizeVolume`은 NaN/음수 extents를 판정 전에 정제한다 — NaN은 모든 비교에서 false가 되어 "충돌이 조용히 사라지는" 최악의 실패 모드를 만들기 때문이다.

**함정/꼬리질문.** epsilon은 용도별로 달라야 한다: Winters도 수직 레이 판정은 1e-8(제곱 길이), 레이 y성분은 1e-6, 지면 레이는 1e-4처럼 스케일에 맞춰 다르게 쓴다. "epsilon 하나를 전역 상수로 쓰면 되지 않나?"라는 꼬리질문에는 "비교 대상의 물리적 단위와 제곱 여부에 따라 유효 스케일이 다르다"로 답해야 한다.

### Q. 점과 선분 사이 최단거리를 구하는 공식을 유도하고, 직선 스킬샷 히트박스에 어떻게 쓰이는지 설명하라.

**정의/수식.** 선분 A→B, 점 P에 대해 투영 파라미터 t = dot(P-A, B-A) / |B-A|^2를 구하고 t' = clamp(t, 0, 1)로 자른다. 최근접점은 A + t'·(B-A)이고, 그 점까지의 제곱 거리를 반경^2과 비교한다. |B-A|^2 ≈ 0인 퇴화 선분은 점-점 거리로 처리한다.

**직관.** t는 "P를 선분의 축에 내린 그림자가 선분의 몇 % 지점인가"다. clamp는 그림자가 선분 밖으로 나가면 끝점이 최근접이라는 기하학적 사실을 코드로 옮긴 것이다. 직선 스킬샷은 결국 "두꺼운 선분" = 선분 중심의 캡슐이고, 캡슐 vs 원 판정은 정확히 이 공식이다.

**Winters 적용.** `Engine/Include/WintersMath.h:207` `DistanceSqPointToSegmentXZ`가 공용 유틸이고, Yone Q/R(`YoneGameSim.cpp:260`)과 Viego(`ViegoGameSim.cpp:238`)의 직선 판정, 서버 투사체 스윕 판정이 전부 이 함수로 수렴한다. pOutT로 t를 돌려주는 시그니처라 "선분 위 어느 지점에서 맞았나"까지 재사용할 수 있다.

**함정/꼬리질문.** clamp를 빼먹으면 선분이 아니라 무한 직선 판정이 되어 스킬 사거리 밖 유닛이 맞는다. 퇴화 선분(캐스터 제자리 시전 등) 분기를 빼먹으면 0으로 나눈다. 꼬리질문: "3D 캡슐이면?" → 선분-선분 최단거리로 확장되고 t가 2개가 된다.

### Q. p += v·dt는 어떤 적분 기법인가? 프레임레이트 독립성과 서버 결정론 관점에서 설명하라.

**정의/수식.** 명시적(전진) 오일러: x(t+dt) = x(t) + v(t)·dt. 미분방정식 dx/dt = v를 1차로 근사한 것으로, 국소 오차는 O(dt^2), 누적 오차는 O(dt).

**직관.** "현재 속도가 dt 동안 유지된다고 가정하고 직진"이다. 등속 이동에서는 이 가정이 정확히 참이라 오차가 0이다 — LoL류 이동은 대부분 등속이므로 오일러로 충분한 이유다. 문제는 dt가 프레임마다 다르면 가속이 있는 경로에서 궤적이 프레임레이트에 의존하게 되는 것이고, 해법이 고정 틱(fixed timestep)이다.

**Winters 적용.** `Shared/GameSim/Systems/Move/MoveSystem.cpp:443`에서 step = moveSpeed · multiplier · tc.fDt를 구하고, 목표 방향을 invDist = 1/dist로 정규화한 뒤 advance = min(step, dist)만큼 pos += dir·advance 한다. 서버 권위 시뮬은 고정 틱 dt로 돌므로 같은 입력이면 같은 결과가 나오는 결정론이 보장되고, 이것이 스냅샷/예측 일치의 전제다. 대시류는 별도로 정규화 시간 t = elapsed/duration의 시작-끝 lerp(공중 아크는 sin(t·π), `YasuoGameSim.cpp:665~`)로 적분한다 — 오차가 누적되는 증분 적분 대신 **닫힌형(closed-form) 보간**이라 어느 틱에 평가해도 위치가 정확하다.

**함정/꼬리질문.** advance = min(step, dist) 클램프가 없으면 목표를 지나쳐 다음 틱에 반대로 되돌아오는 오버슛 진동이 생긴다. 꼬리질문: "대시를 pos += dashVel·dt로 적분하면 안 되나?" → 가능하지만 틱 경계 반올림으로 총 이동거리가 미세하게 어긋나고, t 기반 lerp는 종료 시점과 최종 위치가 정의상 정확하다.

### Q. 명시적 오일러, 반암시적(심플렉틱) 오일러, Verlet의 차이는? 게임에서 무엇을 언제 쓰나?

**정의/수식.** 명시적: x += v·dt 후 v += a·dt (위치를 옛 속도로 갱신). 반암시적: v += a·dt 먼저, 그 다음 x += v·dt (새 속도로 위치 갱신). Verlet: x_new = 2x - x_prev + a·dt^2 (속도를 명시적으로 저장하지 않음).

**직관.** 명시적 오일러는 진동/스프링계에서 에너지가 계속 증가해 폭발한다. 반암시적은 순서만 바꿨는데 에너지가 유계로 유지되는 심플렉틱 성질을 가져 물리엔진 기본값이다. Verlet은 위치 기반이라 제약(constraint) 해소와 궁합이 좋아 클로스/로프 시뮬에 쓰인다.

**Winters 적용.** Winters 이동은 등속 + 목표 클램프 구조라 세 기법의 차이가 발생하지 않는 영역이고, 그래서 가장 단순한 명시적 오일러(`MoveSystem.cpp:443`)를 쓴 것이 올바른 선택이다. 발사체 낙하나 넉백에 가속을 도입한다면 그때 반암시적으로 바꾸면 된다 — "필요해질 때까지 가장 단순한 적분기"가 답이다.

**함정/꼬리질문.** "무조건 RK4가 제일 정확하니 RK4를 쓰겠다"는 답은 감점 요인이다. 게임 물리는 정확도보다 안정성·결정론·비용이 우선이고, 등속 이동엔 고차 적분기가 순수 낭비다. 꼬리질문: "고정 틱인데 렌더는 가변 프레임이면?" → 두 스냅샷 사이 보간(interpolation) 또는 마지막 틱에서 외삽(extrapolation)으로 렌더 위치를 만든다.

### Q. 빠른 투사체가 얇은 타깃을 뚫고 지나가는 터널링은 왜 생기고, 어떻게 막나?

**정의/수식.** 이산 판정은 틱 시점의 위치에서만 겹침을 보므로, 한 틱 이동량 speed·dt가 타깃 지름보다 크면 "이번 틱엔 앞, 다음 틱엔 뒤"가 되어 교차 순간을 건너뛴다. 해법은 연속 충돌 검출(CCD): 한 틱의 이동 구간 start → start + dir·speed·dt를 **세그먼트**로 보고, 각 타깃 원(반경 = 투사체 hitRadius + 유닛 전투반경)과 점-선분 제곱거리로 판정한다. 여러 타깃이 맞으면 세그먼트 파라미터 t가 가장 작은(가장 이른) 히트를 채택한다.

**직관.** "점의 스냅샷"이 아니라 "점이 지나간 자취(스윕)"와 충돌시키는 것이다. 투사체 원 + 타깃 원을 합쳐 반경을 한쪽에 몰아주면(민코프스키 합) 문제는 "선분 vs 확대된 원"으로 환원된다.

**Winters 적용.** `Server/Private/Game/ServerProjectileAuthority.cpp:38` `FindSkillProjectileHitTarget`이 이 구조다. `GameRoomProjectiles.cpp:405~`에서 만든 틱 이동 세그먼트를 각 타깃과 `DistanceSqPointToSegmentXZ`로 검사하고 bestT 최소 타깃을 명중 처리해 터널링을 방지한다. 유도형 투사체(`StructureProjectileSystem.cpp:29`)는 매 틱 재조준 + t = step/dist 보간으로 접근하고 hitRadius 도달 시 명중이라 스윕이 필요 없다.

**함정/꼬리질문.** 최소 t 선택을 빼먹으면 배열 순회 순서에 따라 뒤쪽 유닛이 먼저 맞는 비결정적 버그가 된다 — 서버 권위 게임에선 치명적이다. 꼬리질문: "타깃도 움직이면?" → 엄밀히는 상대 속도 프레임에서 스윕해야 하지만, 틱이 짧고 유닛 속도 << 투사체 속도라 타깃을 정지로 근사하는 것이 실용적 트레이드오프다.

### Q. 슬랩(slab) 기법으로 세그먼트 vs AABB 교차를 판정하는 원리를 설명하라. 에이전트 반경은 어디서 처리하나?

**정의/수식.** AABB를 축별 구간(슬랩)의 교집합으로 본다. 각 축에 대해 t1 = (min - O)/D, t2 = (max - O)/D를 구해 [min(t1,t2), max(t1,t2)] 구간을 만들고, tMin = max(축별 진입 t), tMax = min(축별 탈출 t)로 좁혀간다. tMin ≤ tMax이고 구간이 [0,1](세그먼트)과 겹치면 교차.

**직관.** "모든 축의 슬랩 안에 동시에 들어있는 t 구간이 존재하는가"를 묻는 것이다. 각 축은 독립적으로 진입/탈출 시각을 주고, 전체 교차는 그 구간들의 교집합이다.

**Winters 적용.** `Engine/Private/Manager/Navigation/NavGrid.cpp:45` `SegmentIntersectsAabbXZ`가 XZ 2축 슬랩이다. 반경 처리는 두 방식이 공존한다: `SegmentWalkable(:206)`은 비보행 셀 AABB를 에이전트 반경만큼 **팽창**(민코프스키 합)시켜 "점 vs 확대된 박스"로 판정하고, A* 그리드는 `BuildInflated`로 미리 부풀린 그리드를 쓴다. 사전 팽창은 판정이 싸지지만 반경별 그리드가 따로 필요하고, 판정 시 가산은 유연하지만 매번 비용을 낸다 — 이 트레이드오프를 말할 수 있어야 한다.

**함정/꼬리질문.** D가 0인 축(축 평행 레이)은 나눗셈 대신 "O가 슬랩 안인가"로 별도 처리해야 한다. IEEE 부동소수의 ±inf 전파를 이용해 분기 없이 처리하는 트릭도 있지만 0/0 = NaN 케이스가 남는다는 것까지 알면 좋다. 꼬리질문: "팽창 방식의 기하학적 부정확성은?" → 민코프스키 합의 진짜 결과는 모서리가 둥근 박스인데 AABB 팽창은 모서리를 각지게 과대평가한다. 보수적(안 걸릴 것을 걸리게) 오차라 내비게이션에선 허용된다.

### Q. 이동이 벽에 막혔을 때 "벽 직전까지"만 이동시키려면? 이분 탐색 접근의 정밀도-비용 트레이드오프를 설명하라.

**정의/수식.** 세그먼트 start → end가 비보행 영역과 교차하면, low = start, high = end로 두고 mid가 통과 가능하면 low = mid, 아니면 high = mid로 N회 이분한다. N회 후 low가 "마지막으로 통과 확인된 지점"이고, 오차는 |end - start| / 2^N.

**직관.** 통과 가능성은 세그먼트를 따라 단조(한 번 막히면 계속 막힘이라고 가정)라는 성질을 이용한 이진 탐색이다. 해석적으로 벽 경계 t를 푸는 대신, 이미 있는 "통과 가능?" 술어를 N번 재사용하는 실용적 기법이다.

**Winters 적용.** `Server/Private/Game/WalkabilityAuthority.cpp:410` `TryClampMoveSegmentXZ`가 12회 이분으로 이동을 벽 직전까지 클램프한다. 12회면 오차가 이동 길이의 1/4096 — 한 틱 이동이 수 유닛 이하이므로 밀리미터 단위 정밀도를 12번의 술어 호출로 산다. 또 `SmoothPathCells(:193)`는 가시선(LOS) 검사 기반 string pulling으로 A* 셀 경로를 축약해, 격자 모양 지그재그 대신 자연스러운 직선 이동을 만든다.

**함정/꼬리질문.** low를 반환해야지 mid나 high를 반환하면 벽 안에 박힐 수 있다 — "항상 통과 검증된 쪽"을 답으로 삼는 보수성이 핵심이다. 꼬리질문: "단조 가정이 깨지면(벽 뒤에 다시 통로)?" → 이분 탐색은 첫 경계만 근사하므로, 애초에 슬랩 교차로 '막힘'을 먼저 확인한 세그먼트에만 적용해야 안전하다.

### Q. 분리축 정리(SAT)로 OBB 겹침을 판정하는 원리를 설명하라. XZ 평면 2D에서는 왜 축 4개로 충분한가?

**정의/수식.** SAT: 두 볼록체가 분리되어 있다 ⇔ 두 도형의 투영 구간이 겹치지 않는 축(분리축)이 존재한다. 축 후보에 대해 |proj(centerB - centerA)| > projRadius(A) + projRadius(B)이면 분리. 2D 볼록 다각형은 각 변의 법선만 후보로 검사하면 되므로, 박스 2개면 축이 2+2 = 4개다. 3D OBB는 면 법선 3+3에 에지 외적 3×3 = 9를 더해 15개가 필요하다.

**직관.** "어떤 방향에서 빛을 비춰도 두 그림자가 겹치면 물체도 겹친다"의 역이다. 볼록체끼리는 분리축이 반드시 면 법선(2D) 또는 면 법선/에지 외적(3D) 중에 있다는 것이 정리의 내용이고, 3D에서 에지×에지 축을 빼먹으면 "모서리끼리 스치는" 교차를 놓친다.

**Winters 적용.** `Engine/Private/Physics3D/HitVolume.cpp:171` `OverlapYawBox`는 yaw 회전 박스를 XZ 축 2개(axisX = {cos, -sin}, axisZ = {sin, cos})로 표현하고, Y는 단순 구간 겹침으로 분리한 뒤 XZ에서 양 박스의 4개 축에 대해 투영 반경 합(`ProjectRadiusXZ`) vs 중심 거리 투영을 비교한다 — 게임플레이가 사실상 2.5D(yaw만 회전)라는 도메인 지식으로 15축 문제를 4축 + 1구간으로 줄인 것이다. 구-박스는 SAT 대신 로컬 좌표 클램프 최근접점(`OverlapSphereBox:197`), AABB끼리는 min/max 구간 비교(`OverlapAABB:346`), 구끼리는 반경합 제곱거리(`OverlapSphere:356`)로 각각 가장 싼 전용 판정을 쓴다.

**함정/꼬리질문.** 투영 반경 계산에서 projRadius = |ex·(axis·ux)| + |ez·(axis·uz)|처럼 각 절대값의 합이어야 한다 — 절대값을 벡터 합에 씌우면 틀린다. 꼬리질문: "구 vs OBB에 SAT를 쓰면 안 되나?" → 구는 면이 없어 SAT 축 후보가 무한하다(중심-최근접점 방향이 필요). 그래서 박스 로컬로 구 중심을 변환해 clamp(center, -extents, +extents)로 최근접점을 구하고 그 거리와 반경을 비교하는 방식이 표준이다.

### Q. 부채꼴(콘) 범위 스킬 판정은 어떻게 구현하나? acos를 쓰지 않는 이유는?

**정의/수식.** 시전자→타깃 방향 d를 정규화하고, 전방 f와의 내적 dot(f, d)이 cos(halfAngle) 이상이면 콘 내부다. 사거리는 제곱거리로 먼저 필터한다. 각도로 비교하려면 acos(dot) ≤ halfAngle이지만, cos는 [0, π]에서 단조감소이므로 dot ≥ cos(halfAngle)과 동치다.

**직관.** 내적은 "두 단위벡터가 얼마나 같은 방향인가"의 지표(= 사잇각의 코사인)다. 비교 기준 쪽을 미리 cos으로 상수화해 두면, 매 타깃마다 역삼각함수를 호출할 필요가 없다. acos는 비싸기도 하지만 dot이 부동소수 오차로 1.0을 살짝 넘으면 NaN을 반환하는 함정도 있다.

**Winters 적용.** `Shared/GameSim/Champions/Annie/AnnieGameSim.cpp:417` `CollectConeTargets`가 사거리 제곱 필터 → invDist 정규화 → XZ 내적 ≥ halfAngleCos 판정 순서이고, Kalista W 감시자 시야 콘(`KalistaGameSim.cpp:236~`)도 같은 규약이다. 반대로 Ashe W(`AsheGameSim.cpp:154~`)는 판정이 아니라 생성이라, kConeDeg=45를 8등분해 `RotateXZ`로 화살 방향을 부채꼴 분배한다 — "콘 판정"과 "콘 분사"는 다른 문제라는 걸 구분해 말하면 좋다.

**함정/꼬리질문.** halfAngle이 90°를 넘으면 cos이 음수가 되는데 비교식은 그대로 성립한다 — "dot ≥ 0이어야 전방"이라고 하드코딩하면 광각 콘이 깨진다. 정규화 전 영벡터(타깃이 시전자 위치와 동일) 분기도 필요하다. 꼬리질문: "cross까지 쓰면?" → 내적은 좌우 대칭이라 "콘의 왼쪽 절반만" 같은 비대칭 판정엔 외적 부호가 추가로 필요하다.

### Q. yaw를 atan2로 다룰 때 ±π 경계에서 캐릭터가 한 바퀴 도는 문제는 왜 생기고, Winters는 어떤 규약으로 막았나?

**정의/수식.** yaw = atan2(dir.x, dir.z) (+Z 전방 기준, 인자 순서에 주의). 정규화는 fmod로 [-π, π) 범위로 접는다. 문제는 179° → -179°처럼 실제로는 2° 회전인데 값 차이는 358°가 되는 랩어라운드다. 해법은 최근접 등가각: nearest(target, ref) = ref + normalize(target - ref) — 목표각에 2π의 정수배를 더해 기준각과 가장 가까운 표현을 고른다.

**직관.** 각도는 원 위의 점이지 실수 직선이 아니다. 보간/저장을 실수처럼 하면 원의 "이음새"(±π)를 지날 때마다 먼 길로 돌게 된다. 그래서 **저장용 각도는 연속 상태**(랩하지 않고 기준각 근처 표현 유지), **비교/전송용 각도만 정규화**로 역할을 나눠야 한다.

**Winters 적용.** `Engine/Include/WintersMath.h:178~205`에 `YawFromDirectionXZ` / `NormalizeRadians` / `NearestEquivalentRadians`가 있고, 소유권 규약이 명문화되어 있다: Transform에 쓰는 body yaw는 모든 쓰기 지점에서 `ResolveChampionVisualYawNear` 계열(현재 yaw 기준 최근접 등가각)을 거치고, canonical 정규화는 와이어 전송/로그/델타 비교에만 쓴다. 이 규약이 없던 시절, 서버가 매 이동 틱마다 yaw를 정규화하는 바람에 빠른 우클릭 연타 시 ±π 경계를 반복해서 재교차하며 몸통이 스핀하는 버그가 있었다.

**함정/꼬리질문.** (1) atan2(z, x)와 atan2(x, z)는 다른 기준축이다 — Winters는 +Z 전방이라 atan2(dir.x, dir.z). (2) 모델 전방축은 에셋마다 다르다: Winters는 고정 .wmesh(Irelia/Yasuo/Viego)는 오프셋 0, raw Riot FBX는 +π를 `GetDefaultChampionVisualYawOffset`(`ChampionRuntimeDefaults.h:14`)에 중앙화했다. 호출부마다 +PI를 흩뿌리면 챔피언별로 facing이 갈라진다. 꼬리질문: "쿼터니언을 쓰면 해결되나?" → slerp는 랩 문제가 없지만, yaw 단일 자유도 게임플레이에선 스칼라 각 + 등가각 규약이 더 단순하고 디버깅 가능하다.

### Q. 유닛끼리 끼이지 않게 하는 로컬 회피를 어떻게 구현했나? 조향력(steering force)이나 RVO와 비교하라.

**정의/수식.** Winters 방식은 후보 각도 팬 샘플링이다: 원하는 방향을 {0, ±35°, ±70°, ±90°}(라디안 {0, ±0.611, ±1.222, ±1.571})로 회전시킨 후보들을 순서대로 검사해, (1) 블로커 미침범(반경합 + 패딩 0.05), (2) 분리 증가(후보 위치의 블로커 거리 > 현재 거리), (3) 지형 통과(`SegmentWalkableXZ`)를 모두 만족하는 첫 방향을 채택한다. 전부 실패하면 zero 벡터를 반환하고 stuck 사유를 트레이스한다.

**직관.** 스펙트럼으로 보면 — 후보 샘플링(이산적·결정론적·검증 가능), boids식 분리 조향력(연속적·부드럽지만 힘 튜닝이 지옥이고 떨림 발생), RVO/ORCA(상호 회피의 이론적 해답이지만 구현·디버깅 비용이 크고 결정론 보장에 추가 노력 필요). 서버 권위 고정 틱 게임에선 "왜 이 방향을 골랐는지 로그로 재현 가능"한 이산 샘플링이 강력한 실용 선택이다.

**Winters 적용.** `Shared/GameSim/Systems/Move/MoveSystem.cpp:260` `ResolveAvoidedDirection`이 위 구조 그대로이며, 특히 조건 (2) `IsSeparatingCandidate`가 핵심이다 — 단순히 "안 부딪히는 방향"이 아니라 "지금보다 멀어지는 방향"을 요구해서, 이미 겹쳐버린 유닛이 겹침 안에서 맴도는 데드락을 끊는다. 실패 시 zero 반환 + stuck 트레이스는 "이동 버그는 현재 셀/다음 waypoint/보정 방향/stuck 사유를 노출하라"는 팀 디버깅 규약의 일부다.

**함정/꼬리질문.** 각도 후보를 좌우 대칭으로 번갈아 검사하면 두 유닛이 같은 규칙으로 같은 쪽을 골라 계속 마주보는 진동이 생길 수 있다 — 엔티티 ID 기반 좌우 우선순위 부여 같은 대칭 깨기가 꼬리질문 단골이다. 또 회피 방향이 경로 목표에서 90°를 넘으면 사실상 후퇴이므로 후보를 ±90°에서 끊은 것도 의도된 설계다.

### Q. A*에서 octile 휴리스틱이란 무엇이고, 왜 8방향 그리드에서 유클리드나 맨해튼보다 적합한가?

**정의/수식.** octile(dx, dy) = max(|dx|, |dy|) + (√2 - 1)·min(|dx|, |dy|). 8방향 이동(직선 비용 1, 대각 비용 √2)에서 장애물이 없을 때의 **정확한** 최단 비용이다. admissible(실제 비용을 절대 과대평가하지 않음)해야 최적 경로가 보장되고, consistent(h(n) ≤ cost(n, n') + h(n'))해야 닫힌 노드 재확장이 없다.

**직관.** 대각으로 갈 수 있는 만큼(min) 대각으로 가고 나머지(max - min)를 직선으로 가는 비용이다. 맨해튼은 대각 이동을 무시해 과대평가(inadmissible → 최적성 상실), 유클리드는 admissible이지만 그리드 실제 비용보다 항상 작아 탐색 노드가 늘어난다. octile은 이 그리드에서 "가장 빡빡한 admissible 휴리스틱"이라 탐색이 가장 좁다.

**Winters 적용.** `Engine/Private/Manager/Navigation/Pathfinder.cpp:82`의 `OctileDistance`가 이 식이고, 대각 스텝 비용 kSqrt2, `std::priority_queue` open 리스트 + thread_local gScore/closed/parent 배열, tentativeG < gScore일 때만 push하는 lazy-deletion A*(:483~)로 구현되어 있다(우선순위 큐의 decrease-key 대신 중복 push 후 stale 노드를 pop 시 폐기). 목표가 비보행 셀이면 `TryFindNearestReachableGoal`의 BFS로 가장 가까운 도달 가능 셀을 대체 목표로 삼는다 — "경로 없음"을 조용히 실패시키지 않는 것이 과거 미니언 stuck 사고의 교훈이다.

**함정/꼬리질문.** 대각 비용을 1로 두면(체비셰프) 지그재그와 직선의 비용이 같아져 부자연스러운 경로가 나온다. lazy-deletion에서 closed 검사 없이 stale 노드를 처리하면 같은 셀을 여러 번 확장한다. 꼬리질문: "h에 1.001을 곱하면?" → weighted A*로 탐색은 빨라지지만 admissible이 깨져 최적성이 사라지는 트레이드오프다.

### Q. 클릭 이동에서 "raw 클릭 의도"와 "경로 waypoint"를 왜 구분해야 하나? 첫 waypoint가 클릭 방향과 반대일 때 무슨 문제가 생기나?

**정의/수식.** 클릭 지점 P가 보행 가능하고 현재 위치 → P 세그먼트가 지형에 막히지 않으면(SegmentWalkable) 경로 탐색 없이 P를 직접 이동 목표로 쓴다. 막힌 경우에만 A* + string pulling으로 waypoint 열을 만든다. 이동 명령의 페이로드는 클라이언트가 보정한 좌표가 아니라 **raw 클릭 XZ**여야 한다.

**직관.** A* 그리드 경로의 첫 waypoint는 셀 중심으로 스냅되어 있어, 열린 평지에서도 클릭 방향과 미세하게(때로는 크게) 어긋날 수 있다. 캐릭터 facing과 초기 이동이 그 waypoint를 향하면 "오른쪽을 클릭했는데 왼쪽으로 몸을 트는" 체감 버그가 된다. 직선으로 갈 수 있는 클릭은 기하 판정(세그먼트 클리어)이 곧 경로이므로 그래프 탐색 자체가 불필요하다.

**Winters 적용.** 이것은 실제 사고에서 나온 규약이다: 직접 보행 가능·세그먼트 클리어 클릭은 raw 타깃을 유지하고 blocked 타깃만 pathfind하며, 클릭 의도와 반대인 첫 waypoint가 초기 yaw를 구동해선 안 된다. 또 클라이언트가 보정한 좌표를 Move 커맨드로 보내면 서버가 플레이어의 원래 의도를 잃으므로, raw 클릭 XZ를 커맨드에 싣고 서버 nav(`WalkabilityAuthority`)가 목표를 해석한다 — 보정된 안전 y는 표면 높이 문제에만 쓴다. 도착 판정은 waypoint당 arriveRadius(0.12) 제곱거리 비교로, 정확히 점에 도달하길 요구하지 않는다.

**함정/꼬리질문.** arriveRadius가 너무 작으면 부동소수 오차로 waypoint 주위를 도는 오비팅이, 너무 크면 코너를 깎아 벽에 끼는 문제가 생긴다. 꼬리질문: "서버 권위인데 클라 예측 yaw는 어떻게 보호하나?" → Winters는 SnapshotApplier가 로컬 net id를 기억하고, 서버 yaw가 실제로 따라잡거나 액션 락이 걸릴 때까지 예측된 이동 yaw를 스냅샷이 덮어쓰지 못하게 한다. `lastAckedCommandSeq`가 진행됐다는 이유만으로 보호를 풀면, 오래된 스냅샷 yaw가 방금 클릭한 방향을 되감아 캐릭터가 순간 뒤를 봤다 돌아오는 아티팩트가 생긴다.

### Q. 서버 권위 이동에서 클라이언트의 로컬 내비게이션 시스템을 함께 돌리면 어떤 문제가 생기나? 스냅샷 적용과 어떻게 공존시켜야 하나?

**정의/수식.** 서버 권위 모델에서 리플리케이트되는 엔티티의 위치·yaw의 유일한 진실은 서버 틱 스냅샷이다. 클라이언트 표현은 스냅샷 간 보간(과거 두 스냅샷 사이 렌더 시각 t로 lerp, yaw는 최근접 등가각 경유)이나 로컬 예측으로 만들고, 로컬 물리/내비 시스템이 같은 Transform에 쓰면 안 된다.

**직관.** 한 프레임 안에서 "스냅샷 적용 → 로컬 NavSystem 갱신 → 렌더 동기화" 순서로 돌면, 로컬 시스템이 방금 적용된 권위 값을 자기 계산으로 덮어쓴다. 두 개의 쓰기 주체가 같은 상태를 놓고 경합하면 결과는 프레임 순서에 따라 달라지는 지터다.

**Winters 적용.** 실제로 클라 `CNavigationSystem`이 `SnapshotApply`와 `SyncFromECS` 사이에서 스냅샷이 적용한 챔피언 yaw를 덮어쓰는 사고가 있었고, 결론은 규약이 되었다: 리플리케이트 챔피언에 대해 클라 NavAgent/Velocity 이동 시스템을 돌리지 않는다. 이후 이동이 계단식(step-like)으로 보이면 로컬 내비를 다시 켜는 게 아니라 **스냅샷 보간/예측을 고치는 것**이 올바른 방향이다 — 증상을 가리는 쪽이 아니라 소유권을 지키는 쪽으로 수정한다.

**함정/꼬리질문.** "보간은 항상 한 스냅샷만큼 과거를 보여준다"가 꼬리질문 포인트다: 보간 지연 vs 외삽의 오버슛 리스크 트레이드오프, 그리고 로컬 플레이어만 예측으로 지연을 숨기고 원격 유닛은 보간하는 비대칭 설계까지 말하면 완결된다. 또 rapid 클릭 시 서버가 pending 큐의 오래된 Move를 최신 Move로 교체(coalescing)하지 않으면, 스냅샷이 stale한 중간 목표들을 차례로 재생해 눈에 보이는 조향 지그재그가 남는다.

---

## 비전 / 시야(FOV) 수학

### Q. 원형 시야 반경 판정에서 sqrt를 호출하지 않고 제곱거리를 비교하는 이유는 무엇인가? 부동소수점 관점에서 주의할 점은?

**정의/수식**: 점 P가 반경 r인 원 안에 있는지는 dist(P, C) <= r 인데, 양변이 음수가 아니므로 제곱해도 부등호가 보존된다. 즉 dx\*dx + dz\*dz <= r\*r 로 동치 변환이 가능하다. sqrt는 단조증가 함수라 순서를 바꾸지 않기 때문이다.

**직관**: "거리 값 자체"가 필요한 게 아니라 "안/밖"이라는 불리언만 필요하다. sqrt는 나눗셈급으로 비싼 명령이고, 시야 판정은 (소스 수 × 후보 수)만큼 매 틱 도는 핫 루프라 호출 횟수가 곱으로 불어난다.

**Winters 적용**: `VisionSystem.cpp`의 `IsTargetVisibleFast`(라인 347)가 XZ 평면에서 `dx*dx+dz*dz > sightRangeSq`로 판정하고, `sightRangeSq`는 소스 루프 바깥에서 `vs.sightRange*vs.sightRange`로 1회만 계산한다(라인 241). Y는 완전히 무시하는 톱다운 2D 시야 모델이라는 것도 명시적 설계 결정이다.

**함정/꼬리질문**: (1) 제곱은 값의 동적 범위를 두 배로 키운다 — float에서 좌표가 수만 단위면 distSq가 1e8~1e9까지 커져 정밀도가 깎이지만, 비교 연산이라 ULP 수준 오차는 경계 근처 한 텍셀 흔들림 정도로 무해하다. (2) "거리에서 뭔가를 빼야 하는" 경우(예: dist - targetRadius)는 제곱 공간에서 직접 못 하고, (r1+r2)^2 처럼 반경 쪽을 합쳐서 제곱해야 한다. (3) 실제 거리 값이 필요한 소비자(UI 거리 표시 등)와 판정 전용 경로를 분리하는 게 정석.

### Q. 부채꼴(콘) 시야 내부 판정을 내적으로 하는 원리를 설명하라. 왜 acos으로 각도를 구하지 않고 cos(반각)을 저장해 비교하는가?

**정의/수식**: 정규화된 전방 벡터 F와 정규화된 타겟 방향 D에 대해 F·D = cos(theta), theta는 두 벡터 사이 각. 타겟이 반각 halfAngle 안에 있으려면 theta <= halfAngle, 즉 cos(theta) >= cos(halfAngle) (cos은 [0, pi]에서 단조감소라 부등호가 뒤집힌다). 그래서 판정은 dot >= halfAngleCos 한 줄이 된다.

**직관**: 내적은 "타겟 방향을 전방 축에 투영한 길이"다. 정면일수록 1, 옆일수록 0, 뒤면 음수. 각도라는 비선형 값을 굳이 복원하지 않고, 투영값끼리 비교하는 것.

**Winters 적용**: `VisionSystem.cpp:74`의 `IsInsideVisionConeXZ`가 정확히 이 패턴이고, acos을 아예 호출하지 않도록 컴포넌트에 각도 대신 코사인을 저장하는 규약을 쓴다(`VisionConeComponent.halfAngleCos`). 같은 수학이 `AnnieGameSim.cpp:417`의 부채꼴 스킬 히트 판정 `CollectConeTargets`에도 재사용된다 — 시야 판정과 데미지 판정이 동일한 "정규화 내적 vs 저장된 코사인" 패턴을 공유한다.

**함정/꼬리질문**: (1) acos/atan2는 sqrt보다도 비싸고, acos은 입력이 부동소수점 오차로 1.0을 살짝 넘으면 NaN을 뱉는다 — 코사인 비교는 이 클램프 문제 자체가 없다. (2) 반각 규약 혼동: cos(30°)=0.866을 저장하면 "총 60° 콘"이다. 전체각을 넣는 실수가 흔하다. (3) 두 벡터가 정규화 안 됐으면 dot은 cos이 아니라 |A||B|cos(theta)라 비교가 깨진다 — 정규화 전제를 반드시 밝혀야 한다.

### Q. 두 벡터의 정규화를 sqrt 한 번으로 합치는 트릭 dot / sqrt(lenSqA \* lenSqB)를 유도해 보라.

**정의/수식**: cos(theta) = (A·B) / (|A||B|) = (A·B) / sqrt(|A|^2 \* |B|^2). 분모의 두 길이를 각각 sqrt하면 sqrt 2회 + 곱셈이지만, 제곱길이끼리 먼저 곱한 뒤 sqrt를 한 번만 취하면 수학적으로 동일하다 (sqrt(a)\*sqrt(b) = sqrt(a\*b), a,b >= 0).

**직관**: 각 벡터를 단위벡터로 만드는 대신, "정규화 계수"를 하나로 합쳐 마지막에 나눠준다. 벡터 자체는 정규화된 적이 없지만 결과 스칼라는 정규화된 내적과 같다.

**Winters 적용**: `IsInsideVisionConeXZ`(VisionSystem.cpp:74)가 `dot = (dx*fwd.x + dz*fwd.z) / sqrt(dirLenSq * fwdLenSq)`로 구현했다. 퇴화 케이스 — forward 길이 0이거나 타겟이 소스 위치와 겹치는 경우(길이 0) — 는 0으로 나누기 전에 걸러서 true로 통과시키는 관용적 정책을 택했다.

**함정/꼬리질문**: (1) 퇴화 케이스를 true로 할지 false로 할지는 도메인 결정이다 — 시야에선 "내 발밑은 보인다"가 자연스럽지만, 스킬 히트에선 자기 자신 타격 버그가 되므로 Annie 콘 판정은 distSq <= 0.0001을 제외한다. 같은 수학, 다른 정책. (2) lenSq 곱이 아주 작을 때 underflow 가능성 — epsilon 하한 체크가 먼저다. (3) 나눗셈조차 아끼고 싶으면 dot >= halfAngleCos \* sqrt(lenSqProduct)로 이항해 비교할 수 있으나, dot이 음수일 때 부호 처리가 필요해 가독성과 트레이드오프.

### Q. halfAngleCos 기본값이 0.8660254인 이유는? 이런 상수를 서버와 클라이언트가 어떻게 일치시켜야 하는가?

**정의/수식**: 0.8660254 = cos(30°) = sqrt(3)/2. 반각 30°이므로 시야콘 전체각은 60°다.

**직관**: 컴포넌트에 "60도"라는 사람 친화 값 대신 판정에 바로 쓰이는 코사인을 저장하면, 매 판정마다의 cos 변환이 데이터 정의 시점 1회로 옮겨진다. 대신 상수만 보면 각도가 즉시 안 읽히므로 주석/네이밍으로 의미를 고정해야 한다.

**Winters 적용**: `VisionComponents.h:18`의 `VisionConeComponent{ forward, halfAngleCos = 0.8660254f }`가 기본 60° 콘을 정의하고, 클라 `SnapshotApplier.cpp:1032`에서 Kalista 센티널 콘을 재생성할 때 동일 상수를 사용해 서버/클라 규약을 맞춘다. 시야 파라미터 자체는 `KalistaGameSim.cpp`에서 데이터 정의의 Resolve\*Param으로 주입된다.

**함정/꼬리질문**: 서버와 클라가 같은 리터럴을 두 곳에 하드코딩하면 드리프트 위험이 있다 — 이상적으로는 Shared 헤더의 단일 상수나 데이터 테이블로 빼야 하고, "지금은 두 곳이 같은 값이지만 단일 소스로 옮기는 게 다음 개선"이라고 스스로 지적할 수 있으면 좋다. 또 부동소수점 리터럴 자릿수 차이(0.866 vs 0.8660254)로 경계 케이스가 서버/클라에서 다르게 갈리면 "서버는 보인다는데 클라는 안 그리는" 유령 디싱크가 난다.

### Q. 팀 가시성을 비트마스크 하나로 표현하는 설계를 설명하라. 매 틱 clear 후 OR 누적하는 이유는?

**정의/수식**: `VisibilityComponent.teamVisibilityMask`(u8)의 비트 i가 "팀 i에게 보임"을 의미. 판정 통과 시 `mask |= (1u << sourceTeam)`. 조회는 `mask & (1u << myTeam)`.

**직관**: 가시성은 "타겟 1개 : 팀 N개"의 다대일 관계라, 팀별 bool 배열 대신 정수 하나의 비트로 압축하면 저장/조회/네트워크 직렬화가 전부 싸진다. 매 틱 0으로 클리어하고 다시 누적하는 건 가시성이 "지속 상태"가 아니라 "매 틱 재도출되는 파생값"이기 때문 — 클리어를 빼먹으면 한 번 보인 유닛이 영원히 보이는 스테일 비트 버그가 난다.

**Winters 적용**: `VisionSystem.cpp:201`에서 매 틱 마스크를 0으로 클리어 → 자기 팀 비트를 먼저 셋(자기 팀에는 항상 보임) → 시야 판정 통과마다 소스 팀 비트 OR(라인 285). 소비자는 셋으로 갈라진다: `RenderVisibilityFilter.h:74`(렌더 컬링), `TurretAISystem.cpp:353`(포탑이 FOW 밖 적을 타겟에서 제외), `MinimapPanel.cpp:450`(미니맵 아이콘). 구조물처럼 항상 보이는 것은 스폰 시 `BuildServerVisibleToAll`(GameRoomSpawn.cpp:91)로 양팀 비트를 미리 셋한다.

**함정/꼬리질문**: (1) u8이라 8팀 한계 — 팀 수가 늘면 타입만 넓히면 되도록 비트 연산을 헬퍼로 감쌌는지가 설계 질문. (2) "자기 팀 항상 가시"를 판정 루프에 섞지 않고 클리어 직후 무조건 셋하는 순서가 중요하다 — 시야 소스가 하나도 없는 팀도 자기 유닛은 봐야 한다. (3) AI(포탑)가 가시성 마스크를 소비한다는 건 "AI도 FOW 규칙을 따른다"는 공정성 설계다 — 치트 없는 AI를 어떻게 보장하냐는 꼬리질문의 답이 된다.

### Q. 부시(은폐) 안의 유닛이 보이는 조건을 어떻게 모델링했나? LoL 부시 규칙의 핵심은?

**정의/수식**: 타겟이 은폐 상태(bInConcealment)면, 기본 원/콘 판정을 통과했더라도 (1) 소스가 같은 concealmentId의 볼륨 안에 있거나 (2) 소스가 트루사이트(bTrueSight)일 때만 가시. 즉 visible = baseVisible AND (NOT concealed OR sameVolume OR trueSight).

**직관**: 부시는 "거리와 무관한 추가 필터"다. 원/콘이 기하 조건이라면 은폐는 집합 멤버십 조건 — 같은 부시라는 집합에 둘 다 속해야 서로 보인다. 트루사이트는 이 필터를 무시하는 특권 플래그.

**Winters 적용**: `VisionSystem.cpp:357`이 same-volume-or-true-sight 규칙을 구현한다. 볼륨 점유는 `UpdateConcealmentOccupancy`가 매 틱 `CConcealmentVolumeIndex::QueryVolumeAt`(원 내부 판정 dx\*dx+dz\*dz <= r\*r, 선형 스캔)으로 갱신하고, 볼륨 출입이 감지되면 m_bForceRebuild로 10Hz 스로틀을 무시하고 즉시 재계산을 트리거한다 — 부시 출입은 게임플레이상 즉각 반응해야 하는 이벤트이기 때문이다.

**함정/꼬리질문**: (1) 같은 볼륨 "id" 비교라는 점 — 인접한 두 부시는 서로 다른 id라 붙어 있어도 안 보인다. LoL 원작도 같은 규칙. (2) 점유 판정도 원 판정이므로 은폐 볼륨 경계에서 시야 갱신 주기(10Hz)와 맞물려 1프레임 늦게 사라지는 현상이 가능하다 — 강제 리빌드가 이 지연을 줄이는 장치. (3) 볼륨 인덱스가 선형 스캔인데 부시 수가 수십 개 수준이라 허용된다는 규모 감각을 말할 수 있어야 한다.

### Q. 시야 후보를 좁히는 브로드페이즈로 균일 그리드 공간 해시를 쓸 때, 반경 질의와 원-원 확장 반경(r1+r2)의 의미를 설명하라.

**정의/수식**: 셀 크기 cellSize인 그리드에서 반경 r 질의는 cellRadius = ceil(r / cellSize) + 1 범위의 셀들을 순회한다. 각 후보에 대해 두 원(질의 반경 r, 엔트리 반경 entry.radius)의 교차는 중심 거리 <= r + entry.radius, 제곱 형태로 distSq <= (r + entry.radius)^2.

**직관**: "반경 r1 원과 반경 r2 원이 겹친다" = "한쪽 반경을 r1+r2로 부풀리고 다른 쪽을 점으로 취급"하는 민코프스키 합 관점이다. 브로드페이즈(그리드 셀)는 거짓 양성을 허용하는 싼 필터, 내로우페이즈(제곱거리)가 정확한 판정 — 두 단계를 분리해야 O(전체 엔티티) 스캔을 피한다.

**Winters 적용**: `SpatialIndex.cpp:60`의 `QueryRadius`가 cellRadius 셀 범위 순회 → kindMask/excludeTeamMask 비트 필터 → expandedRadius = radius + entry.radius 제곱거리 판정 순으로 동작한다. `VisionSystem.TickVisibility`는 소스마다 이 질의로 후보를 좁히는데, 자기 팀은 어차피 항상 보이므로 excludeTeamMask로 아예 후보에서 제외하는 것이 마스크 필터의 실전 활용이다.

**함정/꼬리질문**: (1) +1 여유 셀은 엔트리 자체 반경 때문에 이웃 셀에 걸친 물체를 놓치지 않기 위한 보수적 확장이다 — entry.radius의 최대값이 cellSize를 넘으면 +1로 부족해지므로, 셀 크기와 최대 엔트리 반경의 관계를 규약으로 못 박아야 한다. (2) 셀 크기 선택: 너무 작으면 순회 셀 수 폭발, 너무 크면 셀당 후보 폭발 — 평균 질의 반경과 비슷한 자릿수가 경험칙. (3) 팀/종류 필터를 제곱거리 계산보다 먼저 두는 순서(싼 비교 먼저)도 짚을 것.

### Q. 미니언 웨이브처럼 시야 소스가 뭉쳐 다닐 때의 중복 계산을 어떻게 줄였나?

**정의/수식**: 소스 (team, cellX, cellZ)를 64비트 키 cellKey = (team << 40) | ((cellX & 0xFFFFF) << 20) | (cellZ & 0xFFFFF) 로 팩킹하고, 같은 키가 이미 처리됐으면 해당 소스의 시야 계산을 스킵한다.

**직관**: 같은 팀 미니언 6기가 같은 그리드 셀에 서 있으면 시야 원 6개가 거의 완전히 겹친다 — 가시성 결과에 기여하는 건 사실상 1개다. "결과가 같으면 계산도 1번"이라는 dedup으로, 시야 비용을 소스 수가 아니라 점유 셀 수에 비례하게 만든다.

**Winters 적용**: `VisionSystem.cpp:219`에서 eSpatialKind::Unit(미니언)에만 이 dedup을 적용하고, m_vecUnitVisionCells에 키를 모아 std::find로 중복을 거른다. 챔피언에는 적용하지 않는다 — 챔피언은 수가 적고 개별 시야 파라미터(콘, 트루사이트)가 다를 수 있어 dedup의 이득 대비 정확도 손실이 크기 때문.

**함정/꼬리질문**: (1) 근사 최적화다 — 셀 하나 차이로 서 있는 미니언 둘은 dedup이 안 되고, 같은 셀이지만 셀 경계 반대편에 있으면 시야 원이 반 셀 어긋난다. 셀 크기 대비 시야 반경이 충분히 크면 오차가 눈에 안 띈다는 전제를 말해야 한다. (2) std::find 선형 탐색은 셀 수가 적을 때만 정당 — 수백 개면 정렬+이분 탐색이나 해시셋으로 교체 시점. (3) 비트 팩킹에서 음수 셀 좌표의 & 0xFFFFF 마스킹은 2의 보수 하위 비트를 취하는 것이라 좌표 범위가 2^20을 넘으면 충돌한다 — 맵 크기 상한이 암묵 전제라는 걸 아는지 묻기 좋은 지점.

### Q. 포그 오브 워를 Unexplored/Explored/Visible 3-상태로 두는 이유와, 단일 채널 텍스처에 인코딩하는 방법은?

**정의/수식**: 256x256 u8(R8) 텍스처에 Unexplored=0, Explored=127, Visible=255. 매 갱신 시 127 초과 값을 127로 강등(현재 시야 → 탐험됨 붕괴)한 뒤, 이번 틱의 시야 원들만 255로 다시 그린다. Explored는 단조 증가(한 번 밝히면 0으로 안 돌아감), Visible은 매 틱 파생.

**직관**: 3-상태는 게임 정보 설계다 — "가본 적 없는 곳"(지형도 모름), "가봤지만 지금은 안 보이는 곳"(지형은 기억, 유닛은 안 보임), "지금 보이는 곳". 값 강등 방식은 별도 explored 비트맵 없이 한 채널로 두 지속성(영구/순간)을 동시에 표현하는 트릭이다: 255→127 강등이 "시야가 지나가면 탐험 흔적이 남는" 동작을 공짜로 만든다.

**Winters 적용**: `VisionSystem.cpp:367`이 이 인코딩과 강등-후-재도장 사이클을 구현하고, 텍스처는 `FogOfWarRenderer.cpp:192`에서 R8_UNORM으로 올라간다. 겹치는 시야원은 dst = max(dst, value)로 합성해 밝은 값이 이긴다 — 시야의 합집합이 max 연산이 되는 이유다.

**함정/꼬리질문**: (1) max 합성이 곧 팀 시야 합집합이다 — 덧셈이 아니라 max인 이유(두 시야가 겹쳐도 "더 잘 보이는" 건 아님)를 물을 수 있다. (2) 127/255 중간값들은 가장자리 페더링용이라, 강등 시 "127 초과만 127로"라는 조건이 페더 링의 어두운 쪽(127 미만)을 건드리지 않는 것이 중요하다. (3) 256x256 저해상도가 허용되는 건 GPU에서 보간+smoothstep으로 복원하기 때문 — 다음 질문으로 이어진다.

### Q. smoothstep의 수식과 성질을 설명하고, 시야 가장자리 페더링에 쓰는 이유를 말하라.

**정의/수식**: t를 [0,1]로 클램프한 뒤 s = 3t^2 - 2t^3 = t\*t\*(3 - 2t). s(0)=0, s(1)=1이고 s'(0)=s'(1)=0 — 양 끝에서 1차 도함수가 0이라 경계에서 C1 연속으로 붙는다.

**직관**: 선형 보간(lerp)은 경계에서 기울기가 갑자기 꺾여 "마하 밴드"처럼 가장자리 선이 눈에 띈다. smoothstep은 진입/이탈이 부드러워 시야 원 가장자리가 자연스러운 그라데이션으로 사라진다. 에르미트 보간의 최저차 다항식이라 셰이더/CPU 어디서든 곱셈 몇 번으로 끝난다.

**Winters 적용**: CPU 쪽은 `VisionSystem.cpp:367` 부근에서 시야원 가장자리 feather = clamp(r\*0.18, 2~7텍셀) 링에 t = t\*t\*(3-2t) 수동 smoothstep으로 127~255 그라데이션을 굽는다. GPU 쪽은 `FogOfWarWorld.hlsl:34`에서 R8 값 하나로 explored = smoothstep(0.02, 0.55, fog), visible = smoothstep(threshold, 1, fog) 두 마스크를 만들어 이중 smoothstep + 이중 lerp로 3-상태 안개를 복원한다.

**함정/꼬리질문**: (1) C1이지 C2는 아니다 — 2차 도함수는 경계에서 불연속이고, 더 매끈한 게 필요하면 smootherstep(6t^5-15t^4+10t^3, C2). (2) feather를 반경 비례(r\*0.18)로 하되 텍셀 단위로 클램프하는 이유: 작은 시야는 페더가 시야를 다 먹지 않게, 큰 시야는 페더가 흐리멍텅해지지 않게. (3) CPU에서 굽는 값과 GPU smoothstep 구간(0.02~0.55, threshold~1)이 127/255 인코딩과 맞물려 있어, 인코딩 값을 바꾸면 셰이더 상수도 같이 바꿔야 하는 암묵 결합이 있다.

### Q. 월드 좌표를 FOW 텍스처 UV로 변환하는 아핀 역변환을 어떻게 구현했나? 행렬식 검사는 왜 필요한가?

**정의/수식**: UV (0,0), (1,0), (0,1)에 대응하는 월드 3점으로 아핀 기저를 정의하면, 월드 오프셋 W = u\*U + v\*V (U, V는 기저 벡터). 이를 u, v에 대해 풀면 2x2 선형계이고 크래머 공식으로 det = ux\*vz - uz\*vx, u = (wx\*vz - wz\*vx)/det, v = (ux\*wz - uz\*wx)/det.

**직관**: FOW 텍스처를 맵 위에 "붙이는" 정변환(UV→월드)의 역을 매 텍셀/매 유닛 위치에서 풀어야 하는데, 2D 아핀이라 역행렬이 닫힌 형태로 나온다. det는 기저 평행사변형의 부호 있는 넓이 — 0에 가까우면 두 기저 벡터가 거의 평행해 역변환이 수치적으로 폭발한다.

**Winters 적용**: `VisionSystem.cpp:52`의 `FowProjection`/`WorldToFowUv`가 크래머 공식 역변환을 구현하고 |det| <= 0.0001 특이 기저를 거부한다. `FowProjection::IsValid`도 동일 det 검사를 해서, 잘못된 투영 데이터가 들어오면 시야를 굽기 전에 걸러낸다. 정변환 `FowTexelToWorld`는 (texel + 0.5)/dim 텍셀 중심 샘플링을 쓴다.

**함정/꼬리질문**: (1) epsilon을 절대값 0.0001로 고정하면 맵 스케일에 따라 과민/둔감해질 수 있다 — 기저 길이로 정규화한 상대 판정이 더 강건하다는 걸 아는지. (2) 3점 정의는 회전+비균등 스케일+시어까지 표현하는 완전한 아핀이다 — "왜 그냥 min/max AABB가 아니라 3점인가"라는 질문에 "맵이 회전돼 붙을 수 있어서"라고 답할 수 있어야 한다. (3) det의 부호는 기저의 방향(뒤집힘)을 알려준다 — 절대값만 검사하면 미러된 투영도 통과하는데, FOW에선 무해하지만 노멀 계산 같은 데선 버그가 된다.

### Q. 시야 원 하나를 FOW 텍스처에 래스터라이즈하는 과정을 설명하라. 텍셀 중심 +0.5 샘플링은 왜 필요한가?

**정의/수식**: texelRadius = r / cellWorld로 반경을 텍셀 단위로 환산 → floor/ceil ± 2 여유의 min/max 바운딩 박스를 텍스처 범위로 클램프 → 박스 내 각 텍셀 중심 (x+0.5, z+0.5)를 월드로 되투영 → distSq > r^2 원 판정(및 콘 소스면 IsInsideVisionConeXZ)을 통과한 텍셀만 밝힘.

**직관**: 원을 전체 텍스처 O(N^2)로 스캔하지 않고 O(반경^2) 박스만 도는 고전적 바운딩 박스 래스터다. +0.5는 "텍셀은 격자점이 아니라 넓이를 가진 칸"이라는 규약 — 칸의 대표점을 중심으로 잡아야 정/역변환이 왕복했을 때 반 텍셀씩 밀리는 오프바이원이 안 생긴다.

**Winters 적용**: `VisionSystem.cpp:415`가 이 구조 그대로다. ±2 텍셀 여유는 페더 링과 부동소수점 반올림이 박스 경계에서 잘리는 것을 막는 보수적 마진이고, 콘 시야 소스는 원 판정 뒤 동일 텍셀에 콘 판정을 한 번 더 얹는다.

**함정/꼬리질문**: (1) +0.5를 정변환에만 쓰고 역변환에서 빼먹으면(또는 반대) 시야 원이 반 텍셀 편향된다 — 렌더링의 하프 텍셀 오프셋 문제와 같은 뿌리(D3D9 시절의 악명 높은 이슈). (2) 박스 클램프를 빼먹으면 맵 가장자리 시야에서 버퍼 오버런. (3) "더 빠르게 하려면?"에는 스팬 단위(행마다 원의 x구간을 해석적으로 계산)나 SIMD, 혹은 아예 GPU 컴퓨트로 옮기는 선택지를 트레이드오프와 함께 말하면 된다.

### Q. 시야 시스템을 10Hz로 스로틀링한 근거는? 어떤 경우에 즉시 재계산해야 하는가?

**정의/수식**: 누적 dt가 TICK_INTERVAL = 0.1s를 채울 때까지 Execute를 스킵. 단 특정 이벤트 발생 시 m_bForceRebuild 플래그로 다음 프레임 즉시 재계산.

**직관**: 가시성은 프레임 정확도가 필요 없는 파생 상태다 — 유닛이 0.1초에 이동하는 거리는 시야 반경 대비 미미해서, 60Hz로 갱신해도 사용자가 차이를 못 느낀다. 비용(소스×후보×텍셀 래스터)을 1/6로 줄이는 대신 최대 100ms 지연을 받아들이는 비용-정확도 트레이드오프.

**Winters 적용**: `VisionSystem.h:103`에 TICK_INTERVAL = 0.1f. 다만 은폐 볼륨 출입, FOW 투영 변경, 로컬 팀 변경은 m_bForceRebuild로 스로틀을 관통한다 — 부시에 들어갔는데 0.1초간 계속 보이는 건 게임플레이 버그이기 때문에, "정기 갱신 + 이벤트 기반 강제 리빌드"의 하이브리드가 핵심이다.

**함정/꼬리질문**: (1) 스로틀된 시스템을 소비하는 쪽(포탑 AI, 렌더 필터)이 최대 100ms 스테일 데이터를 봐도 되는지 소비자별로 검토해야 한다 — 포탑 타겟팅은 자체 틱이 있어 허용, 즉발 판정(스킬 히트)은 시야 마스크가 아니라 직접 기하 판정을 쓰므로 무관. (2) 모든 소스의 갱신 위상이 같으면 0.1초마다 스파이크가 생긴다 — 소스를 프레임별로 나눠 갱신하는 시간 슬라이싱이 다음 단계 최적화. (3) 강제 리빌드 조건을 빼먹으면 "부시 숨었는데 잠깐 보임" 같은 리포트 재현이 어려운 타이밍 버그가 된다.

### Q. 시야/FOW 판정은 서버와 클라이언트 중 어디서 해야 하는가? 맵핵 방지 관점에서 설명하라.

**정의/수식**: 원칙 — 정보 은닉은 정보를 아예 보내지 않는 것으로만 보장된다. 클라이언트가 데이터를 받고 나서 "그리지만 않는" 필터링은 메모리를 읽는 치트(맵핵)에 무력하다.

**직관**: 클라 렌더 필터는 연출 계층이고, 보안 경계는 스냅샷 직렬화 시점이다. 서버가 팀별 가시성 마스크를 권위적으로 계산하고, 그 팀에 안 보이는 엔티티는 스냅샷에서 제외(또는 지연/퍼지 위치)해야 진짜 FOW다.

**Winters 적용**: 가시성 계산의 권위는 서버 쪽에 있고(스폰 시 `BuildServerVisibleToAll`로 구조물 양팀 비트를 서버가 셋, GameRoomSpawn.cpp:91), 클라는 `RenderVisibilityFilter.h:74`로 렌더 컬링, `MinimapPanel.cpp:450`으로 미니맵 필터를 수행한다. 즉 클라 필터는 존재하되 그것이 보안 장치가 아니라 표현 장치라는 역할 구분이 설계에 깔려 있다. 클라 `SnapshotApplier`가 서버 규약(halfAngleCos 등)을 재현해 시야 소스를 로컬 재구성하는 것도 "서버가 규칙의 원본"이라는 방향성의 일부다.

**함정/꼬리질문**: (1) 서버측 스냅샷 필터링(interest management)은 대역폭 절감과 보안을 동시에 준다 — 다만 시야 경계에서 엔티티가 나타났다 사라졌다 하는 팝핑을 보간/유예 시간으로 감춰야 한다. (2) "적이 부시에서 나오는 순간"은 클라가 그 엔티티의 과거 상태를 모르므로, 등장 프레임의 스냅 위치/애니 처리 규약이 필요하다. (3) 사운드/투사체/체력바 같은 부수 채널로 정보가 새는 것도 서버에서 걸러야 한다 — 필터 대상은 "렌더 메시"가 아니라 "정보" 전체다.

### Q. Kalista W 센티널처럼 '움직이면서 앞만 보는' 시야 소스는 어떻게 만들었나? 정적 시야와 무엇이 다른가?

**정의/수식**: start~end 패트롤 구간에서 phaseSec = fmod(elapsed, cycle)로 위상을 구해 앞/뒤 반 구간에서 t를 반전시키는 핑퐁 왕복. 진행 방향 forward로 yaw = atan2(forward.x, forward.z)를 갱신하고, 같은 forward를 VisionConeComponent.forward에 매 틱 써서 시야콘이 이동 방향을 따라 회전하게 한다.

**직관**: 시야콘은 (위치, forward, halfAngleCos)의 순수 데이터라, "움직이는 시야"는 별도 시스템이 아니라 그 데이터를 매 틱 갱신하는 것으로 충분하다. 시각적 회전(yaw)과 게임플레이 시야(forward)를 같은 근원 벡터에서 파생시켜 "보이는 방향 = 실제로 보는 방향"을 보장하는 것이 포인트.

**Winters 적용**: `KalistaGameSim.cpp:320`이 이 패턴이고, sightRange/halfAngleCos 같은 시야 파라미터는 코드 하드코딩이 아니라 데이터 정의에서 Resolve\*Param으로 주입된다. 클라는 `SnapshotApplier.cpp:1032`에서 동일 상수로 콘을 재구성해 서버 시뮬과 클라 표현의 규약을 일치시킨다.

**함정/꼬리질문**: (1) fmod 위상 기반 왕복은 상태 없는(stateless) 구현이라 스냅샷 복원/리와인드에 강하다 — "속도 적분" 방식이면 서버/클라 누적 오차가 갈라진다. (2) atan2(x, z) 인자 순서는 엔진의 yaw 규약(어느 축이 전방인가)에 묶여 있다 — Winters는 챔피언 body forward 축이 에셋별로 달라 yaw 보정을 별도 헬퍼로 관리하는 코드베이스라, 시야 forward(순수 수학)와 렌더 yaw(에셋 보정 포함)를 섞으면 안 된다는 점을 언급하면 좋다. (3) 10Hz 시야 스로틀과 매 틱 forward 갱신이 만나면, 콘이 실제보다 최대 0.1초 뒤처진 방향을 밝힌다 — 빠른 패트롤 속도에서 눈에 띌 수 있는 한계.

### Q. 이 시야 모델(원+콘+은폐 볼륨)의 한계는 무엇인가? 벽/지형에 의한 시선 차단(LOS)을 추가한다면 어떻게 하겠는가?

**정의/수식**: 현재 모델은 가시성 = 반경 판정 AND (콘 판정) AND (은폐 규칙)으로, 소스와 타겟 사이의 장애물을 전혀 보지 않는다. LOS는 선분-장애물 교차 문제: 소스→타겟 선분이 차단 지오메트리와 교차하면 불가시. 대표 기법은 (a) 물리/내브그리드 레이캐스트(선분 위 셀을 DDA/브레즌햄으로 걸으며 차단 셀 검사), (b) 타일 기반 recursive shadowcasting(소스 중심 8분면을 훑으며 차단 타일 뒤 그림자 각도 구간을 전파), (c) 높이 필드 비교(LoL식 — 낮은 지형에서 높은 지형 위는 안 보임).

**직관**: LoL 원작 시야의 정체성은 벽 뒤/수풀/고지대 은폐인데, 원+콘 모델은 그중 "거리"와 "방향"만 다룬다. FOW 텍스처를 이미 굽고 있으므로, LOS는 시야원 래스터 단계에서 "각 텍셀이 소스에서 보이는가"를 추가 판정하는 형태로 자연스럽게 확장된다.

**Winters 적용(정직한 답변 포인트)**: Winters의 VisionSystem은 의도적으로 원+콘+은폐 볼륨만 구현했고 occlusion 레이캐스트/shadowcasting은 미구현이다 — 톱다운 맵에서 은폐 볼륨(부시)이 게임플레이상 가장 중요한 차단을 커버하고, 벽 차단은 비용 대비 우선순위가 낮다고 판단했다고 말하는 것이 좋다. 확장한다면 이미 있는 자산을 재사용한다: 시야원 래스터(VisionSystem.cpp:415)의 텍셀 루프에 "소스→텍셀 브레즌햄 워크로 내브그리드 차단 셀 검사"를 끼우거나, 텍셀 수가 부담이면 recursive shadowcasting으로 O(시야 반경 텍셀)에 그림자를 한 번에 전파한다. 유닛 가시성(IsTargetVisibleFast)은 소스→타겟 1회 레이캐스트만 추가하면 된다.

**함정/꼬리질문**: (1) 텍셀마다 레이캐스트하면 O(r^3)이 된다 — shadowcasting이 O(r^2)로 우월한 이유를 설명할 수 있어야 한다. (2) 높이 기반이면 "고지대 유닛은 저지대를 본다"의 비대칭 규칙이 생겨 가시성이 대칭이 아니게 된다 — 팀 마스크 누적 로직은 이미 방향성이 있어(소스→타겟) 구조 변경은 불필요. (3) LOS 결과도 10Hz 스로틀/강제 리빌드 체계에 그대로 태울 수 있는지, 벽은 정적이라 차단 맵을 프리컴퓨트할 수 있는지가 실무 최적화 논점.

### Q. 시야 파이프라인에서 CPU와 GPU의 역할을 어떻게 나눴나? 그 경계는 왜 거기인가?

**정의/수식**: CPU — 게임플레이에 영향을 주는 이산 판정: 가시성 마스크(u8 비트), FOW 텍스처의 3-상태 값(0/127/255)과 페더 링 굽기. GPU — 순수 표현: R8 텍스처 하나를 샘플해 explored/visible 두 smoothstep 마스크를 만들고 alpha = lerp(lerp(unexploredAlpha, exploredAlpha, explored), 0, visible), 색 = lerp(unexploredColor, exploredColor, explored)로 화면 합성.

**직관**: 경계 기준은 "게임 로직이 그 값을 읽는가"다. 포탑 AI와 미니맵이 읽는 가시성/FOW 상태는 CPU(그리고 궁극적으로 서버)에 있어야 결정론과 권위가 지켜지고, 색/알파/부드러운 경계는 프레임마다 픽셀 수만큼 계산해야 하므로 GPU가 압도적으로 싸다. 256x256 저해상도 CPU 텍스처를 GPU 보간+smoothstep이 시각적으로 확대 복원하는 분업이다.

**Winters 적용**: CPU 측은 `VisionSystem.cpp:367`(3-상태 인코딩+수동 smoothstep 페더), GPU 측은 `FogOfWarWorld.hlsl:34`(이중 smoothstep+이중 lerp, UV 범위 밖은 무조건 미탐험 색)이고, 텍스처는 R8_UNORM(FogOfWarRenderer.cpp:192)으로 채널 1개 1바이트/텍셀 = 64KB라 매 갱신 업로드가 부담 없다. 미니맵 오버레이는 같은 데이터를 value >= 250 → α0 / >= 127 → α112 / 그외 α214로 양자화(FogOfWarRenderer.cpp:290)해 소비한다 — 한 소스, 두 표현.

**함정/꼬리질문**: (1) "전부 GPU 컴퓨트로 옮기면?" — 시야 굽기는 옮길 수 있지만 결과를 AI가 읽으려면 리드백 지연(1~2프레임)이 생기고, 서버는 GPU가 없다 — 서버 권위 게임에선 CPU 시야가 원본이어야 한다는 답이 정석. (2) UV 밖 처리 규약(무조건 미탐험)을 셰이더에 넣지 않으면 텍스처 랩/클램프 모드에 따라 맵 밖이 밝게 새는 버그. (3) CPU 인코딩 상수(127/255)와 셰이더 smoothstep 구간(0.02~0.55 등)의 암묵 결합은 한쪽만 바꾸면 조용히 깨진다 — 공유 상수화가 개선 방향.

---

