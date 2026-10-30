# PiEngine

PiEngine은 현대적인 C++20으로 만드는 고성능 CPU 기반 원주율 계산기입니다.

목표는 단순히 작은 자릿수의 π를 빠르게 출력하는 것이 아닙니다. 수십억 자리
이상의 장시간 계산을 멀티코어에서 수행하면서, 중단 후 재시작하고 RAM 한계를
넘겨 계산하며, 마지막 결과를 독립적으로 검증할 수 있는 계산 엔진을 지향합니다.

프로젝트의 설계 방향을 한 문장으로 요약하면 다음과 같습니다.

> **Checkpointable Parallel Chudnovsky Binary Splitting**

> [!IMPORTANT]
> `pi-engine`은 병렬 Chudnovsky 계산, out-of-core 저장, interval-gated
> checkpoint 생성, 검증된 block 기반 resume, 진행 보고, 최종 결과 기록과
> 검증을 하나의 실행 경로로 연결합니다. 손상되거나 manifest에 없는 block은
> 계산에 사용하지 않고 격리한 뒤 해당 범위를 다시 계산합니다.

## 목표

- Chudnovsky 공식으로 수십억 자리 이상의 π 계산
- Binary Splitting과 work stealing을 통한 CPU 멀티코어 활용
- GMP에 큰 정수 연산과 FFT multiplication 위임
- P/Q/T 블록 체크포인트를 이용한 중단 후 재시작
- OS swap에 의존하지 않는 전용 out-of-core 저장 계층
- CLI뿐 아니라 JSON, GUI, metrics로 확장 가능한 진행 상태 모델
- checksum, 독립 modular P/Q/T 검증, known digits, hash, BBP 검증
- 정확성 → 안정성 → 유지보수성 → 성능 순서의 개발 원칙

## 전체 계산 방향

```mermaid
flowchart TD
    A["목표 자릿수와 설정"] --> B["Chudnovsky 항 수 결정"]
    B --> C["P/Q/T leaf 범위 생성"]
    C --> D["Cutoff 기반 병렬 Binary Splitting"]
    D --> E["Thread Pool · Local Queue · Work Stealing"]
    E --> F["GMP 큰 정수 P/Q/T merge"]
    F --> G["버전된 체크포인트 블록"]
    G --> H["최종 π 재구성과 10진수 출력"]
    H --> I["Hash · BBP · Known Digits 검증"]
```

계산 worker는 P/Q/T 연산만 담당합니다. 체크포인트 I/O와 진행 상태 출력은
별도 경로로 분리하여 느린 디스크나 reporter가 계산 worker를 막지 않게 만드는
것이 목표입니다.

## 핵심 알고리즘

### 1. Chudnovsky 공식

PiEngine의 π 급수는 Chudnovsky 공식을 사용합니다.

```math
\frac{1}{\pi}
=
12\sum_{k=0}^{\infty}
\frac{(-1)^k(6k)!(13591409+545140134k)}
{(3k)!(k!)^3(640320)^{3k+\frac{3}{2}}}
```

한 항은 약 14.18자리의 정밀도를 추가합니다. 목표가 $D$자리라면 필요한 항의
수는 대략 다음과 같습니다.

```math
N=\left\lceil\frac{D}{14.1816474627}\right\rceil
```

직접 앞에서부터 항을 더하지 않고 Binary Splitting으로 평가하여 큰 정수의
불필요한 성장을 줄이고 독립적인 하위 범위를 병렬 작업으로 만듭니다.

### 2. Binary Splitting과 P/Q/T

범위 $[a,b)$의 결과는 세 개의 큰 정수 $P(a,b)$, $Q(a,b)$, $T(a,b)$로
표현합니다. 중간점 $m$에서 왼쪽과 오른쪽 결과는 다음처럼 병합됩니다.

```math
P(a,b)=P(a,m)P(m,b)
```

```math
Q(a,b)=Q(a,m)Q(m,b)
```

```math
T(a,b)=T(a,m)Q(m,b)+P(a,m)T(m,b)
```

현재 P/Q/T 자료구조, Chudnovsky leaf, `[start, end)` 범위 검증, cutoff
기반 staged parallel DAG와 parallel merge, integer fixed-point 최종 계산과
decimal formatting이 구현되어 있습니다.

분할 트리의 깊이는 $O(\log N)$입니다. $M(n)$을 $n$비트 정수 곱셈 비용이라고
할 때 전체 급수 계산 비용은 큰 정수 곱셈에 지배되며, Binary Splitting 기반
평가는 일반적으로 $O(M(n)\log^2 n)$ 범위의 계산을 목표로 합니다. 실제 성능과
메모리 피크는 cutoff, GMP 알고리즘 선택, 동시에 유지하는 subtree 수에 따라
달라지므로 benchmark로 결정합니다.

### 3. GMP 큰 정수 연산

PiEngine은 자체 Big Integer나 FFT/NTT multiplication을 다시 구현하지
않습니다. `GMPInteger`가 GMP의 `mpz_t`를 소유하고 P/Q/T의 덧셈, 뺄셈,
곱셈을 `mpz_*` 연산에 위임합니다.

GMP는 피연산자 크기에 따라 기본 곱셈, Karatsuba, Toom-Cook, FFT 계열
알고리즘을 선택합니다. PiEngine의 멀티코어 전략은 먼저 Binary Splitting의
독립 subtree를 병렬 실행하는 것입니다. 하나의 거대한 `mpz_mul`이 항상 모든
코어를 사용한다고 가정하지 않으며, 상위 merge 단계의 병렬성 한계는 실제
benchmark 이후 최적화합니다.

### 4. Thread Pool과 Work Stealing

현재 scheduler 기반은 구현과 동시성 검증이 완료되어 있습니다.

- 고정된 worker thread pool
- 외부 제출용 bounded MPMC `LockFreeQueue`
- worker가 생성한 child 작업용 local `WorkStealingQueue`
- idle worker가 다른 worker의 local 작업을 가져오는 work stealing
- `TaskHandle` 기반 wait, 완료·실패 확인, 예외 전달
- `Stopped → Running → Stopping → Stopped` 수명주기
- stop 이전에 승인된 모든 작업을 끝내는 drain shutdown
- stop 시작 이후 신규 제출 거부

알고리즘 코드가 직접 thread를 만들지 않고 scheduler에 작업을 제출하는 구조를
유지합니다. 실제 steal 테스트는 root worker를 차단한 상태에서 local child가
복수의 다른 worker thread에서 실행되는 것까지 검증합니다.

### 5. 체크포인트와 무결성

대규모 계산은 재귀 호출 스택이 아니라 독립적으로 병합 가능한 P/Q/T 범위를
체크포인트로 저장합니다.

각 블록은 다음 정보를 포함합니다.

- 파일 magic과 format version
- 계산 identity와 목표 자릿수
- Chudnovsky 범위 `[a, b)`와 tree level
- P/Q/T payload 길이와 값
- checksum 종류와 checksum 값
- 완료 상태와 manifest 연결 정보

블록은 임시 파일에 기록하고 flush·동기화한 다음 atomic rename으로 완료합니다.
재시작 시에는 구조, 범위, checksum, manifest 일관성뿐 아니라 별도 modular
P/Q/T residue까지 통과한 블록만 재사용합니다. 손상된 블록은 격리하고 해당
범위를 다시 계산합니다.

### 6. Out-of-Core와 진행 상태

RAM이 부족할 때 OS swap에 의존하지 않고 명시적인 out-of-core merge와 저장
정책을 사용합니다. 진행 상태는 계산 로직이 문자열을 직접 출력하지 않고,
thread-safe tracker가 immutable snapshot을 제공하는 구조로 확장합니다.

snapshot에는 phase, 목표 자릿수, 완료 항 수와 블록 수, merge level,
active/queued task, 처리 속도, ETA, 메모리, storage chunk와 I/O, checkpoint
telemetry가 포함됩니다. CLI text와 JSON reporter는 같은 snapshot을 소비합니다.

Checkpoint telemetry는 서로 다른 네 가지 개념을 구분합니다.

- **완료 작업**(`checkpoints=C/T`, JSON `checkpoint_blocks.completed/total`):
  검증을 통과해 재사용한 resume 블록과 새로 계산한 초기 frontier 블록의 수입니다.
  단조 증가하며, 고정된 total(재사용 블록 + 재계산 예정 leaf 블록)을 넘지 않습니다.
- **Durable 블록**(`durable=D`, JSON `checkpoint_blocks.durable`): 현재
  manifest의 entry 수입니다. 자식 블록이 병합된 부모로 교체되면 **감소할 수
  있습니다**. `checkpoint_bytes`는 현재 manifest가 참조하는 블록 파일 크기의
  정확한 합이며 manifest 자체와 참조되지 않은 파일은 제외합니다.
- **검증된 블록**(`last_validated=`, JSON `last_validated_checkpoint`): resume
  시 구조·checksum·manifest·modular P/Q/T 검증을 통과한 입력에만 설정됩니다.
- **커밋된 블록**(`last_committed=`, JSON `last_committed_checkpoint`): 새로
  기록된 마지막 블록으로, 이를 참조하는 manifest가 atomic하게 게시된 뒤에만
  갱신됩니다. 새로 기록된 블록은 검증된 블록으로 표시하지 않습니다.

Checkpoint telemetry는 manifest 교체가 성공한 뒤 coordinating thread에서 한
번에 게시됩니다. 실패했거나 interval 때문에 생략됐거나 내용이 같은 commit은
telemetry를 바꾸지 않습니다. JSON progress `schema_version`은 4입니다.

## 현재 구현 상태

| 영역 | 상태 | 현재 범위 |
| --- | --- | --- |
| 설정과 CLI | 완료 | TOML/default/CLI override, 병렬 계산, 결과 기록과 검증 |
| Platform | 기반 완료 | CPUID와 AVX/AVX2/AVX-512 기능 탐지 |
| Memory | 기반 완료 | Arena, pool, alignment, scratch buffer |
| Scheduler | 완료 | lifecycle, drain, MPMC queue, local routing, work stealing |
| Big Integer | 기반 완료 | GMP `mpz_t` RAII wrapper와 기본 산술 |
| Binary Splitting | 완료 | P/Q/T node, 순차 계산, staged parallel merge |
| Chudnovsky leaf | 완료 | leaf 공식, 범위 검증, known P/Q/T와 pi 앞자리 검증 |
| 병렬 Binary Splitting | 완료 | 명시적 cutoff, bounded leaf block, 단계별 merge, fallback |
| Chudnovsky finalization | 완료 | precision plan, integer sqrt/division, rounded decimal API |
| Storage·out-of-core | 완료 | 동기·비동기 spill/reload, concurrent I/O, LZ4 |
| Checkpoint·무결성 | 완료 | interval-gated merge frontier, atomic manifest, verified resume, quarantine, 자동 CLI 중단·resume 회귀 테스트 |
| Progress reporting | 완료 | immutable snapshot, CLI text/JSON schema v4, live checkpoint telemetry, resume 복구 |
| 최종 결과 검증 | 완료 | streaming hash, BBP spot check, known digits, manifest |

세부 구현 순서는 [Implementation Plan](docs/IMPLEMENTATION_PLAN.md), 완료 상태는
[Roadmap](docs/ROADMAP.md)과 [Checklist](docs/CHECKLIST.md), 설계 근거는
[Architecture Decisions](docs/DECISIONS.md)에서 확인할 수 있습니다.

## 하지 않는 것

- GMP와 중복되는 자체 Big Integer·FFT multiplication 구현
- 알고리즘 내부의 임시 thread 생성 또는 detached thread
- OS swap을 대규모 계산의 저장 전략으로 사용
- checksum만 통과한 checkpoint를 수학적으로 옳다고 간주
- benchmark 없이 cutoff, NUMA, SIMD, Huge Page 최적화 확정
- 아직 연결되지 않은 기능을 완료된 π 계산 기능으로 표시

## 빌드

### 요구 사항

- CMake 3.20 이상
- C++20 컴파일러(GCC 또는 Clang)
- GMP 개발 패키지
- LZ4 개발 패키지(`liblz4`; Debian/Ubuntu `liblz4-dev`, Arch `lz4`)
- POSIX thread 지원 환경
- Git

`toml++`는 `third_party/tomlplusplus`에 포함되어 있습니다.

프로젝트 루트에서 실행합니다.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

빌드가 완료되면 다음 실행 파일이 생성됩니다.

```text
build/pi-engine
```

실행 파일은 π를 계산해 결과 파일에 기록하고, 기본 설정에서는 결과 검증과
verification manifest 저장까지 수행합니다. `--threads 0`은 사용 가능한 hardware
thread 수를 자동 선택합니다.

```bash
./build/pi-engine --digits 1000000 --threads 0
```

기본 출력은 `pi.txt`, verification manifest는 `pi.txt.verify`입니다. 출력 경로를
바꾸면 manifest 기본 경로도 `<output>.verify`가 됩니다.

```bash
./build/pi-engine \
  --digits 1000000 \
  --output /data/pi/pi.txt \
  --verification-manifest /data/pi/pi.verify \
  --bbp-samples 8
```

`--no-verify`로 최종 검증을 끌 수 있습니다. `--bbp-samples`는 1~32이며 기본값은
8입니다.

### 설정 우선순위

실행 시 현재 작업 디렉터리의 `config.toml`을 읽습니다. 파일이 없으면 내장
기본값을 사용하고, CLI 옵션은 TOML 값을 덮어씁니다.

```bash
cp config.toml.example config.toml
./build/pi-engine --digits 1000000
```

> [!WARNING]
> 현재 알 수 없는 CLI 옵션은 오류 없이 무시됩니다. 예를 들어 `--digit` 오타는
> `--digits`로 처리되지 않고 기본 자릿수로 실행될 수 있으므로 실행 전 옵션명을
> 확인해야 합니다.

### Checkpoint와 resume

기본값은 checkpoint와 resume 활성화, `checkpoints`와
`checkpoint-quarantine` 디렉터리, 300초 interval입니다. 같은 계산 identity로
다시 실행하면 검증을 통과한 durable block만 자동 재사용하고, 손상되거나
manifest에 없는 block은 quarantine한 뒤 해당 범위를 다시 계산합니다.

```bash
./build/pi-engine --checkpoint --resume \
  --checkpoint-directory checkpoints \
  --checkpoint-quarantine-directory checkpoint-quarantine \
  --checkpoint-interval 300
```

기존 checkpoint를 사용하지 않고 새 계산을 시작하거나 checkpoint 자체를
비활성화할 수 있습니다.

```bash
./build/pi-engine --checkpoint --no-resume --digits 1000000
./build/pi-engine --no-checkpoint --no-resume --digits 1000000
```

### Progress 출력

진행 상태는 기본으로 500 ms마다 text 형식으로 **stderr**에 출력됩니다. JSON
형식은 한 줄당 하나의 schema version 4 record인 JSONL입니다.

```bash
./build/pi-engine --progress --progress-interval 500 --progress-format json \
  2>progress.jsonl
./build/pi-engine --no-progress
```

### Out-of-core 저장

Out-of-core 저장은 기본 활성화되어 `storage/`를 사용합니다. 기본 memory budget은
512 MiB, 목표 chunk 크기는 64 MiB, 최대 동시 I/O는 1, compression은 LZ4입니다.

```bash
./build/pi-engine \
  --storage-directory /data/pi/storage \
  --storage-memory-budget-bytes 536870912 \
  --storage-chunk-size-bytes 67108864 \
  --storage-max-concurrent-io 1
```

Chunk 크기는 memory budget 이하여야 합니다. `out_of_core` 활성화와
`compression = "none" | "lz4"` 선택은 현재 `config.toml`에서만 바꿀 수
있습니다.

## 테스트

프로젝트 루트에서 전체 테스트를 실행합니다.

```bash
ctest --test-dir build --output-on-failure
```

테스트에는 실제 `pi-engine`을 첫 manifest 게시 뒤 중단하고 같은 identity로
resume한 다음, 중단 없이 계산한 결과와 decimal 출력 및 verification hash가
byte 단위로 같은지 비교하는 Linux 전용 CLI 회귀 테스트가 포함됩니다.

Sanitizer 빌드는 다음처럼 구성할 수 있습니다.

```bash
cmake -S . -B build-sanitize \
  -DENABLE_ASAN=ON \
  -DENABLE_UBSAN=ON
cmake --build build-sanitize
ASAN_OPTIONS=detect_leaks=0 \
  ctest --test-dir build-sanitize --output-on-failure
```

## 문서

- [Implementation Plan](docs/IMPLEMENTATION_PLAN.md)
- [Architecture Decisions](docs/DECISIONS.md)
- [Roadmap](docs/ROADMAP.md)
- [Checklist](docs/CHECKLIST.md)
- [Changelog](docs/CHANGELOG.md)
- [Workflow](docs/WORKFLOW.md)
